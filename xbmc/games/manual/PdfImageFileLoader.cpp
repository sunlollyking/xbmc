/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PdfImageFileLoader.h"

#include "PdfDocument.h"
#include "PdfDocumentCache.h"
#include "guilib/Texture.h"
#include "imagefiles/ImageFileURL.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <charconv>

using namespace KODI::GAME;

namespace
{
//! Tall enough to fill a 1080p screen with a little to spare, so that a page
//! is not visibly resampled when the viewer is showing it whole
constexpr unsigned int DEFAULT_HEIGHT = 1200;

//! A page has to stay within something sane whatever the caller asks for. The
//! texture is also clamped to the GPU's limit when it is rendered.
constexpr unsigned int MAX_HEIGHT = 8192;

//! Pages that are double page spreads are much wider than they are tall, so
//! the width bound is generous relative to the height
constexpr unsigned int MAX_WIDTH = 8192;

/*!
 * \brief Read an unsigned option, falling back if it is missing or malformed
 *
 * The URL can be built by a skin, so a value that isn't a number is a mistake
 * to absorb rather than a reason to fail to show the page.
 */
unsigned int GetUnsignedOption(const IMAGE_FILES::CImageFileURL& imageFile,
                               const std::string& key,
                               unsigned int fallback)
{
  const std::string value = imageFile.GetOption(key);
  if (value.empty())
    return fallback;

  unsigned int result = 0;
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto [parsed, error] = std::from_chars(begin, end, result);
  if (error != std::errc{} || parsed != end)
    return fallback;

  return result;
}
} // namespace

bool CPdfImageFileLoader::CanLoad(const std::string& specialType) const
{
  return specialType == "pdf";
}

std::unique_ptr<CTexture> CPdfImageFileLoader::Load(
    const IMAGE_FILES::CImageFileURL& imageFile) const
{
  const std::string& path = imageFile.GetTargetFile();
  if (path.empty())
    return {};

  const unsigned int page = GetUnsignedOption(imageFile, "page", 0);

  unsigned int height = GetUnsignedOption(imageFile, "height", DEFAULT_HEIGHT);
  height = std::clamp(height, 1u, MAX_HEIGHT);

  return CPdfDocumentCache::GetInstance().WithDocument(
      path, [page, height](const CPdfDocument& document) -> std::unique_ptr<CTexture>
      { return document.RenderPage(page, MAX_WIDTH, height); });
}
