/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameHighScores.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "addons/AddonManager.h"
#include "addons/IAddon.h"
#include "addons/addoninfo/AddonType.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/database/GameDatabase.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <algorithm>

using namespace KODI;
using namespace GAME;

namespace
{
//! Descriptions and score files are a few kilobytes
constexpr size_t MAX_FILE_SIZE = 1024 * 1024;
//! More than any game's table holds, so a damaged library row can't grow without end
constexpr size_t MAX_ROWS = 100;
constexpr size_t MAX_COLUMNS = 20;
constexpr size_t MAX_CELL = 256;

//! An arcade set name, as MAME and FBNeo write them: never a path
bool IsSetName(const std::string& name)
{
  return !name.empty() && name.size() <= 32 &&
         std::ranges::all_of(name,
                             [](char c)
                             {
                               return StringUtils::isasciilowercaseletter(c) ||
                                      (c >= '0' && c <= '9') || c == '_';
                             });
}

std::string ReadText(const std::string& path)
{
  std::vector<uint8_t> data;
  XFILE::CFile file;
  if (!XFILE::CFile::Exists(path) || file.LoadFile(path, data) <= 0 || data.size() > MAX_FILE_SIZE)
    return "";
  return std::string(data.begin(), data.end());
}

/*!
 * \brief Where a game client's emulator saved a set's scores
 *
 * game.libretro gives its emulator a save folder in the add-on's profile
 * folder, and each emulator keeps its scores in a folder of its own inside,
 * such as mame2003-plus/hi or fbneo/hiscores.
 */
std::string FindScoreFile(const std::string& gameClient, const std::string& set)
{
  const std::string saves = "special://profile/addon_data/" + gameClient + "/save/";
  std::vector<std::string> folders{saves, saves + "hi/"};

  CFileItemList subfolders;
  if (XFILE::CDirectory::GetDirectory(saves, subfolders, "/", XFILE::DIR_FLAG_NO_FILE_DIRS))
  {
    for (const auto& item : subfolders)
    {
      if (!item->IsFolder())
        continue;
      std::string folder = item->GetPath();
      URIUtils::AddSlashAtEnd(folder);
      folders.emplace_back(folder);
      folders.emplace_back(folder + "hi/");
      folders.emplace_back(folder + "hiscores/");
    }
  }

  for (const std::string& folder : folders)
  {
    const std::string path = URIUtils::AddFileToFolder(folder, set + ".hi");
    if (XFILE::CFile::Exists(path))
      return path;
  }
  return "";
}

std::string Lowercase(std::string text)
{
  StringUtils::ToLower(text);
  return text;
}

std::string Trimmed(std::string text)
{
  StringUtils::Trim(text);
  return text;
}
} // namespace

void CGameHighScores::Record(const std::string& gamePath, const std::string& gameClient)
{
  if (gamePath.empty() || gameClient.empty())
    return;

  ADDON::AddonPtr addon;
  if (!CServiceBroker::GetAddonMgr().GetAddon(DESCRIPTIONS_ADDON, addon,
                                              ADDON::AddonType::RESOURCE_GAMES,
                                              ADDON::OnlyEnabled::CHOICE_YES))
    return;
  const std::string descriptions = URIUtils::AddFileToFolder(addon->Path(), "resources");

  // An arcade set is named by its file: galaga.zip holds the set galaga
  std::string set = URIUtils::GetFileName(gamePath);
  URIUtils::RemoveExtension(set);
  set = Lowercase(set);
  if (!IsSetName(set))
    return;

  const std::string description = ReadText(URIUtils::AddFileToFolder(descriptions, set + ".xml"));
  if (description.empty())
    return;

  const std::string scoreFile = FindScoreFile(gameClient, set);
  if (scoreFile.empty())
    return;

  std::vector<uint8_t> data;
  XFILE::CFile file;
  if (file.LoadFile(scoreFile, data) <= 0 || data.size() > MAX_FILE_SIZE)
    return;

  const auto load = [&descriptions](const std::string& parent)
  {
    return IsSetName(parent) ? ReadText(URIUtils::AddFileToFolder(descriptions, parent + ".xml"))
                             : std::string();
  };

  HighScoreTable table;
  if (!Decode(description, ReadText(URIUtils::AddFileToFolder(descriptions, "hi2txt.dtd")), load,
              set + ".hi", data, table))
    return;

  CGameDatabase db;
  if (db.Open() && db.SetHighScoresForFile(gamePath, Serialize(table)))
    CLog::Log(LOGDEBUG, "GAME: Kept the high scores of {}", set);
}

