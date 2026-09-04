/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/tags/GameInfoTag.h"
#include "games/tags/GameInfoTagLoader.h"
#include "test/TestUtils.h"
#include "utils/URIUtils.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
/*!
 * \brief A temporary folder to lay NFO files out in
 *
 * The names matter, since an NFO is found by the name of the thing it
 * describes, so the files are made by hand rather than by CreateTempFile().
 */
class CNFOFixture
{
public:
  CNFOFixture()
  {
    XFILE::CFile* anchor = XBMC_CREATETEMPFILE("");
    m_root = URIUtils::AddFileToFolder(CXBMCTestUtils::Instance().TempFileDirectory(anchor),
                                       "gamenfotest");
    XBMC_DELETETEMPFILE(anchor);

    XFILE::CDirectory::RemoveRecursive(m_root);
    XFILE::CDirectory::Create(m_root);
  }

  ~CNFOFixture() { XFILE::CDirectory::RemoveRecursive(m_root); }

  //! Write a file into the fixture, creating any directory it sits in
  std::string Write(const std::string& name, const std::string& contents) const
  {
    const std::string path = Path(name);

    XFILE::CDirectory::Create(URIUtils::GetDirectory(path));

    XFILE::CFile file;
    if (file.OpenForWrite(path, true))
    {
      file.Write(contents.c_str(), contents.size());
      file.Close();
    }

    return path;
  }

  std::string Path(const std::string& name) const
  {
    return URIUtils::AddFileToFolder(m_root, name);
  }

private:
  std::string m_root;
};

constexpr auto FULL_NFO = R"(<game>
  <title>Super Test Bros.</title>
  <overview>A game that exists to be parsed.</overview>
  <platform>Nintendo 64</platform>
  <developer>Team Kodi</developer>
  <publisher>Nobody</publisher>
  <region>PAL</region>
  <year>1996</year>
  <genre>Platformer</genre>
  <genre>Test</genre>
</game>)";
} // namespace

TEST(TestGameInfoTagLoader, ReadsTheNFOBesideAGame)
{
  //
  // Spec: a game is described by <name>.nfo sitting beside it
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", FULL_NFO);
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  ASSERT_TRUE(CGameInfoTagLoader::HasNFO(item));
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, tag));

  EXPECT_EQ(tag.GetTitle(), "Super Test Bros.");
  EXPECT_EQ(tag.GetOverview(), "A game that exists to be parsed.");
  EXPECT_EQ(tag.GetPlatform(), "Nintendo 64");
  EXPECT_EQ(tag.GetDeveloper(), "Team Kodi");
  EXPECT_EQ(tag.GetPublisher(), "Nobody");
  EXPECT_EQ(tag.GetRegion(), "PAL");
  EXPECT_EQ(tag.GetYear(), 1996u);
  ASSERT_EQ(tag.GetGenres().size(), 2u);
  EXPECT_EQ(tag.GetGenres()[0], "Platformer");
  EXPECT_EQ(tag.GetGenres()[1], "Test");
}

TEST(TestGameInfoTagLoader, ReadsTheNFOInsideAFolder)
{
  //
  // Spec: a folder is described by folder.nfo inside it, so a platform folder
  // can describe the system it holds
  //
  CNFOFixture fixture;
  fixture.Write("Nintendo 64/folder.nfo", FULL_NFO);
  const CFileItem item(fixture.Path("Nintendo 64/"), true);

  CGameInfoTag tag;
  ASSERT_TRUE(CGameInfoTagLoader::HasNFO(item));
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, tag));

  EXPECT_EQ(tag.GetTitle(), "Super Test Bros.");
  EXPECT_EQ(tag.GetPlatform(), "Nintendo 64");
}

TEST(TestGameInfoTagLoader, AcceptsPlotWhereThereIsNoOverview)
{
  //
  // Spec: plot is what every other NFO in Kodi calls it
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", "<game><plot>Written for the other kind of NFO.</plot></game>");
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_EQ(tag.GetOverview(), "Written for the other kind of NFO.");
}

TEST(TestGameInfoTagLoader, PrefersOverviewToPlot)
{
  //
  // Spec: an NFO carrying both is answered with the game-specific one
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", "<game><overview>Chosen</overview><plot>Ignored</plot></game>");
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_EQ(tag.GetOverview(), "Chosen");
}

TEST(TestGameInfoTagLoader, AnItemWithNoNFOIsNotRead)
{
  //
  // Spec: most of a listing has no NFO, and asking must not read or create one
  //
  CNFOFixture fixture;
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  EXPECT_FALSE(CGameInfoTagLoader::HasNFO(item));
  EXPECT_FALSE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_TRUE(tag.GetTitle().empty());
}

TEST(TestGameInfoTagLoader, MalformedXMLIsRefused)
{
  //
  // Spec: a hand-written file is allowed to be broken, and must not take the
  // listing with it
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", "<game><title>Unclosed");
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  EXPECT_TRUE(CGameInfoTagLoader::HasNFO(item));
  EXPECT_FALSE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_TRUE(tag.GetTitle().empty());
}

TEST(TestGameInfoTagLoader, AnotherKindOfNFOIsRefused)
{
  //
  // Spec: a folder holding a movie NFO is not describing a game
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", "<movie><title>Not a game</title></movie>");
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  EXPECT_FALSE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_TRUE(tag.GetTitle().empty());
}

