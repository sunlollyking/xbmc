/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLibraryTypes.h"

#include <cstdint>
#include <string>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief Work out what identifies a game file to a catalogue
 *
 * Cartridge-sized files are hashed whole. Archives are hashed on their
 * largest member, since that is the ROM and the catalogues hash the ROM,
 * not the archive. Disc images are never hashed whole: the serial their
 * boot sector or system file carries is read instead, the way RetroArch and
 * the catalogues themselves identify discs.
 *
 * Every read is bounded. A file that cannot be read is left unidentified,
 * never a reason to stop a scan.
 */
class CGameFileIdentity
{
public:
  /*!
   * \brief Files larger than this are treated as disc images and only their
   *        header is read
   */
  static constexpr uint64_t MAX_HASH_SIZE = 64 * 1024 * 1024;

  /*!
   * \brief How much of a disc image is searched for its serial
   */
  static constexpr uint64_t DISC_HEADER_SIZE = 4 * 1024 * 1024;

  /*!
   * \brief Fill in what can be learnt about a file
   *
   * \param path The game file, which may be an archive or a cue sheet
   * \param[out] file Size and, where obtainable, crc32, md5, serial
   * \param media The platform's media format, which decides whether a
   *        serial is worth looking for
   *
   * \return True if at least one identity was found
   */
  static bool Identify(const std::string& path, GameFile& file, MediaFormat media);

  /*!
   * \brief The serial a disc image's header carries, or empty
   *
   * Reads PlayStation (SYSTEM.CNF), Saturn, Mega-CD and Dreamcast headers.
   * The result is normalised to upper-case letters and digits, as the
   * catalogues key on.
   */
  static std::string ReadDiscSerial(const std::string& trackPath);

  /*!
   * \brief The first data track named by a cue sheet or GDI file, or empty
   */
  static std::string FirstTrack(const std::string& sheetPath);

  /*!
   * \brief A serial as the catalogues key on it
   */
  static std::string NormaliseSerial(std::string serial);

private:
  static bool HashWhole(const std::string& path, GameFile& file);
  static bool HashArchive(const std::string& path, GameFile& file);
};
} // namespace GAME
} // namespace KODI
