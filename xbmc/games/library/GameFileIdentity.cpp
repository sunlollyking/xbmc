/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameFileIdentity.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "utils/Crc32.h"
#include "utils/Digest.h"
#include "utils/RegExp.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr size_t CHUNK_SIZE = 1024 * 1024;

constexpr std::array<const char*, 2> archiveExtensions{".zip", ".7z"};
constexpr std::array<const char*, 2> sheetExtensions{".cue", ".gdi"};
constexpr std::array<const char*, 5> trackExtensions{".bin", ".img", ".iso", ".raw", ".mdf"};

constexpr std::string_view SATURN_MAGIC = "SEGA SEGASATURN";
constexpr std::string_view MEGACD_MAGIC = "SEGADISCSYSTEM";
constexpr std::string_view DREAMCAST_MAGIC = "SEGA SEGAKATANA";

bool HasExtension(const std::string& path, const auto& extensions)
{
  const std::string ext = StringUtils::ToLower(URIUtils::GetExtension(path));
  return std::ranges::any_of(extensions, [&ext](const char* e) { return ext == e; });
}

std::string ReadHead(const std::string& path, size_t bytes)
{
  XFILE::CFile file;
  if (!file.Open(path, XFILE::READ_NO_CACHE))
    return "";

  std::string data;
  data.resize(bytes);
  size_t got = 0;
  while (got < bytes)
  {
    const ssize_t n = file.Read(data.data() + got, bytes - got);
    if (n <= 0)
      break;
    got += static_cast<size_t>(n);
  }
  data.resize(got);
  return data;
}

std::string HeaderField(const std::string& data, size_t offset, size_t length)
{
  if (offset >= data.size())
    return "";
  std::string value = data.substr(offset, std::min(length, data.size() - offset));
  const size_t nul = value.find('\0');
  if (nul != std::string::npos)
    value.erase(nul);
  StringUtils::Trim(value);
  return value;
}
} // namespace

