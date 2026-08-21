/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PdfDocument.h"

#include "ServiceBroker.h"
#include "filesystem/File.h"
#include "guilib/Texture.h"
#include "rendering/RenderSystem.h"
#include "utils/log.h"

#include <algorithm>
#include <mutex>

#include <poppler-document.h>
#include <poppler-global.h>
#include <poppler-image.h>
#include <poppler-page-renderer.h>
#include <poppler-page.h>

using namespace KODI::GAME;

namespace
{
//! A manual is a handful of megabytes. Anything this size is not a manual, and
//! reading it would hurt more than refusing it.
constexpr int64_t MAX_DOCUMENT_SIZE = 256 * 1024 * 1024;

//! PDF sizes are in points, 72 to the inch
constexpr double POINTS_PER_INCH = 72.0;

void LogPopplerError(const std::string& message, void* /*closure*/)
{
  CLog::Log(LOGDEBUG, "CPdfDocument: {}", message);
}
} // namespace

CPdfDocument::CPdfDocument(std::vector<char> buffer, std::unique_ptr<poppler::document> document)
  : m_buffer(std::move(buffer)),
    m_document(std::move(document))
{
  const int pages = m_document->pages();
  m_pageCount = pages > 0 ? static_cast<unsigned int>(pages) : 0;
}

CPdfDocument::~CPdfDocument() = default;

void CPdfDocument::InstallErrorHandler()
{
  static std::once_flag installed;
  std::call_once(installed, []() { poppler::set_debug_error_function(LogPopplerError, nullptr); });
}

std::unique_ptr<CPdfDocument> CPdfDocument::Open(const std::string& path)
{
  InstallErrorHandler();

  XFILE::CFile file;
  if (!file.Open(path))
  {
    CLog::Log(LOGERROR, "CPdfDocument: failed to open \"{}\"", path);
    return {};
  }

  const int64_t length = file.GetLength();
  if (length <= 0 || length > MAX_DOCUMENT_SIZE)
  {
    CLog::Log(LOGERROR, "CPdfDocument: refusing \"{}\", size {} bytes", path, length);
    return {};
  }

  std::vector<char> buffer(static_cast<size_t>(length));
  const ssize_t read = file.Read(buffer.data(), buffer.size());
  if (read < 0 || static_cast<size_t>(read) != buffer.size())
  {
    CLog::Log(LOGERROR, "CPdfDocument: read {} of {} bytes from \"{}\"", read, buffer.size(), path);
    return {};
  }

  // load_from_raw_data() parses the buffer in place instead of copying it, so
  // the buffer is moved into the document object below and kept alive there
  std::unique_ptr<poppler::document> document(
      poppler::document::load_from_raw_data(buffer.data(), static_cast<int>(buffer.size())));

  if (!document)
  {
    CLog::Log(LOGERROR, "CPdfDocument: \"{}\" is not a readable PDF", path);
    return {};
  }

  if (document->is_locked())
  {
    // There is nowhere to ask for a password, so an encrypted manual is simply
    // one that cannot be shown
    CLog::Log(LOGERROR, "CPdfDocument: \"{}\" is password protected", path);
    return {};
  }

  if (document->pages() <= 0)
  {
    CLog::Log(LOGERROR, "CPdfDocument: \"{}\" has no pages", path);
    return {};
  }

  std::unique_ptr<CPdfDocument> pdf(new CPdfDocument(std::move(buffer), std::move(document)));

  CLog::Log(LOGDEBUG, "CPdfDocument: opened \"{}\", {} page(s)", path, pdf->GetPageCount());

  return pdf;
}

std::unique_ptr<CTexture> CPdfDocument::RenderPage(unsigned int pageIndex,
                                                   unsigned int maxWidth,
                                                   unsigned int maxHeight) const
{
  if (pageIndex >= m_pageCount || maxWidth == 0 || maxHeight == 0)
    return {};

  // A manual scanned as double page spreads is far wider than it is tall, so
  // zooming one can ask for a texture wider than the GPU will accept - 4096 is
  // still a common limit. Clamping here costs some detail at high zoom, which
  // is better than a page that silently fails to upload.
  const unsigned int maxTextureSize = CServiceBroker::GetRenderSystem()->GetMaxTextureSize();
  if (maxTextureSize > 0)
  {
    maxWidth = std::min(maxWidth, maxTextureSize);
    maxHeight = std::min(maxHeight, maxTextureSize);
  }

  std::unique_ptr<poppler::page> page(m_document->create_page(static_cast<int>(pageIndex)));
  if (!page)
  {
    CLog::Log(LOGERROR, "CPdfDocument: failed to read page {}", pageIndex);
    return {};
  }

  const poppler::rectf pageRect = page->page_rect();
  if (pageRect.width() <= 0.0 || pageRect.height() <= 0.0)
  {
    CLog::Log(LOGERROR, "CPdfDocument: page {} has no size", pageIndex);
    return {};
  }

  // Scale to whichever bound is reached first, so the whole page fits
  const double scale = std::min(maxWidth / pageRect.width(), maxHeight / pageRect.height());
  const double dpi = scale * POINTS_PER_INCH;

  poppler::page_renderer renderer;
  renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
  renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
  // Pages are drawn onto white. Without this a manual scanned without a
  // background renders as dark text on transparency, which reads as black on
  // black once it reaches the screen.
  renderer.set_paper_color(0xffffffff);

  const poppler::image image = renderer.render_page(page.get(), dpi, dpi);
  if (!image.is_valid() || image.format() != poppler::image::format_argb32)
  {
    CLog::Log(LOGERROR, "CPdfDocument: page {} did not render", pageIndex);
    return {};
  }

  std::unique_ptr<CTexture> texture = CTexture::CreateTexture();
  if (!texture)
    return {};

  // Poppler's format_argb32 is a native endian 0xAARRGGBB word, which on a
  // little endian machine is the byte order B,G,R,A that XB_FMT_A8R8G8B8
  // means. The pixels go straight to the texture with no conversion.
  texture->LoadFromMemory(static_cast<unsigned int>(image.width()),
                          static_cast<unsigned int>(image.height()),
                          static_cast<unsigned int>(image.bytes_per_row()), XB_FMT_A8R8G8B8,
                          false, // pages are drawn opaque onto the paper colour
                          reinterpret_cast<const unsigned char*>(image.const_data()));

  return texture;
}