TEST(TestGameInfoTagLoader, AYearThatIsNotANumberIsLeftAlone)
{
  //
  // Spec: the year is the one field that is not a string, so it is the one
  // that can be given something unusable
  //
  CNFOFixture fixture;
  fixture.Write("game.nfo", "<game><title>Kept</title><year>nineteen ninety six</year></game>");
  const CFileItem item(fixture.Path("game.n64"), false);

  CGameInfoTag tag;
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, tag));
  EXPECT_EQ(tag.GetTitle(), "Kept");
  EXPECT_EQ(tag.GetYear(), 0u);
}

TEST(TestGameInfoTagLoader, WhatIsWrittenIsWhatIsReadBack)
{
  //
  // Spec: an exported game survives the trip through XML
  //

  CGameInfoTag written;
  written.SetTitle("Super Test Bros.");
  written.SetOriginalTitle("スーパーテストブラザーズ");
  written.SetSortTitle("Super Test Bros");
  written.SetPlatform("Nintendo 64");
  written.SetOverview("A game that exists to be written.");
  written.SetYear(1996);
  written.SetReleaseDate("1996-06-23");
  written.SetRegion("PAL");
  written.SetGenres({"Platformer", "Test"});
  written.SetDevelopers({"Team Kodi", "Someone Else"});
  written.SetPublishers({"Nobody"});
  written.SetCollections({"Test Bros."});
  written.SetTags({"verified"});
  written.SetPlayers(1, 4);
  written.SetCoop(true);
  written.SetUniqueID("igdb", "1234", true);
  written.SetUniqueID("thegamesdb", "99");
  written.SetPlayCount(7);
  written.SetLastPlayed("2026-09-04 12:00:00");
  written.SetUserRating(9);
  written.SetFavourite(true);
  written.SetCompleted(true);
  written.SetAchievements(25, 3, 1);

  CNFOFixture fixture;
  const std::string game = fixture.Path("roundtrip.n64");
  CFileItem item(game, false);
  ASSERT_TRUE(CGameInfoTagLoader::Save(item, written));

  CGameInfoTag read;
  ASSERT_TRUE(CGameInfoTagLoader::Load(item, read));

  EXPECT_EQ(read.GetTitle(), written.GetTitle());
  EXPECT_EQ(read.GetOriginalTitle(), written.GetOriginalTitle());
  EXPECT_EQ(read.GetSortTitle(), written.GetSortTitle());
  EXPECT_EQ(read.GetPlatform(), written.GetPlatform());
  EXPECT_EQ(read.GetOverview(), written.GetOverview());
  EXPECT_EQ(read.GetYear(), written.GetYear());
  EXPECT_EQ(read.GetReleaseDate(), written.GetReleaseDate());
  EXPECT_EQ(read.GetRegion(), written.GetRegion());
  EXPECT_EQ(read.GetGenres(), written.GetGenres());
  EXPECT_EQ(read.GetDevelopers(), written.GetDevelopers());
  EXPECT_EQ(read.GetPublishers(), written.GetPublishers());
  EXPECT_EQ(read.GetCollections(), written.GetCollections());
  EXPECT_EQ(read.GetTags(), written.GetTags());
  EXPECT_EQ(read.GetPlayersMin(), 1);
  EXPECT_EQ(read.GetPlayersMax(), 4);
  EXPECT_TRUE(read.IsCoop());
  EXPECT_EQ(read.GetUniqueID("igdb"), "1234");
  EXPECT_EQ(read.GetUniqueID("thegamesdb"), "99");
  EXPECT_EQ(read.GetPlayCount(), 7);
  EXPECT_EQ(read.GetLastPlayed(), written.GetLastPlayed());
  EXPECT_EQ(read.GetUserRating(), 9);
  EXPECT_TRUE(read.IsFavourite());
  EXPECT_TRUE(read.IsCompleted());
  EXPECT_EQ(read.GetAchievementsTotal(), 25);
  EXPECT_EQ(read.GetAchievementsEarned(), 3);
  EXPECT_EQ(read.GetAchievementsHardcore(), 1);
}

TEST(TestGameInfoTagLoader, WritingLeavesOutWhatTheGameDoesNotHave)
{
  //
  // Spec: an empty field is absent rather than written empty, so an NFO says
  // only what is known
  //

  CGameInfoTag tag;
  tag.SetTitle("Bare");

  CNFOFixture fixture;
  const std::string game = fixture.Path("bare.n64");
  ASSERT_TRUE(CGameInfoTagLoader::Save(CFileItem(game, false), tag));

  XFILE::CFile file;
  ASSERT_TRUE(file.Open(URIUtils::ReplaceExtension(game, ".nfo")));
  std::vector<uint8_t> buffer(static_cast<size_t>(file.GetLength()));
  file.Read(buffer.data(), buffer.size());
  const std::string contents(buffer.begin(), buffer.end());

  EXPECT_NE(contents.find("<title>Bare</title>"), std::string::npos);
  EXPECT_EQ(contents.find("<overview>"), std::string::npos);
  EXPECT_EQ(contents.find("<year>"), std::string::npos);
  EXPECT_EQ(contents.find("<favourite>"), std::string::npos);
  EXPECT_EQ(contents.find("<achievements"), std::string::npos);
}
