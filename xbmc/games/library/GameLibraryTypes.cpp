/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameLibraryTypes.h"

#include "utils/StringUtils.h"

#include <array>
#include <cctype>
#include <utility>

using namespace KODI;
using namespace GAME;

namespace
{
template<typename T, size_t N>
using Words = std::array<std::pair<T, std::string_view>, N>;

template<typename T, size_t N>
std::string_view ToWord(const Words<T, N>& words, T value)
{
  for (const auto& [v, word] : words)
  {
    if (v == value)
      return word;
  }
  return words.front().second;
}

template<typename T, size_t N>
T FromWord(const Words<T, N>& words, std::string_view word)
{
  for (const auto& [v, w] : words)
  {
    if (w == word)
      return v;
  }
  return words.front().first;
}

constexpr Words<PlatformType, 8> platformTypes{{
    {PlatformType::UNKNOWN, ""},
    {PlatformType::CONSOLE, "console"},
    {PlatformType::HANDHELD, "handheld"},
    {PlatformType::COMPUTER, "computer"},
    {PlatformType::ARCADE, "arcade"},
    {PlatformType::PINBALL, "pinball"},
    {PlatformType::VIRTUAL, "virtual"},
    {PlatformType::OTHER, "other"},
}};

constexpr Words<MediaFormat, 8> mediaFormats{{
    {MediaFormat::UNKNOWN, ""},
    {MediaFormat::CARTRIDGE, "cartridge"},
    {MediaFormat::DISC, "disc"},
    {MediaFormat::TAPE, "tape"},
    {MediaFormat::DISK, "disk"},
    {MediaFormat::CARD, "card"},
    {MediaFormat::DOWNLOAD, "download"},
    {MediaFormat::OTHER, "other"},
}};

constexpr Words<GameCategory, 7> categories{{
    {GameCategory::RETAIL, "retail"},
    {GameCategory::HACK, "hack"},
    {GameCategory::HOMEBREW, "homebrew"},
    {GameCategory::DEMO, "demo"},
    {GameCategory::BIOS, "bios"},
    {GameCategory::APPLICATION, "application"},
    {GameCategory::NONGAME, "nongame"},
}};

constexpr Words<ReleaseStatus, 9> releaseStatuses{{
    {ReleaseStatus::RETAIL, "retail"},
    {ReleaseStatus::BETA, "beta"},
    {ReleaseStatus::PROTOTYPE, "proto"},
    {ReleaseStatus::SAMPLE, "sample"},
    {ReleaseStatus::DEMO, "demo"},
    {ReleaseStatus::ALPHA, "alpha"},
    {ReleaseStatus::KIOSK, "kiosk"},
    {ReleaseStatus::DEBUG, "debug"},
    {ReleaseStatus::PROGRAM, "program"},
}};

constexpr Words<Licence, 5> licences{{
    {Licence::LICENSED, "licensed"},
    {Licence::UNLICENSED, "unlicensed"},
    {Licence::AFTERMARKET, "aftermarket"},
    {Licence::HOMEBREW, "homebrew"},
    {Licence::PIRATE, "pirate"},
}};

constexpr Words<DumpStatus, 4> dumpStatuses{{
    {DumpStatus::UNKNOWN, ""},
    {DumpStatus::GOOD, "good"},
    {DumpStatus::BAD, "bad"},
    {DumpStatus::VERIFIED, "verified"},
}};

constexpr Words<MatchMethod, 7> matchMethods{{
    {MatchMethod::NONE, ""},
    {MatchMethod::HASH, "hash"},
    {MatchMethod::SERIAL, "serial"},
    {MatchMethod::SIDECAR, "sidecar"},
    {MatchMethod::GAMELIST, "gamelist"},
    {MatchMethod::NAME, "name"},
    {MatchMethod::MANUAL, "manual"},
}};
} // namespace

std::string_view CGameLibraryTypes::ToString(PlatformType type)
{
  return ToWord(platformTypes, type);
}

PlatformType CGameLibraryTypes::PlatformTypeFromString(std::string_view type)
{
  return FromWord(platformTypes, type);
}

std::string_view CGameLibraryTypes::ToString(MediaFormat format)
{
  return ToWord(mediaFormats, format);
}

MediaFormat CGameLibraryTypes::MediaFormatFromString(std::string_view format)
{
  return FromWord(mediaFormats, format);
}

std::string_view CGameLibraryTypes::ToString(GameCategory category)
{
  return ToWord(categories, category);
}

GameCategory CGameLibraryTypes::GameCategoryFromString(std::string_view category)
{
  return FromWord(categories, category);
}

std::string_view CGameLibraryTypes::ToString(ReleaseStatus status)
{
  return ToWord(releaseStatuses, status);
}

ReleaseStatus CGameLibraryTypes::ReleaseStatusFromString(std::string_view status)
{
  return FromWord(releaseStatuses, status);
}

std::string_view CGameLibraryTypes::ToString(Licence licence)
{
  return ToWord(licences, licence);
}

Licence CGameLibraryTypes::LicenceFromString(std::string_view licence)
{
  return FromWord(licences, licence);
}

std::string_view CGameLibraryTypes::ToString(DumpStatus status)
{
  return ToWord(dumpStatuses, status);
}

DumpStatus CGameLibraryTypes::DumpStatusFromString(std::string_view status)
{
  return FromWord(dumpStatuses, status);
}

std::string_view CGameLibraryTypes::ToString(MatchMethod method)
{
  return ToWord(matchMethods, method);
}

