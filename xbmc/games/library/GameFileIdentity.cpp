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
#include "utils/Digest.h"
#include "utils/RegExp.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <zlib.h>
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

//! How far into a disc image its header is looked for before giving up
constexpr uint64_t MAX_DISC_SCAN = 2ULL * 1024 * 1024 * 1024;

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

std::string CGameFileIdentity::SerialFromHeader(const std::string& data, size_t at, int console)
{
  // 0: Dreamcast, 1: Saturn, 2: Mega-CD
  if (console == 0)
  {
    // "MK-51171" or "T40201N", sometimes with a second field after a space
    std::string serial = HeaderField(data, at + 0x40, 0x10);
    const size_t space = serial.find(' ');
    if (space != std::string::npos)
      serial.erase(space);
    return NormaliseSerial(serial);
  }

  if (console == 1)
    return NormaliseSerial(HeaderField(data, at + 0x20, 0x10));

  std::string serial = HeaderField(data, at + 0x180, 0x10);
  if (serial.starts_with("GM "))
    serial.erase(0, 3);
  const size_t suffix = serial.find("-00");
  if (suffix != std::string::npos)
    serial.erase(suffix);
  return NormaliseSerial(serial);
}

std::string CGameFileIdentity::ReadDiscSerial(const std::string& trackPath)
{
  XFILE::CFile file;
  if (!file.Open(trackPath, XFILE::READ_NO_CACHE))
    return "";

  // The system area sits at the start of a plain track, but a container such
  // as DiscJuggler's puts the data track wherever it lands: measured on real
  // images, anywhere from 1 MB to 460 MB in. So the file is read through until
  // the header appears and reading stops the moment it does, which is no
  // dearer than the hashing this replaces and usually far cheaper.
  static constexpr std::array<std::pair<std::string_view, int>, 3> magics{{
      {DREAMCAST_MAGIC, 0},
      {SATURN_MAGIC, 1},
      {MEGACD_MAGIC, 2},
  }};

  // Enough of the last chunk is kept for a header that straddles the join
  static constexpr size_t OVERLAP = 0x400;

  std::vector<char> buffer(CHUNK_SIZE);
  std::string window;
  uint64_t read = 0;

  while (read < MAX_DISC_SCAN)
  {
    const ssize_t n = file.Read(buffer.data(), buffer.size());
    if (n <= 0)
      break;
    read += static_cast<uint64_t>(n);
    window.append(buffer.data(), static_cast<size_t>(n));

    for (const auto& [magic, console] : magics)
    {
      const size_t at = window.find(magic);
      if (at == std::string::npos)
        continue;
      const std::string serial = SerialFromHeader(window, at, console);
      if (!serial.empty())
        return serial;
    }

    // PlayStation names itself in text rather than in a header
    CRegExp re(true);
    if (re.RegComp("BOOT\\s*=\\s*cdrom:\\\\?([A-Z]{4})[_-]?([0-9]{3})\\.?([0-9]{2})") &&
        re.RegFind(window) >= 0)
      return NormaliseSerial(re.GetMatch(1) + re.GetMatch(2) + re.GetMatch(3));

    if (window.size() > OVERLAP)
      window.erase(0, window.size() - OVERLAP);
  }

  return "";
}

bool CGameFileIdentity::HashWhole(const std::string& path, GameFile& file)
{
  XFILE::CFile in;
  if (!in.Open(path, XFILE::READ_NO_CACHE))
    return false;

  // The catalogues key ROMs by the CRC-32 that PKZIP and the No-Intro sets use.
  // Kodi's own Crc32 is the non-reflected variant and matches none of them.
  uLong crc = crc32(0UL, nullptr, 0);
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
    crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), static_cast<uInt>(n));
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

  // The members are wanted as files. Letting Kodi convert them to directories
  // offers each one to every VFS add-on in turn to ask whether it is itself an
  // archive, so one bad add-on takes the whole scan down with it: vfs.rar
  // segfaults on some names, and a library scan is thousands of chances to hit
  // one. Nothing here needs a nested archive opened, only the member list.
  CFileItemList members;
  if (!XFILE::CDirectory::GetDirectory(archiveUrl, members, "",
                                       XFILE::DIR_FLAG_NO_FILE_DIRS |
                                           XFILE::DIR_FLAG_NO_FILE_INFO))
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

  // Stream the member through the decompressor rather than letting the zip
  // backend copy it whole into special://temp first, which it does for anything
  // deflated over 4 MB and never cleans up. Hashing is one pass front to back,
  // so nothing here needs to seek.
  CURL memberUrl(best->GetPath());
  memberUrl.SetOptions("?cache=no");

  GameFile member;
  member.size = static_cast<uint64_t>(best->GetSize());
  if (!HashWhole(memberUrl.Get(), member))
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

    // A disc is known by the serial printed in its own header. Hashing the
    // container matches nothing: the catalogues key discs by serial, and every
    // rip of one game differs by however it was made.
    if (media == MediaFormat::DISC || file.size > MAX_HASH_SIZE)
    {
      file.serial = ReadDiscSerial(path);
      return !file.serial.empty();
    }

    return HashWhole(path, file);
  }
  catch (...)
  {
    CLog::Log(LOGWARNING, "GAME: Could not identify {}", path);
  }
  return false;
}
