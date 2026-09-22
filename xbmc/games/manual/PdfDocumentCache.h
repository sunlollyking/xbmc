/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>

namespace KODI
{
namespace GAME
{
class CPdfDocument;

/*!
 * \brief Keeps the manual being read open
 *
 * Every page turn asks the image loader for a new image, and reopening a
 * document means reading and reparsing the whole file each time. A manual is
 * read one at a time, so holding the most recent one is enough to make paging
 * cost only the rendering.
 *
 * The image loader runs on background threads, so access is serialised.
 */
class CPdfDocumentCache
{
public:
  static CPdfDocumentCache& GetInstance();

  /*!
   * \brief The number of pages in a document, opening it if needed
   *
   * \return The page count, or 0 if the document could not be opened
   */
  unsigned int GetPageCount(const std::string& path);

  /*!
   * \brief Run something with the open document
   *
   * The document is held under the cache's lock for the duration, which keeps
   * it alive while a page renders.
   *
   * \param path The document to open
   * \param function Called with the open document
   *
   * \return What the function returned, or empty if the document is unopenable
   */
  template<typename TFunction>
  auto WithDocument(const std::string& path, TFunction&& function)
      -> decltype(function(std::declval<const CPdfDocument&>()))
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    const CPdfDocument* document = GetOrOpen(path);
    if (document == nullptr)
      return {};

    return function(*document);
  }

  /*!
   * \brief Drop the held document, freeing the file it was read from
   *
   * Called when the viewer closes; a manual is several megabytes and there is
   * no reason to hold it once it is off screen.
   */
  void Clear();

private:
  CPdfDocumentCache();
  ~CPdfDocumentCache();

  //! Returns the open document for the path, opening it if it isn't the one
  //! already held. Must be called with the lock held.
  const CPdfDocument* GetOrOpen(const std::string& path);

  std::mutex m_mutex;
  std::string m_path;
  std::unique_ptr<CPdfDocument> m_document;
};

} // namespace GAME
} // namespace KODI