MatchMethod CGameLibraryTypes::MatchMethodFromString(std::string_view method)
{
  return FromWord(matchMethods, method);
}

int CGameLibraryTypes::PlatformTypeLabel(PlatformType type)
{
  switch (type)
  {
    case PlatformType::CONSOLE:
      return 35612; // "Console"
    case PlatformType::HANDHELD:
      return 35613; // "Handheld"
    case PlatformType::COMPUTER:
      return 35614; // "Computer"
    case PlatformType::ARCADE:
      return 35615; // "Arcade"
    case PlatformType::PINBALL:
      return 35616; // "Pinball"
    case PlatformType::VIRTUAL:
      return 35617; // "Virtual machine"
    case PlatformType::UNKNOWN:
    case PlatformType::OTHER:
      break;
  }
  return 0;
}

int CGameLibraryTypes::MediaFormatLabel(MediaFormat format)
{
  switch (format)
  {
    case MediaFormat::CARTRIDGE:
      return 35578; // "Cartridge"
    case MediaFormat::DISC:
      return 35579; // "Disc"
    case MediaFormat::TAPE:
      return 35618; // "Tape"
    case MediaFormat::DISK:
      return 35619; // "Floppy disk"
    case MediaFormat::CARD:
      return 35620; // "Card"
    case MediaFormat::DOWNLOAD:
      return 35621; // "Download"
    case MediaFormat::UNKNOWN:
    case MediaFormat::OTHER:
      break;
  }
  return 0;
}

int CGameLibraryTypes::ReleaseStatusLabel(ReleaseStatus status)
{
  switch (status)
  {
    case ReleaseStatus::BETA:
      return 35705; // "Beta"
    case ReleaseStatus::PROTOTYPE:
      return 35706; // "Prototype"
    case ReleaseStatus::SAMPLE:
      return 35707; // "Sample"
    case ReleaseStatus::DEMO:
      return 35596; // "Demo"
    case ReleaseStatus::ALPHA:
      return 35708; // "Alpha"
    case ReleaseStatus::KIOSK:
      return 35709; // "Kiosk"
    case ReleaseStatus::DEBUG:
      return 35710; // "Debug"
    case ReleaseStatus::PROGRAM:
      return 35597; // "BIOS"
    case ReleaseStatus::RETAIL:
      break;
  }
  return 0;
}

int CGameLibraryTypes::LicenceLabel(Licence licence)
{
  switch (licence)
  {
    case Licence::UNLICENSED:
      return 35711; // "Unlicensed"
    case Licence::AFTERMARKET:
      return 35712; // "Aftermarket"
    case Licence::HOMEBREW:
      return 35537; // "Homebrew"
    case Licence::PIRATE:
      return 35713; // "Pirate"
    case Licence::LICENSED:
      break;
  }
  return 0;
}

int CGameLibraryTypes::CategoryLabel(GameCategory category)
{
  switch (category)
  {
    case GameCategory::HACK:
      return 35536; // "Hacks"
    case GameCategory::HOMEBREW:
      return 35537; // "Homebrew"
    case GameCategory::DEMO:
      return 35596; // "Demo"
    case GameCategory::BIOS:
      return 35597; // "BIOS"
    case GameCategory::APPLICATION:
      return 35598; // "Application"
    case GameCategory::NONGAME:
      return 35676; // "Not a video game"
    case GameCategory::RETAIL:
      break;
  }
  return 0;
}

std::string CGameLibraryTypes::TitleKey(std::string_view title)
{
  std::string lower = StringUtils::ToLower(std::string(title));
  StringUtils::Replace(lower, "&", " and ");

  // "Legend of Zelda, The" and "The Legend of Zelda" are the same title
  static constexpr std::array<std::string_view, 3> articles{"the", "a", "an"};
  std::vector<std::string> words = StringUtils::Split(lower, ' ');
  std::erase_if(words, [](const std::string& w) { return w.empty(); });
  if (!words.empty())
  {
    for (std::string_view article : articles)
    {
      if (words.front() == article)
      {
        words.erase(words.begin());
        break;
      }
      std::string last = words.back();
      if (last.ends_with(','))
        last.pop_back();
      if (last == article && words.size() > 1)
      {
        words.pop_back();
        break;
      }
    }
  }

  std::string key;
  for (const std::string& word : words)
  {
    for (const char c : word)
    {
      if (std::isalnum(static_cast<unsigned char>(c)))
        key += c;
    }
  }
  return key;
}

std::string KODI::GAME::RomsetsToString(const std::vector<EmulatorRomset>& romsets)
{
  std::vector<std::string> pairs;
  for (const EmulatorRomset& romset : romsets)
  {
    std::string pair = romset.gameClient + "=" + romset.romset;
    for (const std::string& required : romset.required)
      pair += "+" + required;
    pairs.emplace_back(std::move(pair));
  }
  return StringUtils::Join(pairs, ";");
}

std::vector<EmulatorRomset> KODI::GAME::RomsetsFromString(std::string_view text)
{
  std::vector<EmulatorRomset> romsets;
  for (const std::string& pair : StringUtils::Split(std::string(text), ';'))
  {
    const size_t equals = pair.find('=');
    if (equals == std::string::npos || equals + 1 == pair.size())
      continue;
    std::vector<std::string> names = StringUtils::Split(pair.substr(equals + 1), '+');
    const std::string romset = names.front();
    names.erase(names.begin());
    romsets.push_back({pair.substr(0, equals), romset, std::move(names)});
  }
  return romsets;
}
