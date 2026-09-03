/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameInfoScanner.h"

#include "FileItem.h"
#include "GUIInfoManager.h"
#include "GUIUserMessages.h"
#include "FileItemList.h"
#include "GameFileIdentity.h"
#include "GameNameParser.h"
#include "GameScraper.h"
#include "PlatformCatalogue.h"
#include "ReleasePolicy.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "dialogs/GUIDialogExtendedProgressBar.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/tags/GameInfoTag.h"
#include "games/tags/GameInfoTagLoader.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "guilib/guiinfo/LibraryGUIInfo.h"
#include "guilib/WindowIDs.h"
#include "interfaces/AnnouncementManager.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/Digest.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <map>

using namespace KODI;
using namespace GAME;

namespace
{
// Containers and sheets every platform may use, beside its own extensions
constexpr std::array<const char*, 8> commonExtensions{"zip", "7z", "cue", "gdi", "m3u",
                                                      "chd", "iso", "bin"};
constexpr std::array<const char*, 3> sheetExtensions{".cue", ".gdi", ".m3u"};
constexpr std::array<const char*, 4> trackExtensions{".bin", ".img", ".raw", ".wav"};

std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}

std::string Stem(const std::string& path)
{
  std::string name = URIUtils::GetFileName(path);
  URIUtils::RemoveExtension(name);
  return name;
}

bool HasExtension(const std::string& path, const auto& extensions)
{
  const std::string ext = StringUtils::ToLower(URIUtils::GetExtension(path));
  return std::ranges::any_of(extensions, [&ext](const char* e) { return ext == e; });
}

std::string ExtensionMask(const PlatformInfo& platform)
{
  std::vector<std::string> extensions = platform.extensions;
  for (const char* ext : commonExtensions)
    extensions.emplace_back(ext);
  std::ranges::sort(extensions);
  extensions.erase(std::ranges::unique(extensions).begin(), extensions.end());

  std::string mask;
  for (const std::string& ext : extensions)
    mask += "." + ext + "|";
  if (!mask.empty())
    mask.pop_back();
  return mask;
}

bool SameRelease(const GameRelease& a, const GameRelease& b)
{
  auto sorted = [](std::vector<std::string> v)
  {
    std::ranges::sort(v);
    return v;
  };
  return sorted(a.regions) == sorted(b.regions) && sorted(a.languages) == sorted(b.languages) &&
         a.revision == b.revision && a.status == b.status && a.licence == b.licence;
}

std::vector<std::string> ReadM3U(const std::string& path)
{
  std::vector<std::string> discs;
  XFILE::CFile file;
  std::vector<uint8_t> data;
  if (!file.LoadFile(path, data) || data.size() > 64 * 1024)
    return discs;

  const std::string folder = URIUtils::GetDirectory(path);
  for (std::string line : StringUtils::Split(std::string(data.begin(), data.end()), '\n'))
  {
    StringUtils::Trim(line, " \t\r");
    if (line.empty() || line.front() == '#')
      continue;
    discs.emplace_back(CURL::IsFullPath(line) ? line
                                                       : URIUtils::AddFileToFolder(folder, line));
  }
  return discs;
}
} // namespace

CGameInfoScanner::CGameInfoScanner() = default;

CGameInfoScanner::~CGameInfoScanner() = default;

void CGameInfoScanner::Start(const std::string& directory)
{
  m_bRunning = true;
  m_pathsToScan.clear();

  if (!m_database.Open())
  {
    m_bRunning = false;
    return;
  }

  if (directory.empty())
  {
    std::vector<std::string> roots;
    m_database.GetContentPaths(roots);
    m_pathsToScan.insert(roots.begin(), roots.end());
  }
  else
  {
    std::string folder = directory;
    URIUtils::AddSlashAtEnd(folder);
    m_pathsToScan.insert(folder);
  }

  Process();

  m_database.Close();
  m_bRunning = false;
}

void CGameInfoScanner::Stop()
{
  m_bRunning = false;
}

