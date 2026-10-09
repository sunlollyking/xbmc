/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/GameManual.h"
#include "test/TestUtils.h"
#include "utils/URIUtils.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI::GAME;

TEST(TestGameManual, NormalisingDropsRegionAndRevisionTags)
{
  EXPECT_EQ(CGameManual::NormaliseName("Sonic The Hedgehog 2 (World) (Rev A)"),
            CGameManual::NormaliseName("Sonic The Hedgehog 2"));
}

TEST(TestGameManual, NormalisingDropsBracketedTags)
{
  EXPECT_EQ(CGameManual::NormaliseName("Zelda (Europe) [!]"), CGameManual::NormaliseName("Zelda"));
}

TEST(TestGameManual, NormalisingDropsBracedTags)
{
  // PC-8801 sets put a game's start-up settings in braces
  EXPECT_EQ(CGameManual::NormaliseName("Demon's Ring (Disk A) {V1 mode}"),
            CGameManual::NormaliseName("Demon's Ring (manual)"));
}

TEST(TestGameManual, NormalisingIgnoresCaseAndPunctuation)
{
  EXPECT_EQ(CGameManual::NormaliseName("Mega Man X2 - The Sequel!"),
            CGameManual::NormaliseName("mega man x2 the sequel"));
}

TEST(TestGameManual, NormalisingHandlesNestedTags)
{
  // A closing bracket inside a tag must not end the skip early
  EXPECT_EQ(CGameManual::NormaliseName("Game (Unl (Aftermarket))"),
            CGameManual::NormaliseName("Game"));
}

TEST(TestGameManual, NormalisingKeepsDifferentGamesApart)
{
  // The whole point of stopping at tag stripping: two different titles must
  // not collapse onto each other, or the wrong manual would open
  EXPECT_NE(CGameManual::NormaliseName("Sonic The Hedgehog 2 (USA)"),
            CGameManual::NormaliseName("Sonic The Hedgehog 3 (USA)"));
  EXPECT_NE(CGameManual::NormaliseName("Sonic The Hedgehog (USA)"),
            CGameManual::NormaliseName("Sonic The Hedgehog 2 (USA)"));
  EXPECT_NE(CGameManual::NormaliseName("Game (USA)"),
            CGameManual::NormaliseName("Game Manual (USA)"));
}

TEST(TestGameManual, NormalisingAnEmptyNameStaysEmpty)
{
  // An empty result must never match, or a folder of tag-only filenames would
  // resolve to the first one found
  EXPECT_TRUE(CGameManual::NormaliseName("").empty());
  EXPECT_TRUE(CGameManual::NormaliseName("(USA)").empty());
}

namespace
{
/*!
 * \brief A temporary folder to lay test files out in
 *
 * CreateTempFile() names its files randomly, which is no use for testing how
 * names are matched, so it is only used to find the temp directory. The files
 * that matter are made by hand beside it.
 */
class CManualFixture
{
public:
  CManualFixture()
  {
    XFILE::CFile* anchor = XBMC_CREATETEMPFILE("");
    m_root = URIUtils::AddFileToFolder(CXBMCTestUtils::Instance().TempFileDirectory(anchor),
                                       "gamemanualtest");
    XBMC_DELETETEMPFILE(anchor);

    XFILE::CDirectory::RemoveRecursive(m_root);
    XFILE::CDirectory::Create(m_root);
  }

  ~CManualFixture() { XFILE::CDirectory::RemoveRecursive(m_root); }

  //! Create an empty file in the fixture, and return its path
  std::string Touch(const std::string& name) const
  {
    const std::string path = URIUtils::AddFileToFolder(m_root, name);

    XFILE::CDirectory::Create(URIUtils::GetDirectory(path));

    XFILE::CFile file;
    if (file.OpenForWrite(path, true))
    {
      file.Write(" ", 1);
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
} // namespace

TEST(TestGameManual, IndexFindsAManualTaggedDifferentlyFromTheGame)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA) (Rev A).md");
  fixture.Touch("Game.pdf");

  CManualIndex index;
  EXPECT_TRUE(index.HasManual(fixture.Path("Game (USA) (Rev A).md")));
}

TEST(TestGameManual, IndexFindsAManualInTheManualsFolder)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  fixture.Touch("manuals/Game (USA).cbz");

  CManualIndex index;
  EXPECT_TRUE(index.HasManual(fixture.Path("Game (USA).md")));
}

TEST(TestGameManual, IndexFindsAManualInACapitalisedManualsFolder)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  fixture.Touch("Manuals/Game (USA).pdf");

  CManualIndex index;
  EXPECT_TRUE(index.HasManual(fixture.Path("Game (USA).md")));
}

TEST(TestGameManual, IndexReportsNothingWhenNoManualIsThere)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");

  CManualIndex index;
  EXPECT_FALSE(index.HasManual(fixture.Path("Game (USA).md")));
}

TEST(TestGameManual, IndexAnswersTheSameWayTwiceForOneFolder)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  fixture.Touch("Other (USA).md");
  fixture.Touch("Game.pdf");

  // The second call is served from what the first read, so it must not differ
  CManualIndex index;
  EXPECT_TRUE(index.HasManual(fixture.Path("Game (USA).md")));
  EXPECT_TRUE(index.HasManual(fixture.Path("Game (USA).md")));
  EXPECT_FALSE(index.HasManual(fixture.Path("Other (USA).md")));
}
