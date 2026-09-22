/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameListXml.h"

#include "GameNameParser.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <array>
#include <charconv>
#include <string_view>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* GAMELIST_NAME = "gamelist.xml";

//! The art a gamelist names, and what the library calls it
constexpr std::array<std::pair<const char*, const char*>, 7> ART_ELEMENTS = {{
    {"thumbnail", "boxfront"}, // ES writes the box here
    {"image", "snap"}, // and a screenshot here, unless it is all there is
    {"marquee", "clearlogo"},
    {"fanart", "fanart"},
    {"titleshot", "titlescreen"},
    {"cartridge", "cartridge"},
    {"boxback", "boxback"},
}};

std::string ElementText(const tinyxml2::XMLElement* parent, const char* name)
{
  std::string value;
  XMLUtils::GetString(parent, name, value);
  StringUtils::Trim(value);
  return value;
}

bool ElementBool(const tinyxml2::XMLElement* parent, const char* name)
{
  std::string value = ElementText(parent, name);
  StringUtils::ToLower(value);
  return value == "true" || value == "yes" || value == "1";
}

//! \brief "./media/box/Sonic.png" beside the file, made absolute
std::string Resolve(const std::string& folder, const std::string& relative)
{
  if (relative.empty())
    return {};
  // A path that is already whole is left alone; the rest are beside the file
  if (relative.front() == '/' || URIUtils::IsURL(relative) ||
      (relative.size() > 2 && relative[1] == ':'))
    return relative;

  std::string trimmed = relative;
  while (StringUtils::StartsWith(trimmed, "./"))
    trimmed.erase(0, 2);
  return URIUtils::AddFileToFolder(folder, trimmed);
}

/*!
 * \brief Read the date these files write, which is not the one Kodi writes
 *
 * "19910623T000000" is the usual form; a bare year and an ISO date are both
 * seen as well, so all three are read and anything else is left alone.
 */
void ReadDate(const std::string& text, std::string& date, int& year)
{
  std::string digits;
  for (const char c : text)
  {
    if (std::isdigit(static_cast<unsigned char>(c)))
      digits += c;
    else if (c == 'T' || c == 't')
      break;
  }

  if (digits.size() >= 4)
  {
    int value = 0;
    const auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + 4, value);
    if (ec == std::errc() && value > 1900 && value < 2200)
      year = value;
  }
  if (digits.size() >= 8 && year > 0)
    date = digits.substr(0, 4) + "-" + digits.substr(4, 2) + "-" + digits.substr(6, 2);
}

//! \brief "1", "2", "1-4" and "4+" all say how many can play
void ReadPlayers(const std::string& text, int& playersMin, int& playersMax)
{
  int numbers[2] = {0, 0};
  size_t found = 0;
  size_t pos = 0;
  while (pos < text.size() && found < 2)
  {
    if (!std::isdigit(static_cast<unsigned char>(text[pos])))
    {
      ++pos;
      continue;
    }
    size_t end = pos;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
      ++end;
    int value = 0;
    std::from_chars(text.data() + pos, text.data() + end, value);
    numbers[found++] = value;
    pos = end;
  }

  if (found == 0)
    return;
  playersMin = numbers[0];
  playersMax = found > 1 ? numbers[1] : numbers[0];
  if (playersMax < playersMin)
    std::swap(playersMin, playersMax);
}
} // namespace

bool CGameListXml::Load(const std::string& folder, std::map<std::string, GameListEntry>& entries)
{
  const std::string path = URIUtils::AddFileToFolder(folder, GAMELIST_NAME);
  if (!XFILE::CFile::Exists(path, false))
    return false;

  CXBMCTinyXML2 doc;
  if (!doc.LoadFile(path))
  {
    CLog::Log(LOGWARNING, "GAME: Cannot read {}", path);
    return false;
  }

  const tinyxml2::XMLElement* root = doc.RootElement();
  if (root == nullptr || std::string_view(root->Value()) != "gameList")
  {
    CLog::Log(LOGWARNING, "GAME: {} is not a game list", path);
    return false;
  }

  size_t read = 0;
  for (const tinyxml2::XMLElement* element = root->FirstChildElement("game"); element != nullptr;
       element = element->NextSiblingElement("game"))
  {
    GameListEntry entry;
    entry.path = Resolve(folder, ElementText(element, "path"));
    if (entry.path.empty())
      continue;

    entry.title = ElementText(element, "name");
    entry.overview = ElementText(element, "desc");
    entry.developer = ElementText(element, "developer");
    entry.publisher = ElementText(element, "publisher");

    // One element, but several front ends write a comma or slash separated list
    const std::string genres = ElementText(element, "genre");
    static constexpr std::array<std::string_view, 2> genreSeparators{",", "/"};
    for (std::string genre : StringUtils::Split(genres, genreSeparators))
    {
      StringUtils::Trim(genre);
      if (!genre.empty())
        entry.genres.emplace_back(std::move(genre));
    }

    ReadDate(ElementText(element, "releasedate"), entry.releaseDate, entry.year);
    ReadPlayers(ElementText(element, "players"), entry.playersMin, entry.playersMax);

    // The file rates a game from 0 to 1; the library from 0 to 10
    const std::string rating = ElementText(element, "rating");
    if (!rating.empty())
    {
      const double value = std::strtod(rating.c_str(), nullptr);
      if (value > 0.0)
        entry.rating = static_cast<float>(value <= 1.0 ? value * 10.0 : value);
    }

    entry.region = CGameNameParser::RegionName(ElementText(element, "region"));
    for (std::string language : StringUtils::Split(ElementText(element, "lang"), ','))
    {
      StringUtils::Trim(language);
      StringUtils::ToLower(language);
      if (!language.empty())
        entry.languages.emplace_back(std::move(language));
    }

    entry.favourite = ElementBool(element, "favorite");
    entry.hidden = ElementBool(element, "hidden");
    entry.kidGame = ElementBool(element, "kidgame");
    entry.playCount = 0;
    XMLUtils::GetInt(element, "playcount", entry.playCount);
    entry.lastPlayed = ElementText(element, "lastplayed");
    entry.trailer = Resolve(folder, ElementText(element, "video"));

    for (const auto& [name, type] : ART_ELEMENTS)
    {
      const std::string art = Resolve(folder, ElementText(element, name));
      if (!art.empty())
        entry.art[type] = art;
    }
    // With no box named, the picture the file does name is the one to show
    if (entry.art.find("boxfront") == entry.art.end())
    {
      const auto snap = entry.art.find("snap");
      if (snap != entry.art.end())
        entry.art["boxfront"] = snap->second;
    }

    entries[entry.path] = std::move(entry);
    ++read;
  }

  CLog::Log(LOGDEBUG, "GAME: Read {} entries from {}", read, path);
  return read > 0;
}
