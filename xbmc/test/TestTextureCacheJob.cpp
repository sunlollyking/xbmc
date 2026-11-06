/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TextureCacheJob.h"
#include "imagefiles/ImageFileURL.h"

#include <gtest/gtest.h>

TEST(TestTextureCacheJob, MayBeAnImage)
{
  EXPECT_TRUE(CTextureCacheJob::MayBeAnImage("image/jpeg"));
  EXPECT_TRUE(CTextureCacheJob::MayBeAnImage("image/png"));
  EXPECT_TRUE(CTextureCacheJob::MayBeAnImage("IMAGE/JPEG"));

  // What a source says when it doesn't know, which an image still has to be tried against
  EXPECT_TRUE(CTextureCacheJob::MayBeAnImage("application/octet-stream"));

  EXPECT_FALSE(CTextureCacheJob::MayBeAnImage("text/html"));
  EXPECT_FALSE(CTextureCacheJob::MayBeAnImage("text/asp"));
  EXPECT_FALSE(CTextureCacheJob::MayBeAnImage("video/mp4"));

  // Nothing has said what it is, so there is nothing here to rule it in either
  EXPECT_FALSE(CTextureCacheJob::MayBeAnImage(""));
}

TEST(TestTextureCacheJob, StandsUpAPictureAskedForUprightThatIsLyingDown)
{
  auto portrait = IMAGE_FILES::CImageFileURL::FromFile("/art/spine.png");
  portrait.AddOption("orientation", "portrait");

  // A spine scanned on its side is turned a quarter clockwise
  EXPECT_EQ(CTextureCacheJob::UprightOrientation(portrait, 680, 97, 0), 5);

  // One already standing is left alone
  EXPECT_EQ(CTextureCacheJob::UprightOrientation(portrait, 97, 680, 0), 0);

  // As is one EXIF has already turned
  EXPECT_EQ(CTextureCacheJob::UprightOrientation(portrait, 680, 97, 2), 2);

  // And any picture that wasn't asked for upright
  EXPECT_EQ(CTextureCacheJob::UprightOrientation(
                IMAGE_FILES::CImageFileURL::FromFile("/art/fanart.png"), 680, 97, 0),
            0);
}