bool CGameHighScores::Decode(const std::string& description,
                             const std::string& dtd,
                             const CHighScoreDecoder::DescriptionLoader& load,
                             const std::string& fileName,
                             const std::vector<uint8_t>& data,
                             HighScoreTable& table)
{
  CHighScoreDecoder decoder;
  decoder.SetEntities(dtd);
  if (!decoder.Load(description, load))
    return false;

  HighScores scores;
  if (!decoder.Decode(fileName, data, HighScoreDetail::ALWAYS, scores))
    return false;

  for (const HighScores::Part& part : scores.parts)
  {
    if (part.isTable && !part.table.rows.empty())
    {
      table = part.table;
      return true;
    }
  }

  // A game that keeps only its best score shows it on its own, as fields
  HighScoreTable fields;
  std::vector<std::string> row;
  for (const HighScores::Part& part : scores.parts)
  {
    if (!part.isTable)
    {
      fields.columns.emplace_back(part.field.id);
      row.emplace_back(part.field.value);
    }
  }
  if (row.empty())
    return false;
  fields.rows.emplace_back(std::move(row));
  table = std::move(fields);
  return true;
}

std::string CGameHighScores::Best(const HighScoreTable& table)
{
  if (table.rows.empty())
    return "";

  // The score is the column that says so, or else the first one that isn't the rank
  const auto& columns = table.columns;
  auto score = std::ranges::find_if(columns, [](const std::string& c)
                                    { return Lowercase(c).find("score") != std::string::npos; });
  if (score == columns.end())
    score =
        std::ranges::find_if(columns, [](const std::string& c) { return Lowercase(c) != "rank"; });
  const auto name =
      std::ranges::find_if(columns,
                           [](const std::string& c)
                           {
                             return Lowercase(c).find("name") != std::string::npos ||
                                    Lowercase(c).find("initials") != std::string::npos;
                           });

  const std::vector<std::string>& best = table.rows.front();
  auto cell = [&best, &columns](auto column)
  {
    const auto at = static_cast<size_t>(column - columns.begin());
    return column != columns.end() && at < best.size() ? Trimmed(best[at]) : std::string();
  };

  const std::string scoreText = cell(score);
  const std::string nameText = cell(name);
  if (scoreText.empty() || nameText.empty())
    return scoreText.empty() ? nameText : scoreText;
  return scoreText + " · " + nameText;
}

std::string CGameHighScores::Text(const HighScoreTable& table)
{
  if (table.rows.empty())
    return "";

  std::string text = "[B]" + StringUtils::Join(table.columns, "   ") + "[/B]";
  for (const std::vector<std::string>& row : table.rows)
  {
    std::vector<std::string> cells;
    for (const std::string& cell : row)
      cells.emplace_back(Trimmed(cell));
    text += "\n" + StringUtils::Join(cells, "   ");
  }
  return text;
}

std::string CGameHighScores::Serialize(const HighScoreTable& table)
{
  CVariant value(CVariant::VariantTypeObject);
  value["columns"] = CVariant(CVariant::VariantTypeArray);
  for (const std::string& column : table.columns)
    value["columns"].push_back(column);
  value["rows"] = CVariant(CVariant::VariantTypeArray);
  for (const std::vector<std::string>& row : table.rows)
  {
    CVariant cells(CVariant::VariantTypeArray);
    for (const std::string& cell : row)
      cells.push_back(cell);
    value["rows"].push_back(cells);
  }

  std::string json;
  return CJSONVariantWriter::Write(value, json, true) ? json : "";
}

HighScoreTable CGameHighScores::Deserialize(const std::string& json)
{
  HighScoreTable table;
  CVariant value;
  if (json.empty() || !CJSONVariantParser::Parse(json, value) || !value.isObject() ||
      !value["columns"].isArray() || !value["rows"].isArray())
    return table;

  for (auto it = value["columns"].begin_array();
       it != value["columns"].end_array() && table.columns.size() < MAX_COLUMNS; ++it)
    table.columns.emplace_back(it->asString().substr(0, MAX_CELL));

  for (auto row = value["rows"].begin_array();
       row != value["rows"].end_array() && table.rows.size() < MAX_ROWS; ++row)
  {
    if (!row->isArray())
      continue;
    std::vector<std::string> cells;
    for (auto cell = row->begin_array(); cell != row->end_array() && cells.size() < MAX_COLUMNS;
         ++cell)
      cells.emplace_back(cell->asString().substr(0, MAX_CELL));
    table.rows.emplace_back(std::move(cells));
  }
  return table;
}
