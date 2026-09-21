/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PdfManualPages.h"

#include "PdfDocumentCache.h"
#include "imagefiles/ImageFileURL.h"

#include <string>

using namespace KODI::GAME;

CPdfManualPages::CPdfManualPages(std::string path, unsigned int pageCount)
  : m_path(std::move(path)),
    m_pageCount(pageCount)
{
}

std::unique_ptr<CPdfManualPages> CPdfManualPages::Open(const std::string& path)
{
  // Opens and parses the document, which is why this belongs off the GUI thread
  const unsigned int pageCount = CPdfDocumentCache::GetInstance().GetPageCount(path);
  if (pageCount == 0)
    return {};

  return std::unique_ptr<CPdfManualPages>(new CPdfManualPages(path, pageCount));
}

unsigned int CPdfManualPages::GetPageCount() const
{
  return m_pageCount;
}

std::string CPdfManualPages::GetPageImage(unsigned int page, unsigned int height) const
{
  if (page >= m_pageCount)
    return {};

  IMAGE_FILES::CImageFileURL imageURL = IMAGE_FILES::CImageFileURL::FromFile(m_path, "pdf");
  imageURL.AddOption("page", std::to_string(page));
  imageURL.AddOption("height", std::to_string(height));

  return imageURL.ToString();
}
