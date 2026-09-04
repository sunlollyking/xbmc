/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItemList.h"
#include "filesystem/Directory.h"
#include "games/library/GameFileIdentity.h"
#include "test/TestUtils.h"
#include "utils/URIUtils.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
// identity.zip holds a single 6 MB member, deflated. The size matters: the zip
// backend copies a deflated member over 4 MB into special://temp unless the read
// asks it not to, and nothing ever deletes that copy.
constexpr auto ARCHIVE = "xbmc/games/library/test/identity.zip";

// The CRC-32 that PKZIP stores and the ROM catalogues index by
constexpr auto MEMBER_CRC32 = "eeea5a12";
constexpr auto MEMBER_MD5 = "6835177061dbf19a6a4436a16dbb6cd7";
constexpr uint64_t MEMBER_SIZE = 6 * 1024 * 1024;

std::vector<std::string> TempFileNames()
{
  CFileItemList items;
  XFILE::CDirectory::GetDirectory("special://temp/", items, "", XFILE::DIR_FLAG_NO_FILE_DIRS);

  std::vector<std::string> names;
  for (const auto& item : items)
  {
    if (!item->IsFolder())
      names.emplace_back(URIUtils::GetFileName(item->GetPath()));
  }
  return names;
}
} // namespace

TEST(TestGameFileIdentity, HashesTheGameInsideAnArchive)
{
  GameFile file;
  ASSERT_TRUE(
      CGameFileIdentity::Identify(XBMC_REF_FILE_PATH(ARCHIVE), file, MediaFormat::CARTRIDGE));

  // The hash is of the member, not of the archive around it
  EXPECT_EQ(file.crc32, MEMBER_CRC32);
  EXPECT_EQ(file.md5, MEMBER_MD5);
  EXPECT_EQ(file.size, MEMBER_SIZE);
}

TEST(TestGameFileIdentity, ReadsAnArchivedGameWithoutExtractingIt)
{
  const std::vector<std::string> before = TempFileNames();

  GameFile file;
  ASSERT_TRUE(
      CGameFileIdentity::Identify(XBMC_REF_FILE_PATH(ARCHIVE), file, MediaFormat::CARTRIDGE));

  EXPECT_EQ(TempFileNames(), before);
}
