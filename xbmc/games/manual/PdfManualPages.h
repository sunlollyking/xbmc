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

namespace KODI
{
namespace GAME
{

/*!
 * \brief A manual that is a PDF
 *
 * Pages are handed to the skin as `image://pdf@` URLs, so they are rendered by
 * the image loader on a background thread and cached like any other image.
 */
class CPdfManualPages : public IManualPages
{
public:
  /*!
   * \brief Open a PDF and read how many pages it has
   *
   * \return The pages, or empty if the document could not be read
   */
  static std::unique_ptr<CPdfManualPages> Open(const std::string& path);

  ~CPdfManualPages() override = default;

  // Implementation of IManualPages
  unsigned int GetPageCount() const override;
  std::string GetPageImage(unsigned int page, unsigned int height) const override;

private:
  CPdfManualPages(std::string path, unsigned int pageCount);

  std::string m_path;
  unsigned int m_pageCount{0};
};

} // namespace GAME
} // namespace KODI
