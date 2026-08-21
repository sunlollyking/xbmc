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
#include <vector>

class CTexture;

namespace poppler
{
class document;
}

namespace KODI
{
namespace GAME
{

/*!
 * \brief A PDF opened for reading, with its pages rendered on demand
 *
 * Used to display game manuals. Only what is needed to put a page on screen is
 * exposed: there is no text extraction, no editing and no form handling.
 *
 * The whole file is read into memory when the document is opened, because
 * Poppler wants random access and a manual arriving over SMB would otherwise
 * seek across the network for every page. Manuals are a few megabytes, so this
 * is a better trade than it would be for video.
 */
class CPdfDocument
{
public:
  /*!
   * \brief Open a PDF
   *
   * \param path The path to the document, in any form Kodi's VFS can read
   *
   * \return The document, or empty if it could not be opened. A document that
   *         needs a password is treated as unopenable, because there is nowhere
   *         to ask for one.
   */
  static std::unique_ptr<CPdfDocument> Open(const std::string& path);

  ~CPdfDocument();

  /*!
   * \brief The number of pages, always at least 1 for an open document
   */
  unsigned int GetPageCount() const { return m_pageCount; }

  /*!
   * \brief Render a page into a texture
   *
   * The page keeps its aspect ratio and is scaled to be as large as it can be
   * within the given bounds.
   *
   * \param pageIndex The page to render, counting from zero
   * \param maxWidth The width not to exceed, in pixels
   * \param maxHeight The height not to exceed, in pixels
   *
   * \return The rendered page, or empty if the page could not be rendered
   */
  std::unique_ptr<CTexture> RenderPage(unsigned int pageIndex,
                                       unsigned int maxWidth,
                                       unsigned int maxHeight) const;

private:
  CPdfDocument(std::vector<char> buffer, std::unique_ptr<poppler::document> document);

  /*!
   * \brief Send Poppler's parse errors to Kodi's log
   *
   * Poppler writes to stderr otherwise, which on most of Kodi's platforms goes
   * nowhere. A manual that renders badly is usually a complaint about the file,
   * so the complaint is worth keeping.
   */
  static void InstallErrorHandler();

  //! The file contents. Poppler reads this buffer in place rather than copying
  //! it, so it has to outlive the document below - declaration order matters.
  std::vector<char> m_buffer;

  std::unique_ptr<poppler::document> m_document;

  unsigned int m_pageCount{0};
};

} // namespace GAME
} // namespace KODI