void CGameInfoScanner::Process()
{
  m_catalogue = std::make_unique<CPlatformCatalogue>();
  m_catalogue->Load();
  m_added = 0;
  m_identified = 0;

  if (m_showDialog)
  {
    auto* dialog = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogExtendedProgressBar>(
        WINDOW_DIALOG_EXT_PROGRESS);
    if (dialog != nullptr)
      m_handle = dialog->GetHandle(Localize(35546));
  }

  auto& announcer = *CServiceBroker::GetAnnouncementManager();
  announcer.Announce(ANNOUNCEMENT::GameLibrary, "OnScanStarted");

  const std::set<std::string, std::less<>> roots = m_pathsToScan;
  for (const std::string& root : roots)
  {
    if (!m_bRunning)
      break;
    DoScan(root);
  }

  announcer.Announce(ANNOUNCEMENT::GameLibrary, "OnScanFinished");
  CLog::Log(LOGINFO, "GAME: Scan finished: {} games added, {} identified", m_added, m_identified);
  CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetLibraryInfoProvider().ResetLibraryBools();

  if (m_handle != nullptr)
  {
    m_handle->MarkFinished();
    m_handle = nullptr;
  }

  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
}

std::pair<CInfoScanner::ScanComplete, CInfoScanner::ContentFound> CGameInfoScanner::DoScan(
    const std::string& strDirectory)
{
  if (!m_bRunning)
    return {ScanComplete::Stopped, ContentFound::None};

  if (HasNoMedia(strDirectory))
    return {ScanComplete::Completed, ContentFound::None};

  GamePathContent content;
  bool foundDirectly = false;
  if (!m_database.GetPathContent(strDirectory, content, foundDirectly) || !content.HasContent())
    return {ScanComplete::Completed, ContentFound::None};

  PlatformInfo platform;
  if (!m_database.GetPlatform(content.idPlatform, platform))
  {
    CLog::Log(LOGERROR, "GAME: {} is set to platform {} which the library does not know",
              strDirectory, content.idPlatform);
    return {ScanComplete::Completed, ContentFound::None};
  }

  const bool found = ScanFolder(strDirectory, content, platform);
  return {m_bRunning ? ScanComplete::Completed : ScanComplete::Stopped,
          found ? ContentFound::NewContentFound : ContentFound::None};
}

std::string CGameInfoScanner::FolderHash(const CFileItemList& items) const
{
  KODI::UTILITY::CDigest digest(KODI::UTILITY::CDigest::Type::MD5);
  for (const auto& item : items)
  {
    digest.Update(item->GetPath());
    digest.Update(std::to_string(item->GetSize()));
    digest.Update(item->GetDateTime().GetAsDBDateTime());
  }
  return digest.Finalize();
}

