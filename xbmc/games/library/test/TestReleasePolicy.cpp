/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/library/ReleasePolicy.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
GameRelease Release(const std::vector<std::string>& regions, std::string revision = "")
{
  GameRelease release;
  for (const std::string& region : regions)
    release.regions.emplace_back(GameRegion::FromString(region));
  release.revision = std::move(revision);
  release.files.emplace_back();
  return release;
}

CReleasePolicy PolicyFor(const std::string& region)
{
  return CReleasePolicy(CReleasePolicy::RegionPriority(GameRegion::FromString(region)), true, true,
                        true);
}

std::vector<std::string> PriorityFor(const std::string& region)
{
  std::vector<std::string> codes;
  for (const GameRegion& preferred : CReleasePolicy::RegionPriority(GameRegion::FromString(region)))
    codes.emplace_back(preferred.ToString());
  return codes;
}
} // namespace

TEST(TestReleasePolicy, PutsThePlayersRegionFirst)
{
  EXPECT_EQ(PriorityFor("GB"), (std::vector<std::string>{"GB", "150", "World", "US", "JP"}));
  EXPECT_EQ(PriorityFor("JP"), (std::vector<std::string>{"JP", "World", "150", "US"}));
  EXPECT_EQ(PriorityFor("CN"), (std::vector<std::string>{"CN", "142", "World", "150", "US", "JP"}));
  EXPECT_EQ(PriorityFor("CA"), (std::vector<std::string>{"CA", "US", "World", "150", "JP"}));
}

TEST(TestReleasePolicy, PlaysTheReleaseFromThePlayersRegion)
{
  const std::vector<GameRelease> releases{Release({"US"}), Release({"150"}), Release({"JP"})};
  EXPECT_EQ(PolicyFor("GB").PickDefault(releases), 1);
  EXPECT_EQ(PolicyFor("JP").PickDefault(releases), 2);
  EXPECT_EQ(PolicyFor("BR").PickDefault(releases), 0);
}

TEST(TestReleasePolicy, PlaysAWorldReleaseBeforeAnotherRegions)
{
  const std::vector<GameRelease> releases{Release({"US"}), Release({"World"})};
  EXPECT_EQ(PolicyFor("150").PickDefault(releases), 1);
}

TEST(TestReleasePolicy, PrefersAFinishedRelease)
{
  std::vector<GameRelease> releases{Release({"150"}), Release({"US"})};
  releases[0].status = ReleaseStatus::BETA;
  EXPECT_EQ(PolicyFor("150").PickDefault(releases), 1);

  const CReleasePolicy regionFirst(CReleasePolicy::RegionPriority(GameRegion::FromString("150")),
                                   false, true, true);
  EXPECT_EQ(regionFirst.PickDefault(releases), 0);
}

TEST(TestReleasePolicy, PrefersTheNewestRevision)
{
  EXPECT_EQ(PolicyFor("150").PickDefault(
                {Release({"150"}), Release({"150"}, "Rev 2"), Release({"150"}, "Rev 1")}),
            1);
  EXPECT_EQ(PolicyFor("150").PickDefault({Release({"150"}, "v1.1"), Release({"150"}, "v1.10")}), 1);
  // Each part of a version is compared as a number
  EXPECT_EQ(PolicyFor("150").PickDefault({Release({"150"}, "v2.0"), Release({"150"}, "v1.10")}), 0);
  EXPECT_EQ(PolicyFor("150").PickDefault({Release({"150"}, "v1.9"), Release({"150"}, "v1.10")}), 1);
  // Letters count by their place in the alphabet, whichever they are
  EXPECT_EQ(PolicyFor("150").PickDefault(
                {Release({"150"}, "Rev A"), Release({"150"}, "Rev E"), Release({"150"}, "Rev B")}),
            1);
}

TEST(TestReleasePolicy, PrefersAVerifiedDump)
{
  std::vector<GameRelease> releases{Release({"150"}), Release({"150"})};
  releases[1].dump = DumpStatus::VERIFIED;
  EXPECT_EQ(PolicyFor("150").PickDefault(releases), 1);
}

TEST(TestReleasePolicy, NeverPlaysABadDumpWhenThereIsAnother)
{
  std::vector<GameRelease> releases{Release({"150"}, "Rev 1"), Release({"JP"})};
  releases[0].dump = DumpStatus::BAD;
  EXPECT_EQ(PolicyFor("150").PickDefault(releases), 1);
}

TEST(TestReleasePolicy, PlaysOnlyAReleaseWithFiles)
{
  std::vector<GameRelease> releases{Release({"150"}), Release({"US"})};
  releases[0].files.clear();
  EXPECT_EQ(PolicyFor("150").PickDefault(releases), 1);
  EXPECT_EQ(PolicyFor("150").PickDefault({}), -1);
}
