/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLibraryTypes.h"
#include "utils/Artwork.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class CFileItem;

namespace ADDON
{
class CScraper;
}

namespace KODI
{
namespace GAME
{
class CGameInfoTag;

/*!
 * \ingroup games
 * \brief What a scraper is told about a file so it can identify it
 */
struct GameScrapeRequest
{
  std::string title;
  std::string fileName;
  std::string platformSlug;
  std::map<std::string, std::string> platformIds;
  std::string crc32;
  std::string md5;
  std::string sha1;
  std::string serial;
  std::string raHash;
  uint64_t size{0};
  std::vector<std::string> regions;
  std::vector<std::string> languages;
  int year{0};

  //! True while a folder is being scanned, false when a person asked about
  //! this one game. A source with a small allowance is left out of a scan.
  bool bulk{false};
};

/*!
 * \ingroup games
 * \brief One answer to a search
 */
struct GameScrapeCandidate
{
  std::string id;
  std::string title;
  int year{0};
  float score{0.0f};
  MatchMethod matchedBy{MatchMethod::NONE};
  std::string details; // the details payload, when the scraper sent it with the candidate

  // What tells two candidates of the same title apart, when a person has to pick
  std::string subtitle; // the catalogue's own name, e.g. "Sonic 2 (USA, Europe) (Rev A)"
  std::vector<std::string> regions;
  std::string provider; // which source answered
  std::string thumb; // an image to show beside the title
};

/*!
 * \ingroup games
 * \brief One piece of art a scraper offers
 */
struct GameScrapeArt
{
  std::string type;
  std::string url;
  std::string region;
  int width{0};
  int height{0};
};

/*!
 * \ingroup games
 *
 * \brief A game scraper add-on, driven through the protocol in
 *        docs/GAME_SCRAPER_PROTOCOL.md
 *
 * Every call is a plugin:// round trip; the add-on answers with JSON in a
 * list-item property, which is read here into the library's own types. A
 * scraper that answers with nothing, or with something unreadable, is a
 * scraper that found nothing: no result is ever invented on its behalf.
 */
class CGameScraper
{
public:
  explicit CGameScraper(std::shared_ptr<ADDON::CScraper> addon);
  ~CGameScraper();

  /*!
   * \brief The enabled game scraper with this add-on ID, or nullptr
   */
  static std::unique_ptr<CGameScraper> Create(const std::string& addonId);

  /*!
   * \brief The user's default game scraper, or the first one enabled
   */
  static std::unique_ptr<CGameScraper> CreateDefault();

  std::string ID() const;
  std::string Name() const;

  /*!
   * \brief Give the scraper the per-path settings it was set up with
   */
  void SetPathSettings(const std::string& settingsXml);

  /*!
   * \brief Ask what a file might be
   *
   * \return Candidates best first; empty when nothing matched
   */
  std::vector<GameScrapeCandidate> Find(const GameScrapeRequest& request);

  /*!
   * \brief Ask about many files in one call
   *
   * A scan spends its time in the round trip to the add-on, not in the
   * lookup, so a full set is asked about in batches. The answer holds one
   * list per request, in the order they were given.
   *
   * \return false when the scraper does not answer batches; the caller then
   *         asks about each file on its own
   */
  bool FindMany(const std::vector<GameScrapeRequest>& requests,
                std::vector<std::vector<GameScrapeCandidate>>& answers);

  //! \brief Whether a batch was refused once already, so it is not tried again
  bool AnswersBatches() const { return m_answersBatches; }

  /*!
   * \brief Ask for everything about one candidate
   *
   * \param[out] details Filled with what the scraper knows; existing values
   *             are replaced only where the scraper gave one
   * \param[out] art Every piece of art offered, by type
   *
   * \return True if the scraper answered
   */
  bool GetDetails(const std::string& id,
                  const GameScrapeRequest& request,
                  CGameInfoTag& details,
                  std::map<std::string, std::vector<GameScrapeArt>>& art);

  /*!
   * \brief Read everything about a candidate from a details payload the
   *        scraper already sent
   */
  bool GetDetails(const GameScrapeCandidate& candidate,
                  const GameScrapeRequest& request,
                  CGameInfoTag& details,
                  std::map<std::string, std::vector<GameScrapeArt>>& art);

  /*!
   * \brief Ask about a platform
   */
  bool GetPlatform(const GameScrapeRequest& request,
                   PlatformInfo& platform,
                   std::map<std::string, std::vector<GameScrapeArt>>& art);

private:
  bool m_answersBatches{true};

  std::string BuildUrl(const std::string& action, const GameScrapeRequest& request) const;
  static bool ReadCandidate(const CFileItem& item, GameScrapeCandidate& candidate, int& queryIndex);
  bool ReadDetails(const std::string& id,
                   const std::string& json,
                   CGameInfoTag& details,
                   std::map<std::string, std::vector<GameScrapeArt>>& art);

  std::shared_ptr<ADDON::CScraper> m_addon;
};
} // namespace GAME
} // namespace KODI