std::vector<CGameInfoScanner::Entry> CGameInfoScanner::GroupEntries(const CFileItemList& items,
                                                                    bool useFolderNames)
{
  std::vector<Entry> entries;
  std::set<std::string> claimed;

  std::vector<std::string> files;
  std::vector<std::string> folders;
  for (const auto& item : items)
  {
    if (item->IsFolder())
      folders.emplace_back(item->GetPath());
    else
      files.emplace_back(item->GetPath());
  }
  std::ranges::sort(files);

  // A disc set: the M3U is the game, the discs it names are its files
  for (const std::string& path : files)
  {
    if (StringUtils::ToLower(URIUtils::GetExtension(path)) != ".m3u")
      continue;
    Entry entry;
    entry.path = path;
    entry.folder = URIUtils::GetDirectory(path);
    entry.files.emplace_back(path);
    for (const std::string& disc : ReadM3U(path))
    {
      entry.files.emplace_back(disc);
      claimed.insert(disc);
    }
    claimed.insert(path);
    entries.emplace_back(std::move(entry));
  }

  // A cue or GDI sheet is the game; the tracks sharing its name are its files
  for (const std::string& path : files)
  {
    if (claimed.contains(path))
      continue;
    const std::string ext = StringUtils::ToLower(URIUtils::GetExtension(path));
    if (ext != ".cue" && ext != ".gdi")
      continue;

    Entry entry;
    entry.path = path;
    entry.folder = URIUtils::GetDirectory(path);
    entry.files.emplace_back(path);
    const std::string stem = Stem(path);
    for (const std::string& track : files)
    {
      if (track == path || claimed.contains(track) || !HasExtension(track, trackExtensions))
        continue;
      if (Stem(track).starts_with(stem))
      {
        entry.files.emplace_back(track);
        claimed.insert(track);
      }
    }
    claimed.insert(path);
    entries.emplace_back(std::move(entry));
  }

  // Everything else stands alone, unless its folder is the game
  for (const std::string& path : files)
  {
    if (claimed.contains(path))
      continue;
    if (useFolderNames)
      continue;
    Entry entry;
    entry.path = path;
    entry.folder = URIUtils::GetDirectory(path);
    entry.files.emplace_back(path);
    entries.emplace_back(std::move(entry));
  }

  if (useFolderNames)
  {
    for (const std::string& folder : folders)
    {
      Entry entry;
      entry.isFolder = true;
      entry.folder = folder;
      entries.emplace_back(std::move(entry));
    }
  }

  return entries;
}

void CGameInfoScanner::FillRequest(const Entry& entry,
                                   const ParsedGameName& parsed,
                                   const GameFile& identity,
                                   const PlatformInfo& platform,
                                   GameScrapeRequest& request)
{
  request.title = parsed.displayTitle;
  request.fileName = entry.isFolder ? URIUtils::GetFileName(entry.folder)
                                    : URIUtils::GetFileName(entry.path);
  request.platformSlug = platform.slug;
  request.platformIds = platform.providerIds;
  request.crc32 = identity.crc32;
  request.md5 = identity.md5;
  request.sha1 = identity.sha1;
  request.serial = identity.serial;
  request.raHash = identity.raHash;
  request.size = identity.size;
  request.regions = parsed.regions;
  request.languages = parsed.languages;
  request.year = parsed.year;
}

bool CGameInfoScanner::ScanFolder(const std::string& folder,
                                  const GamePathContent& content,
                                  const PlatformInfo& platform)
{
  if (m_extensions.empty() || m_extensions.find(platform.slug) == std::string::npos)
    m_extensions = ExtensionMask(platform);

  CFileItemList items;
  if (!XFILE::CDirectory::GetDirectory(folder, items, m_extensions,
                                       XFILE::DIR_FLAG_NO_FILE_DIRS | XFILE::DIR_FLAG_NO_FILE_INFO))
  {
    CLog::Log(LOGWARNING, "GAME: Cannot list {}", folder);
    return false;
  }
  items.Sort(SortBy::FILE, SortOrder::ASCENDING);

  if (m_handle != nullptr)
    m_handle->SetText(platform.name);

  // A root folder's own picture stands in for the platform until one is scraped
  bool foundDirectly = false;
  GamePathContent own;
  if (m_database.GetPathContent(folder, own, foundDirectly) && foundDirectly &&
      m_database.GetArtForItem(platform.id, MediaTypeGamePlatform, "thumb").empty())
  {
    for (const char* name : {"folder.jpg", "folder.png"})
    {
      const std::string picture = URIUtils::AddFileToFolder(folder, name);
      if (XFILE::CFile::Exists(picture, false))
      {
        m_database.SetArtForItem(platform.id, MediaTypeGamePlatform, "thumb", picture);
        break;
      }
    }
  }

  const std::string hash = FolderHash(items);
  std::string storedHash;
  m_database.GetPathHash(folder, storedHash);

  bool found = false;
  if (hash != storedHash)
  {
    const std::vector<Entry> entries = GroupEntries(items, content.useFolderNames);
    m_itemCount = static_cast<int>(entries.size());
    m_currentItem = 0;
    for (const Entry& entry : entries)
    {
      if (!m_bRunning)
        return found;
      if (m_handle != nullptr)
        m_handle->SetProgress(m_currentItem, m_itemCount);
      ++m_currentItem;
      found = ScanEntry(entry, content, platform) || found;
    }
    if (m_bRunning)
      m_database.SetPathHash(folder, hash);
  }
  else
  {
    CLog::Log(LOGDEBUG, "GAME: {} is unchanged", folder);
  }

  // Subfolders hold more games, unless each subfolder was itself a game
  if (content.scanRecursive && !content.useFolderNames)
  {
    for (const auto& item : items)
    {
      if (!m_bRunning)
        break;
      if (item->IsFolder())
      {
        const auto [complete, subFound] = DoScan(item->GetPath());
        found = found || subFound == ContentFound::NewContentFound;
      }
    }
  }

  return found;
}

