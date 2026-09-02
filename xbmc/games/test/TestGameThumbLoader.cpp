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
#include "games/GameThumbLoader.h"
#include "test/TestUtils.h"
#include "utils/URIUtils.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
/*!
 * \brief A temporary folder to lay artwork out in
 *
 * Artwork is found by the name of the game it belongs to, so the files are
 * made by hand rather than by CreateTempFile().
 */
class CArtFixture
{
public:
  CArtFixture()
  {
    XFILE::CFile* anchor = XBMC_CREATETEMPFILE("");
    m_root = URIUtils::AddFileToFolder(CXBMCTestUtils::Instance().TempFileDirectory(anchor),
                                       "gamearttest");
    XBMC_DELETETEMPFILE(anchor);

    XFILE::CDirectory::RemoveRecursive(m_root);
    XFILE::CDirectory::Create(m_root);
  }

  ~CArtFixture() { XFILE::CDirectory::RemoveRecursive(m_root); }

  std::string Touch(const std::string& name) const
  {
    const std::string path = Path(name);

    XFILE::CDirectory::Create(URIUtils::GetDirectory(path));

    XFILE::CFile file;
    if (file.OpenForWrite(path, true))
      file.Close();

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

TEST(TestGameThumbLoader, FindsBoxArtBesideTheGame)
{
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  const std::string boxfront = fixture.Touch("Sonic The Hedgehog-boxfront.jpg");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_EQ(item.GetArt("boxfront"), boxfront);
}

TEST(TestGameThumbLoader, TheBoxFrontBecomesTheThumb)
{
  // Spec: a skin that asks for nothing in particular still shows the game
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  const std::string boxfront = fixture.Touch("Sonic The Hedgehog-boxfront.png");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_EQ(item.GetArt("thumb"), boxfront);
}

TEST(TestGameThumbLoader, TheBoxFrontFillsThePosterSkinsAskFor)
{
  // Spec: skins built for films ask for a poster, and nothing writes one for
  // a game, so the box front fills it rather than the slot staying empty
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  const std::string boxfront = fixture.Touch("Sonic The Hedgehog-boxfront.jpg");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_EQ(item.GetArt("poster"), boxfront);
}

TEST(TestGameThumbLoader, FindsEveryTypeItKnows)
{
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  fixture.Touch("Sonic The Hedgehog-boxfront.jpg");
  fixture.Touch("Sonic The Hedgehog-boxback.jpg");
  fixture.Touch("Sonic The Hedgehog-fanart.jpg");
  fixture.Touch("Sonic The Hedgehog-clearlogo.png");
  fixture.Touch("Sonic The Hedgehog-banner.jpg");
  fixture.Touch("Sonic The Hedgehog-snap.png");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  for (const char* type : {"boxfront", "boxback", "fanart", "clearlogo", "banner", "snap"})
    EXPECT_FALSE(item.GetArt(type).empty()) << type;
}

TEST(TestGameThumbLoader, PrefersJpegOverPng)
{
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  const std::string jpg = fixture.Touch("Sonic The Hedgehog-boxfront.jpg");
  fixture.Touch("Sonic The Hedgehog-boxfront.png");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_EQ(item.GetArt("boxfront"), jpg);
}

TEST(TestGameThumbLoader, TakesAPictureLeftBesideTheGame)
{
  // A collection that has never been through a scraper looks like this
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  const std::string plain = fixture.Touch("Sonic The Hedgehog.jpg");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_TRUE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_EQ(item.GetArt("thumb"), plain);
}

TEST(TestGameThumbLoader, DoesNotTakeAnotherGamesArtwork)
{
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");
  fixture.Touch("Sonic The Hedgehog 2-boxfront.jpg");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_FALSE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_TRUE(item.GetArt("boxfront").empty());
}

TEST(TestGameThumbLoader, FindsNothingWhenThereIsNoArtwork)
{
  const CArtFixture fixture;
  fixture.Touch("Sonic The Hedgehog.md");

  CFileItem item{fixture.Path("Sonic The Hedgehog.md"), false};
  EXPECT_FALSE(CGameThumbLoader::LoadLocalArt(item));
  EXPECT_TRUE(item.GetArt("thumb").empty());
}
