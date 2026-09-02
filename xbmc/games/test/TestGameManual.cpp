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

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI::GAME;

namespace
{
bool Contains(const std::vector<std::string>& paths, const std::string& path)
{
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

//! Where a path sits in the candidate list, or -1 if it isn't there
int IndexOf(const std::vector<std::string>& paths, const std::string& path)
{
  const auto it = std::find(paths.begin(), paths.end(), path);
  if (it == paths.end())
    return -1;

  return static_cast<int>(std::distance(paths.begin(), it));
}
} // namespace

// The candidates are derived without touching the filesystem, so these cases
// can be stated directly. Whether any of the files are actually there is a
// separate question, answered by GetManualPath().

TEST(TestGameManual, OffersEveryManualFormatBesideTheGame)
{
  const std::vector<std::string> paths = CGameManual::BuildManualPaths("/roms/Game.rom");

  EXPECT_TRUE(Contains(paths, "/roms/Game.pdf"));
  EXPECT_TRUE(Contains(paths, "/roms/Game.cbz"));
  EXPECT_TRUE(Contains(paths, "/roms/Game.cbr"));
}

TEST(TestGameManual, PrefersAPdfOverAComicArchive)
{
  // A PDF has real page structure; a comic archive is a bag of images that has
  // to be ordered by filename, so it is the fallback
  const std::vector<std::string> paths = CGameManual::BuildManualPaths("/roms/Game.rom");

  EXPECT_LT(IndexOf(paths, "/roms/Game.pdf"), IndexOf(paths, "/roms/Game.cbz"));
}

TEST(TestGameManual, LooksInTheManualsSubfolder)
{
  const std::vector<std::string> paths = CGameManual::BuildManualPaths("/roms/Game.rom");

  EXPECT_TRUE(Contains(paths, "/roms/manuals/Game.pdf"));
  EXPECT_TRUE(Contains(paths, "/roms/manuals/Game.cbz"));
}

TEST(TestGameManual, PrefersBesideTheGameOverTheSubfolder)
{
  // A manual kept with one game is more specific than one in a shared folder
  const std::vector<std::string> paths = CGameManual::BuildManualPaths("/roms/Game.rom");

  EXPECT_LT(IndexOf(paths, "/roms/Game.pdf"), IndexOf(paths, "/roms/manuals/Game.pdf"));
}

TEST(TestGameManual, KeepsRegionAndRevisionTagsInTheExactCandidates)
{
  // The exact candidates keep the name as it is. Tags are only ignored later,
  // by the fallback search, and only when nothing matched exactly.
  const std::vector<std::string> paths =
      CGameManual::BuildManualPaths("/roms/Zelda (Europe) (Rev A) [!].nes");

  EXPECT_TRUE(Contains(paths, "/roms/Zelda (Europe) (Rev A) [!].pdf"));
}

TEST(TestGameManual, KeepsSpacesAndPunctuation)
{
  const std::vector<std::string> paths =
      CGameManual::BuildManualPaths("/roms/Mega Man X2 - The Sequel!.sfc");

  EXPECT_TRUE(Contains(paths, "/roms/Mega Man X2 - The Sequel!.pdf"));
}

TEST(TestGameManual, KeepsDotsInsideTheName)
{
  // Only the final extension is replaced
  const std::vector<std::string> paths = CGameManual::BuildManualPaths("/roms/Sonic 3.0 (USA).md");

  EXPECT_TRUE(Contains(paths, "/roms/Sonic 3.0 (USA).pdf"));
}

TEST(TestGameManual, WorksOnNetworkPaths)
{
  // The manual sits beside the game wherever the game lives
  EXPECT_TRUE(Contains(CGameManual::BuildManualPaths("smb://nas/roms/Game.rom"),
                       "smb://nas/roms/Game.pdf"));
  EXPECT_TRUE(Contains(CGameManual::BuildManualPaths("nfs://10.0.0.1/roms/Game.rom"),
                       "nfs://10.0.0.1/roms/Game.pdf"));
}

TEST(TestGameManual, RefusesAGameWithNoExtension)
{
  // Replacing nothing would invent a basename the player never chose
  EXPECT_TRUE(CGameManual::BuildManualPaths("/roms/Game").empty());
}

TEST(TestGameManual, RefusesAGameInsideAnArchive)
{
  // There is no directory alongside the game to hold a manual
  EXPECT_TRUE(CGameManual::BuildManualPaths("zip://%2froms%2fgames.zip/Game.rom").empty());
}

TEST(TestGameManual, RefusesAnEmptyPath)
{
  EXPECT_TRUE(CGameManual::BuildManualPaths("").empty());
}

// The looser fallback match, which only runs when no exact name matched

TEST(TestGameManual, NormalisingDropsRegionAndRevisionTags)
{
  EXPECT_EQ(CGameManual::NormaliseName("Sonic The Hedgehog 2 (World) (Rev A)"),
            CGameManual::NormaliseName("Sonic The Hedgehog 2"));
}

TEST(TestGameManual, NormalisingDropsBracketedTags)
{
  EXPECT_EQ(CGameManual::NormaliseName("Zelda (Europe) [!]"), CGameManual::NormaliseName("Zelda"));
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

// The lookup itself, against real files. Everything above reasons about paths;
// these check that the right one is picked off disk, including the fallback
// that only runs when no name matched exactly.

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

TEST(TestGameManual, FindsAManualBesideTheGame)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string manual = fixture.Touch("Game (USA).pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), manual);
}

TEST(TestGameManual, FindsAComicArchive)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string manual = fixture.Touch("Game (USA).cbz");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), manual);
}

