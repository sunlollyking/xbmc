/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <memory>
#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \brief The pages of an opened manual
 *
 * A manual is either a PDF, whose pages have to be rendered, or a comic
 * archive, whose pages are already images. Both end up as an image URL the
 * skin can display, so the viewer does not need to know which it has.
 */
class IManualPages
{
public:
  virtual ~IManualPages() = default;

  /*!
   * \brief The number of pages, at least one for an opened manual
   */
  virtual unsigned int GetPageCount() const = 0;

  /*!
   * \brief The image to display for a page
   *
   * \param page The page, counting from zero
   * \param height The height to render at, for a format that is rendered.
   *        Formats that are already images ignore it and let Kodi scale.
   *
   * \return An image URL, or empty if there is no such page
   */
  virtual std::string GetPageImage(unsigned int page, unsigned int height) const = 0;
};

/*!
 * \brief Open a manual
 *
 * Reads and parses the whole document, so this belongs on a background thread.
 *
 * \param path The manual, as returned by CGameManual
 *
 * \return The manual's pages, or empty if it could not be read
 */
std::unique_ptr<IManualPages> OpenManualPages(const std::string& path);

/*!
 * \brief Whether a file extension is one the viewer can open
 *
 * \param extension The extension, including the leading dot, in any case
 */
bool IsManualExtension(const std::string& extension);

} // namespace GAME
} // namespace KODI
