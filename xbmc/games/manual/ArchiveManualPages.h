/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ManualPages.h"

#include <memory>
#include <string>
#include <vector>

namespace KODI
{
namespace GAME
{

/*!
 * \brief A manual that is a comic archive
 *
 * A .cbz or .cbr is a zip or rar holding one image per page, which is how
 * scanned manuals are usually distributed. Nothing has to be rendered: the
 * pages are handed to the skin as ordinary image paths inside the archive, and
 * Kodi's existing image loading does the rest.
 *
 * Pages are ordered by filename, which is the convention the format relies on.
 */
class CArchiveManualPages : public IManualPages
{
public:
  /*!
   * \brief Open a comic archive and read its page list
   *
   * \param path The archive
   *
   * \return The pages, or empty if the archive holds no images or could not
   *         be read - a rar needs an archive add-on that may not be installed
   */
  static std::unique_ptr<CArchiveManualPages> Open(const std::string& path);

  ~CArchiveManualPages() override = default;

  // Implementation of IManualPages
  unsigned int GetPageCount() const override;
  std::string GetPageImage(unsigned int page, unsigned int height) const override;

private:
  explicit CArchiveManualPages(std::vector<std::string> pages);

  std::vector<std::string> m_pages;
};

} // namespace GAME
} // namespace KODI
