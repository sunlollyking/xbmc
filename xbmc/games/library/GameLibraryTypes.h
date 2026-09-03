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
#include <string>
#include <string_view>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \brief Settings the library reads
 */
constexpr const char* SETTING_GAMELIBRARY_SHOWDERIVEDGAMES = "gamelibrary.showderivedgames";
constexpr const char* SETTING_GAMELIBRARY_REGIONPRIORITY = "gamelibrary.regionpriority";
constexpr const char* SETTING_GAMELIBRARY_REGIONPRIORITY_DEFAULT = "eu,wor,us,jp";
constexpr const char* SETTING_GAMELIBRARY_PREFERRETAIL = "gamelibrary.preferretail";
constexpr const char* SETTING_GAMELIBRARY_PREFERNEWESTREVISION = "gamelibrary.prefernewestrevision";
constexpr const char* SETTING_GAMELIBRARY_PREFERVERIFIED = "gamelibrary.preferverified";

/*!
 * \ingroup games
 * \brief What kind of machine a platform is
 */
enum class PlatformType
{
  UNKNOWN,
  CONSOLE,
  HANDHELD,
  COMPUTER,
  ARCADE,
  PINBALL,
  VIRTUAL,
  OTHER,
};

/*!
 * \ingroup games
 * \brief What a platform's games are distributed on
 */
enum class MediaFormat
{
  UNKNOWN,
  CARTRIDGE,
  DISC,
  TAPE,
  DISK,
  CARD,
  DOWNLOAD,
  OTHER,
};

/*!
 * \ingroup games
 * \brief What kind of work a game is
 *
 * Fan translations are releases of the original game, so they are not a
 * category. Hacks and homebrew are their own games, linked to the game they
 * derive from where there is one.
 */
enum class GameCategory
{
  RETAIL,
  HACK,
  HOMEBREW,
  DEMO,
  BIOS,
  APPLICATION,
};

/*!
 * \ingroup games
 * \brief How finished a release was when it was dumped
 */
enum class ReleaseStatus
{
  RETAIL,
  BETA,
  PROTOTYPE,
  SAMPLE,
  DEMO,
  ALPHA,
  KIOSK,
  DEBUG,
  PROGRAM,
};

/*!
 * \ingroup games
 * \brief Under whose authority a release was published
 */
enum class Licence
{
  LICENSED,
  UNLICENSED,
  AFTERMARKET,
  HOMEBREW,
  PIRATE,
};

/*!
 * \ingroup games
 * \brief How trustworthy a dump is
 */
enum class DumpStatus
{
  UNKNOWN,
  GOOD,
  BAD,
  VERIFIED,
};

/*!
 * \ingroup games
 * \brief How a file was identified as a game
 *
 * Kept with the game so a match that was not exact can be found and reviewed.
 */
enum class MatchMethod
{
  NONE,
  HASH,
  SERIAL,
  SIDECAR,
  NAME,
  MANUAL,
};

/*!
 * \ingroup games
 * \brief Translate between the enums above and the lower-case words stored
 *        in the database and sidecar files
 *
 * The words are the contract: a database or NFO written by one version must
 * read in the next, so a word is never renamed once shipped. Unknown words
 * translate to the first enumerator.
 */
class CGameLibraryTypes
{
public:
  static std::string_view ToString(PlatformType type);
  static PlatformType PlatformTypeFromString(std::string_view type);

  static std::string_view ToString(MediaFormat format);
  static MediaFormat MediaFormatFromString(std::string_view format);

  static std::string_view ToString(GameCategory category);
  static GameCategory GameCategoryFromString(std::string_view category);

  static std::string_view ToString(ReleaseStatus status);
  static ReleaseStatus ReleaseStatusFromString(std::string_view status);

  static std::string_view ToString(Licence licence);
  static Licence LicenceFromString(std::string_view licence);

  static std::string_view ToString(DumpStatus status);
  static DumpStatus DumpStatusFromString(std::string_view status);

  static std::string_view ToString(MatchMethod method);
  static MatchMethod MatchMethodFromString(std::string_view method);

  /*!
   * \brief The key two titles are compared on to decide they are one game
   *
   * Case, punctuation, whitespace and a leading article are all ignored, so
   * "The Legend of Zelda", "Legend of Zelda, The" and "legend-of-zelda" share
   * a key. The key is stored beside the title and never shown.
   */
  static std::string TitleKey(std::string_view title);
};

/*!
 * \ingroup games
 * \brief A score from one source, on that source's own scale
 */
struct GameRating
{
  float rating{0.0f};
  float max{10.0f};
  int votes{0};
};

/*!
 * \ingroup games
 * \brief A classification from one ratings board
 */
struct GameAgeRating
{
  std::string board; // e.g. "ESRB", "PEGI", "CERO"
  std::string value; // e.g. "E", "12", "A"
  std::string descriptors; // e.g. "Mild Violence"
};

/*!
 * \ingroup games
 * \brief One file on disk belonging to a release
 */
struct GameFile
{
  int id{-1};
  std::string path;
  uint64_t size{0};
  std::string crc32;
  std::string md5;
  std::string sha1;
  std::string serial;
  std::string raHash; // the RetroAchievements hash, when the platform has one
  int disc{0}; // 1-based for multi-disc releases, 0 otherwise
  int playCount{0};
  std::string lastPlayed;
  std::string dateAdded;
};

/*!
 * \ingroup games
 * \brief One distinct dump of a game
 *
 * A game's regional versions, revisions and pre-release builds are each a
 * release. One of a game's releases is the default, and is what plays and
 * what the game's own art and title come from.
 */
struct GameRelease
{
  int id{-1};
  std::string title; // the title as this release names it
  std::vector<std::string> regions; // lower-case region codes, most specific first
  std::vector<std::string> languages; // lower-case ISO 639-1 codes
  std::string revision; // "Rev 1", "v1.1", empty for the first release
  ReleaseStatus status{ReleaseStatus::RETAIL};
  Licence licence{Licence::LICENSED};
  bool alternate{false};
  DumpStatus dump{DumpStatus::UNKNOWN};
  std::string releaseDate;
  std::string serial;
  std::string notes;
  bool isDefault{false};
  std::vector<GameFile> files;
};

/*!
 * \ingroup games
 * \brief Everything known about a platform
 *
 * A row of the platform table, or an entry in the bundled catalogue before it
 * has been added to the database.
 */
struct PlatformInfo
{
  int id{-1};
  std::string slug; // stable identifier, e.g. "megadrive"
  std::string name;
  std::string sortName;
  std::string manufacturer;
  PlatformType type{PlatformType::UNKNOWN};
  MediaFormat media{MediaFormat::UNKNOWN};
  std::string family; // slug of the platform this one extends, e.g. "megadrive" for Sega CD
  int released{0};
  int discontinued{0};
  std::string overview;
  std::vector<std::string> extensions; // lower case, no leading dot
  std::vector<std::string> aliases;
  std::map<std::string, std::string> providerIds; // provider name -> that provider's id
  std::string defaultGameClient;
  std::string dateAdded;
  std::string lastScraped;
  int gameCount{0};
};
} // namespace GAME
} // namespace KODI
