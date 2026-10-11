/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/File.h"
#include "pictures/UltraHdr.h"
#include "test/TestUtils.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI;

namespace
{
std::string MakeJpeg(const std::string& xmp)
{
  return "\xFF\xD8<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">" + xmp + "</x:xmpmeta>\xFF\xD9";
}

std::string MakeUltraHdr(const std::string& gainMapXmp)
{
  const std::string gainMap = MakeJpeg(gainMapXmp);
  const std::string primary = MakeJpeg(
      "<rdf:Description hdrgm:Version=\"1.0\"><Container:Directory><rdf:Seq>"
      "<rdf:li><Container:Item Item:Semantic=\"Primary\" Item:Mime=\"image/jpeg\"/></rdf:li>"
      "<rdf:li><Container:Item Item:Semantic=\"GainMap\" Item:Mime=\"image/jpeg\" Item:Length=\"" +
      std::to_string(gainMap.size()) +
      "\"/></rdf:li></rdf:Seq></Container:Directory></rdf:Description>");
  return primary + gainMap;
}

PICTURE::GainMap Read(const std::string& contents)
{
  XFILE::CFile* file = XBMC_CREATETEMPFILE(".jpg");
  EXPECT_NE(file, nullptr);
  if (!file)
    return {};
  file->Write(contents.data(), contents.size());
  file->Close();

  PICTURE::GainMap gainMap = PICTURE::ReadGainMap(XBMC_TEMPFILEPATH(file));
  XBMC_DELETETEMPFILE(file);
  return gainMap;
}
} // namespace

TEST(TestUltraHdr, GainMap)
{
  const std::string gainMapXmp =
      "<rdf:Description hdrgm:Version=\"1.0\" hdrgm:GainMapMin=\"0.5\" hdrgm:GainMapMax=\"2.5\" "
      "hdrgm:Gamma=\"1.2\" hdrgm:OffsetSDR=\"0\" hdrgm:OffsetHDR=\"0\" "
      "hdrgm:HDRCapacityMin=\"0\" hdrgm:HDRCapacityMax=\"2.5\"/>";
  const PICTURE::GainMap gainMap = Read(MakeUltraHdr(gainMapXmp));

  EXPECT_EQ(gainMap.image, MakeJpeg(gainMapXmp));
  EXPECT_FLOAT_EQ(gainMap.params.min, 0.5f);
  EXPECT_FLOAT_EQ(gainMap.params.max, 2.5f);
  EXPECT_FLOAT_EQ(gainMap.params.gamma, 1.2f);
  EXPECT_FLOAT_EQ(gainMap.params.offsetSdr, 0.0f);
  EXPECT_FLOAT_EQ(gainMap.params.offsetHdr, 0.0f);
  EXPECT_FLOAT_EQ(gainMap.params.capacityMin, 0.0f);
  EXPECT_FLOAT_EQ(gainMap.params.capacityMax, 2.5f);
}

TEST(TestUltraHdr, ValuePerChannel)
{
  const PICTURE::GainMap gainMap =
      Read(MakeUltraHdr("<rdf:Description hdrgm:Version=\"1.0\" hdrgm:HDRCapacityMax=\"3\">"
                        "<hdrgm:GainMapMax><rdf:Seq><rdf:li>3</rdf:li><rdf:li>2.9</rdf:li>"
                        "<rdf:li>2.8</rdf:li></rdf:Seq></hdrgm:GainMapMax></rdf:Description>"));

  EXPECT_FALSE(gainMap.image.empty());
  EXPECT_FLOAT_EQ(gainMap.params.max, 3.0f);
  EXPECT_FLOAT_EQ(gainMap.params.offsetSdr, 1.0f / 64);
}

TEST(TestUltraHdr, NoGainMap)
{
  EXPECT_TRUE(Read(MakeJpeg("<rdf:Description/>")).image.empty());
  EXPECT_TRUE(Read(MakeUltraHdr("<rdf:Description hdrgm:Version=\"1.0\" "
                                "hdrgm:BaseRenditionIsHDR=\"True\"/>"))
                  .image.empty());
}
