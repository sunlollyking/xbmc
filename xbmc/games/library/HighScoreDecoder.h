/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tinyxml2
{
class XMLDocument;
}

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief How much of a decoded high score file to show
 *
 * A definition marks what is worth showing: the scores themselves always,
 * settings and totals as extra, and what only helps someone working on the
 * definition as debug.
 */
enum class HighScoreDetail
{
  ALWAYS,
  EXTRA,
  DEBUG,
};

/*!
 * \ingroup games
 * \brief One table a high score file decodes to, such as the top ten
 */
struct HighScoreTable
{
  std::string id; // e.g. "TOP SCORES", often empty
  std::vector<std::string> columns; // e.g. "RANK", "SCORE", "NAME"
  std::vector<std::vector<std::string>> rows;

  bool operator==(const HighScoreTable&) const = default;
};

/*!
 * \ingroup games
 * \brief One value a high score file decodes to, outside any table
 */
struct HighScoreField
{
  std::string id;
  std::string value;
};

/*!
 * \ingroup games
 * \brief What a high score file decodes to, in the order its definition
 *        lists it
 */
struct HighScores
{
  struct Part
  {
    bool isTable{false};
    HighScoreTable table;
    HighScoreField field;
  };
  std::vector<Part> parts;
};

/*!
 * \ingroup games
 * \brief Reads the high scores an arcade emulator saved, using a hi2txt
 *        description of the game
 *
 * An emulator saves a game's high score table as the bytes the game kept it
 * in, which every game lays out its own way. hi2txt describes those layouts,
 * one XML file per game; the format is documented at
 * https://greatstoneex.github.io/hi2txt-doc/doc/xml.html. This reads the
 * parts of it that the descriptions use and refuses a file it cannot read,
 * rather than show scores it has guessed at.
 */
class CHighScoreDecoder
{
public:
  //! Loads another game's description by its set name, or returns ""
  using DescriptionLoader = std::function<std::string(const std::string& setName)>;

  CHighScoreDecoder();
  ~CHighScoreDecoder();

  /*!
   * \brief Read a game's description
   *
   * \param xml The description
   * \param loader Where a clone that says it is the same as its parent finds
   *        the parent's description
   */
  bool Load(const std::string& xml, const DescriptionLoader& loader);

  /*!
   * \brief Read the named characters the descriptions use, such as
   *        &black-heart;, from hi2txt's DTD
   *
   * They're declared there rather than in each description, and an XML parser
   * that doesn't read DTDs would show them as they're written.
   */
  void SetEntities(const std::string& dtd);

  /*!
   * \brief Decode a high score file
   *
   * \param fileName The file's name, such as "galaga.hi" or "nvram", which
   *        picks the layout that applies to it
   * \param data The file's contents
   * \param detail How much to keep
   * \param[out] scores What the file holds
   */
  bool Decode(const std::string& fileName,
              const std::vector<uint8_t>& data,
              HighScoreDetail detail,
              HighScores& scores) const;

private:
  std::string Expand(const std::string& xml) const;

  std::unique_ptr<tinyxml2::XMLDocument> m_document;
  std::vector<std::pair<std::string, std::string>> m_entities;
};
} // namespace GAME
} // namespace KODI
