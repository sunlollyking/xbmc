/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <map>
#include <mutex>
#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \brief Remembers how far through each manual the player got
 *
 * Reopening a manual on its cover having read half of it is a small thing that
 * is noticed every time, so the page is kept per document and restored.
 *
 * Positions live in userdata rather than a database, because there are few of
 * them and losing them costs nothing.
 */
class CManualPosition
{
public:
  static CManualPosition& GetInstance();

  /*!
   * \brief The page last read in a manual
   *
   * \param path The manual
   *
   * \return The page, counting from zero, or 0 for a manual not seen before
   */
  unsigned int GetPage(const std::string& path);

  /*!
   * \brief Record the page being read
   *
   * Page zero is recorded as "no position" and removes any entry, so that a
   * manual read back to its cover stops taking up a slot.
   *
   * \param path The manual
   * \param page The page, counting from zero
   */
  void SetPage(const std::string& path, unsigned int page);

private:
  CManualPosition() = default;
  ~CManualPosition() = default;

  //! Reads the file the first time a position is asked for. Must be called
  //! with the lock held.
  void Load();

  //! Must be called with the lock held
  void Save();

  //! The file the positions live in
  static std::string GetPath();

  std::mutex m_mutex;
  bool m_loaded{false};

  //! Manual path to page number
  std::map<std::string, unsigned int> m_positions;
};

} // namespace GAME
} // namespace KODI