bool CGameInfoScanner::ScanEntry(const Entry& entry,
                                 const GamePathContent& content,
                                 const PlatformInfo& platform)
{
  std::string playPath = entry.path;
  std::vector<std::string> files = entry.files;

  if (entry.isFolder)
  {
    CFileItemList inside;
    if (!XFILE::CDirectory::GetDirectory(entry.folder, inside, m_extensions,
                                         XFILE::DIR_FLAG_NO_FILE_DIRS | XFILE::DIR_FLAG_NO_FILE_INFO))
      return false;
    std::vector<Entry> parts = GroupEntries(inside, false);
    if (parts.empty())
      return false;
    // A sheet plays the folder; failing that, its first file
    auto sheet = std::ranges::find_if(parts, [](const Entry& e) { return HasExtension(e.path, sheetExtensions); });
    const Entry& first = sheet != parts.end() ? *sheet : parts.front();
    playPath = first.path;
    files.clear();
    for (const Entry& part : parts)
      files.insert(files.end(), part.files.begin(), part.files.end());
  }

  if (playPath.empty())
    return false;

  if (m_database.GetGameIdByFile(playPath) > 0)
    return false;

  const std::string nameSource =
      entry.isFolder ? URIUtils::GetFileName(URIUtils::GetDirectory(entry.folder + "x"))
                     : URIUtils::GetFileName(playPath);
  const ParsedGameName parsed = CGameNameParser::Parse(nameSource);
  if (parsed.displayTitle.empty())
    return false;

  if (m_handle != nullptr)
    m_handle->SetText(platform.name + " - " + parsed.displayTitle);

  GameFile identity;
  CGameFileIdentity::Identify(playPath, identity, platform.media);

  CGameInfoTag tag;
  tag.SetTitle(parsed.displayTitle);
  tag.SetPlatformId(platform.id);
  tag.SetPlatformSlug(platform.slug);
  tag.SetPlatform(platform.name);
  if (parsed.year > 0)
    tag.SetYear(static_cast<unsigned int>(parsed.year));
  if (!parsed.publisher.empty())
    tag.SetPublishers({parsed.publisher});
  if (parsed.hack)
    tag.SetCategory(GameCategory::HACK);
  else if (parsed.licence == Licence::HOMEBREW || parsed.licence == Licence::AFTERMARKET)
    tag.SetCategory(GameCategory::HOMEBREW);
  else if (parsed.status == ReleaseStatus::PROGRAM)
    tag.SetCategory(GameCategory::BIOS);

  GameRelease release;
  release.title = parsed.title;
  release.regions = parsed.regions;
  release.languages = parsed.languages;
  if (!parsed.translation.empty())
    release.languages.emplace_back(parsed.translation);
  release.revision = parsed.revision;
  release.status = parsed.status;
  release.licence = parsed.licence;
  release.alternate = parsed.alternate;
  release.dump = parsed.bad ? DumpStatus::BAD : (parsed.verified ? DumpStatus::VERIFIED : DumpStatus::UNKNOWN);
  release.serial = identity.serial;
  release.isDefault = true;
  int disc = 0;
  for (const std::string& file : files)
  {
    GameFile f = (file == playPath) ? identity : GameFile{};
    f.path = file;
    if (files.size() > 1 && HasExtension(file, sheetExtensions))
      f.disc = ++disc;
    release.files.emplace_back(std::move(f));
  }

  // A sidecar file beside the game says what it is; the scraper is not asked
  bool sidecar = false;
  {
    CFileItem sidecarItem(playPath, false);
    CGameInfoTag fromFile;
    if (CGameInfoTagLoader::HasNFO(sidecarItem) && CGameInfoTagLoader::Load(sidecarItem, fromFile) &&
        !fromFile.GetTitle().empty())
    {
      fromFile.SetPlatformId(platform.id);
      fromFile.SetPlatformSlug(platform.slug);
      fromFile.SetPlatform(platform.name);
      if (fromFile.GetYear() == 0)
        fromFile.SetYear(tag.GetYear());
      if (fromFile.GetCategory() == GameCategory::RETAIL)
        fromFile.SetCategory(tag.GetCategory());
      tag = fromFile;
      sidecar = true;
    }
  }

  // Ask the scraper set for this folder, or the default one
  CGameScraper* scraper = nullptr;
  const std::string scraperId = content.scraper;
  auto it = m_scrapers.find(scraperId);
  if (it == m_scrapers.end())
  {
    std::unique_ptr<CGameScraper> created =
        scraperId.empty() ? CGameScraper::CreateDefault() : CGameScraper::Create(scraperId);
    if (!created && !scraperId.empty())
    {
      CLog::Log(LOGWARNING, "GAME: Scraper {} is not available; using the default", scraperId);
      created = CGameScraper::CreateDefault();
    }
    if (created && !content.settings.empty())
      created->SetPathSettings(content.settings);
    it = m_scrapers.emplace(scraperId, std::move(created)).first;
  }
  scraper = it->second.get();

  KODI::ART::Artwork art;
  MatchMethod matchedBy = sidecar ? MatchMethod::SIDECAR : MatchMethod::NONE;
  std::string candidateId;

  if (scraper != nullptr && !sidecar)
  {
    GameScrapeRequest request;
    FillRequest(entry, parsed, identity, platform, request);

    const std::vector<GameScrapeCandidate> candidates = scraper->Find(request);
    const GameScrapeCandidate* chosen = nullptr;
    if (!candidates.empty())
    {
      const GameScrapeCandidate& best = candidates.front();
      if (best.matchedBy == MatchMethod::HASH || best.matchedBy == MatchMethod::SERIAL)
        chosen = &best;
      else if (candidates.size() == 1)
        chosen = &best;
    }

    if (chosen != nullptr)
    {
      std::map<std::string, std::vector<GameScrapeArt>> offered;
      CGameInfoTag scraped;
      if (scraper->GetDetails(chosen->id, request, scraped, offered))
      {
        matchedBy = chosen->matchedBy;
        candidateId = chosen->id;

        // What the file name said stays only where the scraper said nothing
        const std::vector<GameRelease> catalogueReleases = scraped.GetReleases();
        scraped.SetReleases({});
        scraped.SetPlatformId(platform.id);
        scraped.SetPlatformSlug(platform.slug);
        scraped.SetPlatform(platform.name);
        if (scraped.GetYear() == 0 && tag.GetYear() > 0)
          scraped.SetYear(tag.GetYear());
        if (scraped.GetPublishers().empty() && !tag.GetPublishers().empty())
          scraped.SetPublishers(tag.GetPublishers());
        if (scraped.GetCategory() == GameCategory::RETAIL && tag.GetCategory() != GameCategory::RETAIL)
          scraped.SetCategory(tag.GetCategory());
        tag = scraped;

        for (const GameRelease& known : catalogueReleases)
        {
          const bool sameDump = std::ranges::any_of(known.files, [&identity](const GameFile& f)
                                                    {
                                                      return (!f.crc32.empty() && f.crc32 == identity.crc32) ||
                                                             (!f.md5.empty() && f.md5 == identity.md5);
                                                    }) ||
                                (!known.serial.empty() && known.serial == identity.serial);
          if (sameDump)
          {
            if (!known.title.empty())
              release.title = known.title;
            if (!known.regions.empty())
              release.regions = known.regions;
            if (!known.languages.empty())
              release.languages = known.languages;
            if (!known.revision.empty())
              release.revision = known.revision;
            release.status = known.status;
            release.licence = known.licence;
            if (!known.releaseDate.empty())
              release.releaseDate = known.releaseDate;
            break;
          }
        }

        for (const auto& [type, pieces] : offered)
        {
          // The first piece whose region agrees with the dump, else the first
          const GameScrapeArt* pick = &pieces.front();
          for (const GameScrapeArt& piece : pieces)
          {
            if (!piece.region.empty() && std::ranges::find(release.regions, piece.region) != release.regions.end())
            {
              pick = &piece;
              break;
            }
          }
          art[type] = pick->url;
        }
        ++m_identified;
      }
    }
  }

  tag.SetMatchMethod(matchedBy);

  // Another dump of a game already in the library becomes one of its releases
  int idGame = -1;
  if (scraper != nullptr && !candidateId.empty())
    idGame = m_database.FindGameByUniqueId(platform.id, scraper->ID(), candidateId);
  if (idGame <= 0)
    idGame = m_database.FindGameByTitleKey(platform.id, CGameLibraryTypes::TitleKey(tag.GetTitle()));

  if (idGame > 0)
  {
    CGameInfoTag existing;
    if (m_database.GetGameInfo(idGame, existing))
    {
      std::vector<GameRelease> releases = existing.GetReleases();
      release.isDefault = false;

      // Another dump of a release already known joins it rather than adding a
      // release of its own; a verified dump goes first so it is what plays
      auto same = std::ranges::find_if(releases, [&release](const GameRelease& known)
                                       { return SameRelease(known, release); });
      if (same != releases.end())
      {
        for (GameFile& file : release.files)
        {
          if (std::ranges::any_of(same->files, [&file](const GameFile& f) { return f.path == file.path; }))
            continue;
          if (release.dump == DumpStatus::VERIFIED && same->dump != DumpStatus::VERIFIED)
            same->files.insert(same->files.begin(), file);
          else
            same->files.emplace_back(file);
        }
        if (release.dump == DumpStatus::VERIFIED)
          same->dump = DumpStatus::VERIFIED;
        if (same->serial.empty())
          same->serial = release.serial;
      }
      else
      {
        releases.emplace_back(release);
      }
      // The policy decides which release plays unless the user already chose
      if (existing.GetMatchMethod() != MatchMethod::MANUAL)
      {
        const int best = CReleasePolicy().PickDefault(releases);
        if (best >= 0)
        {
          for (GameRelease& r : releases)
            r.isDefault = false;
          releases[static_cast<size_t>(best)].isDefault = true;
          existing.SetDefaultReleaseId(releases[static_cast<size_t>(best)].id);
        }
      }
      existing.SetReleases(releases);
      if (existing.GetMatchMethod() == MatchMethod::NONE && matchedBy != MatchMethod::NONE)
      {
        // The newer dump identified what the earlier one could not
        const std::vector<GameRelease> keep = existing.GetReleases();
        const int keepDefault = existing.GetDefaultReleaseId();
        existing = tag;
        existing.SetDatabaseId(idGame);
        existing.SetReleases(keep);
        existing.SetDefaultReleaseId(keepDefault);
      }
      KODI::ART::Artwork existingArt;
      m_database.GetArtForItem(idGame, MediaTypeGame, existingArt);
      for (const auto& [type, url] : art)
      {
        if (!existingArt.contains(type))
          existingArt[type] = url;
      }
      return m_database.SetDetailsForGame(existing, existingArt) > 0;
    }
  }

  tag.SetReleases({release});
  const bool added = m_database.SetDetailsForGame(tag, art) > 0;
  if (added)
    ++m_added;
  return added;
}