TEST(TestGameManual, PrefersThePdfWhenBothAreThere)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string pdf = fixture.Touch("Game (USA).pdf");
  fixture.Touch("Game (USA).cbz");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), pdf);
}

TEST(TestGameManual, FindsAManualInTheManualsSubfolder)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string manual = fixture.Touch("manuals/Game (USA).pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), manual);
}

TEST(TestGameManual, FindsAManualInACapitalisedManualsSubfolder)
{
  // Collections built elsewhere use "Manuals", and on a case-sensitive
  // filesystem that is a different folder from the one downloads go in
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string manual = fixture.Touch("Manuals/Game (USA).pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), manual);
}

TEST(TestGameManual, FindsATagStrippedManualInACapitalisedSubfolder)
{
  const CManualFixture fixture;
  fixture.Touch("Sonic The Hedgehog 2 (World) (Rev A).md");
  const std::string manual = fixture.Touch("Manuals/Sonic The Hedgehog 2.pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Sonic The Hedgehog 2 (World) (Rev A).md")),
            manual);
}

TEST(TestGameManual, FindsAManualWhoseTagsDiffer)
{
  // The case the exact match cannot serve: ROM naming conventions add region
  // and revision tags that a manual scanned from the box will not carry
  const CManualFixture fixture;
  fixture.Touch("Sonic The Hedgehog 2 (World) (Rev A).md");
  const std::string manual = fixture.Touch("Sonic The Hedgehog 2.pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Sonic The Hedgehog 2 (World) (Rev A).md")),
            manual);
}

TEST(TestGameManual, PrefersAnExactMatchOverATagStrippedOne)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");
  const std::string exact = fixture.Touch("Game (USA).pdf");
  fixture.Touch("Game.pdf");

  EXPECT_EQ(CGameManual::GetManualPath(fixture.Path("Game (USA).md")), exact);
}

TEST(TestGameManual, DoesNotOpenADifferentGamesManual)
{
  // The failure that matters. A near miss must produce nothing rather than
  // the wrong game's instructions.
  const CManualFixture fixture;
  fixture.Touch("Sonic The Hedgehog 2 (USA).md");
  fixture.Touch("Sonic The Hedgehog 3 (USA).pdf");
  fixture.Touch("Sonic The Hedgehog (USA).pdf");

  EXPECT_TRUE(CGameManual::GetManualPath(fixture.Path("Sonic The Hedgehog 2 (USA).md")).empty());
}

TEST(TestGameManual, FindsNothingWhenThereIsNoManual)
{
  const CManualFixture fixture;
  fixture.Touch("Game (USA).md");

  EXPECT_TRUE(CGameManual::GetManualPath(fixture.Path("Game (USA).md")).empty());
}
