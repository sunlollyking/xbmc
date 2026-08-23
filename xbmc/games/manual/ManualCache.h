/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \brief Keeps downloaded manuals within a size the player chose
 *
 * A manual runs from a few hundred kilobytes to well over a hundred megabytes,
 * and a large collection would otherwise fill a disk one download at a time.
 *
 * Only manuals Kodi fetched are tracked, and only tracked manuals are ever
 * deleted. A manual the player put beside their games themselves is never in
 * here, so it can never be evicted - which matters, because these files live
 * in the player's own games folders rather than somewhere Kodi owns.
 *
 * Eviction is by least recently opened rather than oldest first. The whole
 * point of keeping a manual is being able to refer back to it, so the one
 * reached for every session should outlive one downloaded later and never
 * read again.
 */
class CManualCache
{
public:
  static CManualCache& GetInstance();

  /*!
   * \brief Record a manual Kodi downloaded
   *
   * \param path Where it was written
   * \param bytes Its size
   */
  void Add(const std::string& path, uint64_t bytes);

  /*!
   * \brief Note that a manual was opened, so that it survives longer
   *
   * Does nothing for a manual that is not tracked, which is how a manual the
   * player supplied stays out of the accounting entirely.
   */
  void Touch(const std::string& path);

  /*!
   * \brief The total size of the tracked manuals
   */
  uint64_t GetSize();

  /*!
   * \brief Delete tracked manuals, least recently opened first, until the
   *        total is within the configured budget
   *
   * \param keep A manual to spare regardless - the one just downloaded, or
   *        the one being read, neither of which should vanish underneath the
   *        player
   */
  void Enforce(const std::string& keep = "");

  /*!
   * \brief Delete every tracked manual
   */
  void Clear();

private:
  CManualCache() = default;
  ~CManualCache() = default;

  struct CacheEntry
  {
    uint64_t bytes{0};

    //! Seconds since the epoch, so that ordering survives a restart
    int64_t lastOpened{0};
  };

  //! Reads the file on first use. Must be called with the lock held.
  void Load();

  //! Must be called with the lock held
  void Save();

  //! Drops entries whose file has gone - deleted by hand, or on a share that
  //! is not mounted. Must be called with the lock held.
  void Forget(const std::string& path);

  static std::string GetPath();

  //! The budget in bytes, from the player's setting
  static uint64_t GetBudget();

  std::mutex m_mutex;
  bool m_loaded{false};

  std::map<std::string, CacheEntry> m_entries;
};

} // namespace GAME
} // namespace KODI