std::string CGameFileIdentity::NormaliseSerial(std::string serial)
{
  StringUtils::Trim(serial);
  if (serial.starts_with("GM "))
    serial.erase(0, 3);
  // Saturn appends a version; Mega-CD a region suffix. The first word is the serial.
  const size_t space = serial.find_first_of(" \t");
  if (space != std::string::npos)
    serial.erase(space);

  std::string out;
  for (const char c : serial)
  {
    if (std::isalnum(static_cast<unsigned char>(c)))
      out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

std::string CGameFileIdentity::FirstTrack(const std::string& sheetPath)
{
  const std::string sheet = ReadHead(sheetPath, 64 * 1024);
  if (sheet.empty())
    return "";

  const std::string folder = URIUtils::GetDirectory(sheetPath);
  const std::string ext = StringUtils::ToLower(URIUtils::GetExtension(sheetPath));

  if (ext == ".gdi")
  {
    // Line: <track> <lba> <type> <sector size> <file> <offset>; track 3 holds the data
    for (const std::string& line : StringUtils::Split(sheet, '\n'))
    {
      std::string trimmed = line;
      StringUtils::Trim(trimmed);
      std::vector<std::string> parts = StringUtils::Split(trimmed, ' ');
      std::erase_if(parts, [](const std::string& p) { return p.empty(); });
      if (parts.size() >= 5 && parts[0] == "3")
      {
        std::string file = parts[4];
        StringUtils::Trim(file, "\"");
        return URIUtils::AddFileToFolder(folder, file);
      }
    }
    return "";
  }

  CRegExp re(true);
  if (re.RegComp("FILE\\s+\"([^\"]+)\"") && re.RegFind(sheet) >= 0)
    return URIUtils::AddFileToFolder(folder, re.GetMatch(1));
  if (re.RegComp("FILE\\s+(\\S+)") && re.RegFind(sheet) >= 0)
    return URIUtils::AddFileToFolder(folder, re.GetMatch(1));
  return "";
}

std::string CGameFileIdentity::ReadDiscSerial(const std::string& trackPath)
{
  const std::string head = ReadHead(trackPath, DISC_HEADER_SIZE);
  if (head.empty())
    return "";

  // The system area sits at the start of the first track, possibly behind a
  // 16-byte raw sector header
  const std::string_view start(head.data(), std::min<size_t>(head.size(), 0x200));

  if (const size_t at = start.find(SATURN_MAGIC); at != std::string_view::npos && at <= 0x10)
    return NormaliseSerial(HeaderField(head, at + 0x20, 0x10));

  if (const size_t at = start.find(DREAMCAST_MAGIC); at != std::string_view::npos && at <= 0x10)
    return NormaliseSerial(HeaderField(head, at + 0x40, 0x0A));

  if (const size_t at = start.find(MEGACD_MAGIC); at != std::string_view::npos && at <= 0x10)
  {
    std::string serial = HeaderField(head, at + 0x180, 0x10);
    if (serial.starts_with("GM "))
      serial.erase(0, 3);
    const size_t suffix = serial.find("-00");
    if (suffix != std::string::npos)
      serial.erase(suffix);
    return NormaliseSerial(serial);
  }

  // PlayStation: BOOT = cdrom:\SLUS_003.00;1 in SYSTEM.CNF
  CRegExp re(true);
  if (re.RegComp("BOOT\\s*=\\s*cdrom:\\\\?([A-Z]{4})[_-]?([0-9]{3})\\.?([0-9]{2})") &&
      re.RegFind(head) >= 0)
    return NormaliseSerial(re.GetMatch(1) + re.GetMatch(2) + re.GetMatch(3));

  return "";
}

bool CGameFileIdentity::HashWhole(const std::string& path, GameFile& file)
{
  XFILE::CFile in;
  if (!in.Open(path, XFILE::READ_NO_CACHE))
    return false;

  Crc32 crc;
  KODI::UTILITY::CDigest md5(KODI::UTILITY::CDigest::Type::MD5);
  std::vector<char> buffer(CHUNK_SIZE);
  uint64_t total = 0;

  while (true)
  {
    const ssize_t n = in.Read(buffer.data(), buffer.size());
    if (n < 0)
      return false;
    if (n == 0)
      break;
    crc.Compute(buffer.data(), static_cast<size_t>(n));
    md5.Update(buffer.data(), static_cast<size_t>(n));
    total += static_cast<uint64_t>(n);
    if (total > MAX_HASH_SIZE)
      return false;
  }

  file.crc32 = StringUtils::Format("{:08x}", static_cast<uint32_t>(crc));
  file.md5 = StringUtils::ToLower(md5.Finalize());
  if (file.size == 0)
    file.size = total;
  return true;
}

bool CGameFileIdentity::HashArchive(const std::string& path, GameFile& file)
{
  // The archive is browsed as a folder; its largest member is the ROM
  const std::string archiveUrl = URIUtils::CreateArchivePath(
      StringUtils::ToLower(URIUtils::GetExtension(path)) == ".zip" ? "zip" : "archive", CURL(path),
      "").Get();

  CFileItemList members;
  if (!XFILE::CDirectory::GetDirectory(archiveUrl, members, "", XFILE::DIR_FLAG_DEFAULTS))
    return false;

  std::shared_ptr<CFileItem> best;
  for (const auto& member : members)
  {
    if (member->IsFolder())
      continue;
    if (!best || member->GetSize() > best->GetSize())
      best = member;
  }
  if (!best || best->GetSize() > static_cast<int64_t>(MAX_HASH_SIZE))
    return false;

  GameFile member;
  member.size = static_cast<uint64_t>(best->GetSize());
  if (!HashWhole(best->GetPath(), member))
    return false;

  file.crc32 = member.crc32;
  file.md5 = member.md5;
  file.size = member.size;
  return true;
}

bool CGameFileIdentity::Identify(const std::string& path, GameFile& file, MediaFormat media)
{
  file.path = path;

  XFILE::CFile probe;
  struct __stat64 st = {};
  if (probe.Stat(path, &st) == 0)
    file.size = static_cast<uint64_t>(st.st_size);

  try
  {
    if (HasExtension(path, sheetExtensions))
    {
      const std::string track = FirstTrack(path);
      if (!track.empty())
        file.serial = ReadDiscSerial(track);
      return !file.serial.empty();
    }

    if (HasExtension(path, archiveExtensions))
      return HashArchive(path, file);

    if (media == MediaFormat::DISC || file.size > MAX_HASH_SIZE)
    {
      if (HasExtension(path, trackExtensions) || file.size > MAX_HASH_SIZE)
      {
        file.serial = ReadDiscSerial(path);
        return !file.serial.empty();
      }
    }

    return HashWhole(path, file);
  }
  catch (...)
  {
    CLog::Log(LOGWARNING, "GAME: Could not identify {}", path);
  }
  return false;
}
