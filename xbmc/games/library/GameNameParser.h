/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLibraryTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief What a ROM's file name says about it
 */
struct ParsedGameName
{
  std::string title; // as written, tags removed: "Legend of Zelda, The"
  std::string displayTitle; // article restored: "The Legend of Zelda"
  std::vector<std::string> regions; // full names, e.g. "USA", "Europe"
  std::vector<std::string> languages; // lower-case ISO 639-1
  std::string revision;
  ReleaseStatus status{ReleaseStatus::RETAIL};
  Licence licence{Licence::LICENSED};
  bool alternate{false};
  bool bad{false};
  bool verified{false};
  bool hack{false};
  std::string translation; // language code of a fan translation, or empty
  int disc{0};
  int discs{0};
  int year{0};
  std::string publisher; // TOSEC names the publisher in the file name
  std::vector<std::string> unknownTags;
};

/*!
 * \ingroup games
 *
 * \brief Read the conventions ROM sets name their files by
 *
 * No-Intro and Redump write "Title (Region) (Languages) (Rev 1) (Beta)",
 * TOSEC writes "Title (1985)(Publisher)[cr Group]", and GoodTools writes
 * "Title [!]" and "Title [h1]". All three are read; everything recognised
 * becomes a field and what is left is the title. Nothing is guessed: a tag
 * that is not understood is kept as it is, so a later version can read it.
 */
class CGameNameParser
{
public:
  /*!
   * \brief Parse a file or folder name
   *
   * \param fileName The base name, with or without an extension
   */
  static ParsedGameName Parse(std::string_view fileName);

  /*!
   * \brief "Legend of Zelda, The" -> "The Legend of Zelda"
   */
  static std::string DisplayTitle(std::string_view title);

  /*!
   * \brief The region a file name's tag names, written in full, or empty
   *
   * "USA", "Europe", "Japan", "World" and the rest, as No-Intro writes them,
   * whatever spelling or abbreviation the tag used.
   */
  static std::string RegionName(std::string_view region);
};
} // namespace GAME
} // namespace KODI
