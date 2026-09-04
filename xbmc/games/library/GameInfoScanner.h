/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLibraryTypes.h"
#include "GameListXml.h"
#include "GameScraper.h"
#include "InfoScanner.h"
#include "games/database/GameDatabase.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class CFileItem;
class CFileItemList;

namespace KODI
{
namespace GAME
{
class CGameInfoTag;
class CGameScraper;
class CPlatformCatalogue;
struct ParsedGameName;

/*!
 * \ingroup games
 *
 * \brief Add the games under a folder to the library
 *
 * Walks every folder that has been given a platform, folds the files it
 * finds into releases (a cue sheet and its tracks, a disc set, a folder that
 * is one game), works out what identifies each one, asks the scraper, and
 * stores what came back. A folder whose files have not changed since the
 * last scan is skipped by a hash of their names and dates, so a rescan of a
 * large library costs seconds.
 *
 * Identification never guesses. A file the scraper cannot name is stored
 * with the title its file name gives and marked as unidentified, so it can
 * be browsed and played and revisited later.
 */
class CGameInfoScanner : public CInfoScanner
{
public:
  CGameInfoScanner();
  ~CGameInfoScanner() override;

  /*!
   * \brief Scan a folder, or every folder with content when none is given
   */
  void Start(const std::string& directory);

  /*!
   * \brief Ask the running scan to stop at the next file
   */
  void Stop();

  /*!
   * \brief Whether the user is watching, and can be asked which game a file is
   *
   * A scan is never interactive: it takes the best candidate and marks the
   * game as matched by name, for review afterwards. A refresh the user asked
   * for is, and offers the candidates the way the video library does.
   */
  void SetInteractive(bool interactive) { m_interactive = interactive; }

  /*!
   * \brief Identify and describe one library game again
   *
   * Its releases, files and the user's own state are kept; everything the
   * scraper answers replaces what it answered before.
   */
  bool RefreshGame(int idGame);

  /*!
   * \brief What the scanner learnt about one file, before it is stored
   */
  struct Entry
  {
    std::string path; // the file that plays, or the cue sheet
    std::vector<std::string> files; // every file of the release, path first
    std::string folder;
    bool isFolder{false};
  };

  /*!
   * \brief Group a folder's files into the games they belong to
   *
   * A cue or GDI sheet claims the tracks beside it, an M3U claims the discs
   * it lists, and in a folder-per-game layout the folder is the game.
   */
  static std::vector<Entry> GroupEntries(const CFileItemList& items, bool useFolderNames);

  /*!
   * \brief Build the request a scraper is sent for an entry
   */
  static void FillRequest(const Entry& entry,
                          const ParsedGameName& parsed,
                          const GameFile& identity,
                          const PlatformInfo& platform,
                          struct GameScrapeRequest& request);

protected:
  // Implementation of CInfoScanner
  std::pair<ScanComplete, ContentFound> DoScan(const std::string& strDirectory) override;

private:
  void Process();
  bool ScanFolder(const std::string& folder, const GamePathContent& content, const PlatformInfo& platform);
  bool ScanEntry(const Entry& entry,
                 const GamePathContent& content,
                 const PlatformInfo& platform,
                 int refreshGameId = -1);
  const struct GameScrapeCandidate* ChooseCandidate(CGameScraper& scraper,
                                                    struct GameScrapeRequest& request,
                                                    std::vector<struct GameScrapeCandidate>& candidates);
  CGameScraper* ScraperFor(const GamePathContent& content);

  //! \brief Ask the scraper how far the signed-in person has got, and record it
  void RefreshProgress();

  //! \brief Describe the machine itself, once, with its own pictures
  void ScrapePlatform(const GamePathContent& content, const PlatformInfo& platform);

  //! \brief Whether the library may ask the internet, or reads what is on this disk
  static bool DownloadsAllowed();

  //! \brief A game as it was sold, rather than a hack, a prototype or a BIOS
  static bool IsRetail(const ParsedGameName& parsed);

  /*!
   * \brief Ask the scraper about a folder's next hundred games at once
   *
   * A scan spends nearly all its time in the round trip to the add-on, so the
   * files of a chunk are hashed and asked about together. What comes back is
   * kept until those entries have been scanned. A scraper that cannot answer
   * batches leaves the caches empty and each file is asked about on its own.
   */
  void Prefetch(const Entry* entries,
                size_t count,
                const GamePathContent& content,
                const PlatformInfo& platform);

  std::string FolderHash(const CFileItemList& items) const;

  CGameDatabase m_database;
  std::unique_ptr<CPlatformCatalogue> m_catalogue;
  std::map<std::string, std::unique_ptr<CGameScraper>> m_scrapers;
  std::string m_extensions;
  std::map<std::string, GameListEntry> m_gameList; // what the folder's gamelist.xml said
  std::map<std::string, GameFile> m_identities; // hashed ahead of the scan, by path
  std::map<std::string, std::vector<struct GameScrapeCandidate>> m_prefetched;
  std::set<std::string> m_pathsToClean;
  int m_currentItem{0};
  int m_itemCount{0};
  bool m_interactive{false};
  bool m_retailOnly{false};
  int m_added{0};
  int m_identified{0};
};
} // namespace GAME
} // namespace KODI
