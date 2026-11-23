/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItemList.h"
#include "Util.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "games/library/GameInfoScanner.h"
#include "utils/URIUtils.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
class TestGameFolderGames : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_root = URIUtils::AddFileToFolder(CSpecialProtocol::TranslatePath("special://temp/"),
                                       "foldergames/");
    XFILE::CDirectory::RemoveRecursive(m_root);
    ASSERT_TRUE(CUtil::CreateDirectoryEx(m_root));
  }

  void TearDown() override { XFILE::CDirectory::RemoveRecursive(m_root); }

  void Write(const std::string& relativePath, const std::string& content = "x")
  {
    const std::string path = URIUtils::AddFileToFolder(m_root, relativePath);
    ASSERT_TRUE(CUtil::CreateDirectoryEx(URIUtils::GetDirectory(path)));
    XFILE::CFile file;
    ASSERT_TRUE(file.OpenForWrite(path, true));
    ASSERT_EQ(file.Write(content.data(), content.size()), static_cast<ssize_t>(content.size()));
  }

  std::vector<CGameInfoScanner::Entry> FolderGames(const PlatformInfo& platform)
  {
    CFileItemList items;
    EXPECT_TRUE(
        XFILE::CDirectory::GetDirectory(m_root, items, "", XFILE::DIR_FLAG_BYPASS_CACHE));
    return CGameInfoScanner::FolderGames(items, platform);
  }

  std::string m_root;
};

PlatformInfo Platform(std::vector<std::string> extensions)
{
  PlatformInfo platform;
  platform.extensions = std::move(extensions);
  return platform;
}

std::string TitleXml(const std::string& root, const std::string& titleId)
{
  return "<?xml version=\"1.0\" encoding=\"utf-8\"?><" + root +
         " type=\"complex\" access=\"777\"><title_id type=\"hexBinary\" length=\"8\">" +
         titleId + "</title_id></" + root + ">";
}
} // namespace

TEST_F(TestGameFolderGames, PlaysASplitImageFromItsFirstPart)
{
  Write("Nested.xci/Nested (Certificate).bin");
  Write("Nested.xci/Nested.xci/00");
  Write("Nested.xci/Nested.xci/01");
  Write("Flat.nsp/00");

  const auto games = FolderGames(Platform({"nsp", "xci"}));
  ASSERT_EQ(games.size(), 2u);
  for (const auto& game : games)
  {
    EXPECT_TRUE(game.nameFromFolder);
    EXPECT_TRUE(game.folderIsGame);
    EXPECT_TRUE(game.folderIsImage);
    ASSERT_EQ(game.files.size(), 1u);
    EXPECT_EQ(game.files.front(), game.path);
  }
  const auto flat = games[0].folder.ends_with("Flat.nsp/") ? games[0] : games[1];
  const auto nested = games[0].folder.ends_with("Nested.xci/") ? games[0] : games[1];
  EXPECT_EQ(flat.path, URIUtils::AddFileToFolder(m_root, "Flat.nsp/00"));
  EXPECT_EQ(nested.path, URIUtils::AddFileToFolder(m_root, "Nested.xci/Nested.xci/00"));
}

TEST_F(TestGameFolderGames, NeedsTheFirstPartAndTheImageName)
{
  Write("Empty.xci/readme.txt");
  Write("Unnamed/00");
  Write("Other.bin/00");

  EXPECT_TRUE(FolderGames(Platform({"nsp", "xci"})).empty());
}

TEST_F(TestGameFolderGames, PlaysAnUnpackedTitleFromItsProgram)
{
  Write("Title [Game]/code/game.rpx");
  Write("Title [Game]/code/library.rpl");
  Write("Title [Game]/code/app.xml", TitleXml("app", "000500001019E500"));
  Write("Title [Game]/meta/meta.xml", TitleXml("menu", "000500001019E500"));
  // An update can carry its game's meta.xml; its app.xml says what it is
  Write("Title [Update]/code/game.rpx");
  Write("Title [Update]/code/app.xml", TitleXml("app", "0005000E1019E500"));
  Write("Title [Update]/meta/meta.xml", TitleXml("menu", "000500001019E500"));
  Write("Title [DLC]/code/game.rpx");
  Write("Title [DLC]/meta/meta.xml", TitleXml("menu", "0005000C1019E500"));

  auto games = FolderGames(Platform({"rpx", "wud"}));
  // The update and DLC are no games, but nothing in them is to be scanned either
  ASSERT_EQ(games.size(), 3u);
  EXPECT_EQ(std::erase_if(games, [](const auto& game) { return game.path.empty(); }), 2u);
  ASSERT_EQ(games.size(), 1u);
  EXPECT_EQ(games.front().path, URIUtils::AddFileToFolder(m_root, "Title [Game]/code/game.rpx"));
  EXPECT_EQ(games.front().folder, URIUtils::AddFileToFolder(m_root, "Title [Game]/"));
  EXPECT_TRUE(games.front().nameFromFolder);
  EXPECT_TRUE(games.front().folderIsGame);
  EXPECT_FALSE(games.front().folderIsImage);
}

TEST_F(TestGameFolderGames, PlaysACopiedDiscFromItsProgram)
{
  Write("Disc Game/PS3_DISC.SFB");
  Write("Disc Game/PS3_GAME/PARAM.SFO");
  Write("Disc Game/PS3_GAME/USRDIR/EBOOT.BIN");
  Write("Disc Game/PS3_GAME/USRDIR/DATA.BIN");

  const auto games = FolderGames(Platform({"iso", "ps3"}));
  ASSERT_EQ(games.size(), 1u);
  EXPECT_EQ(games.front().path, URIUtils::AddFileToFolder(m_root, "Disc Game/PS3_DISC.SFB"));
  EXPECT_TRUE(games.front().folderIsGame);
  EXPECT_FALSE(games.front().folderIsImage);
}

TEST_F(TestGameFolderGames, FindsTheFolderGameAFilePlays)
{
  Write("Title [Game]/code/game.rpx");
  Write("Title [Game]/meta/meta.xml", TitleXml("menu", "000500001019E500"));
  Write("Nested.xci/Nested.xci/00");
  Write("Loose/game.rpx");

  const PlatformInfo platform = Platform({"rpx", "xci"});
  const auto title =
      CGameInfoScanner::FolderGameOf(URIUtils::AddFileToFolder(m_root, "Title [Game]/code/game.rpx"), platform);
  ASSERT_TRUE(title.has_value());
  EXPECT_EQ(title->folder, URIUtils::AddFileToFolder(m_root, "Title [Game]/"));

  const auto split = CGameInfoScanner::FolderGameOf(
      URIUtils::AddFileToFolder(m_root, "Nested.xci/Nested.xci/00"), platform);
  ASSERT_TRUE(split.has_value());
  EXPECT_TRUE(split->folderIsImage);

  EXPECT_FALSE(
      CGameInfoScanner::FolderGameOf(URIUtils::AddFileToFolder(m_root, "Loose/game.rpx"), platform)
          .has_value());
}
