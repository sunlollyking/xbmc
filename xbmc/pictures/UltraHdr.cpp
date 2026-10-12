/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "UltraHdr.h"

#include "PictureXmp.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <cmath>
#include <cstdint>
#include <string_view>

using namespace KODI::PICTURE;

namespace
{
float GetFloat(std::string_view xmp, std::string_view name, float fallback)
{
  const std::string_view value = XMP::GetValue(xmp, name);
  return value.empty() ? fallback : StringUtils::ToFloat(value, fallback);
}
} // namespace

GainMap KODI::PICTURE::ReadGainMap(const std::string& path)
{
  if (!URIUtils::HasExtension(path, ".jpg|.jpeg"))
    return {};

  XFILE::CFile file;
  if (!file.Open(path))
    return {};

  const std::string xmp = XMP::Read(file);
  if (XMP::GetValue(xmp, "hdrgm:Version").empty())
    return {};

  const int64_t fileSize = file.GetLength();
  if (fileSize <= 0)
    return {};
  const uint64_t limit = static_cast<uint64_t>(fileSize);

  // The gain map is stored after the primary picture, followed by any later items
  uint64_t offsetFromEnd = 0;
  uint64_t size = 0;
  for (const auto& item : XMP::GetContainerItems(xmp))
  {
    if (item.semantic == "GainMap" && size == 0)
      size = item.length;
    if (size == 0)
      continue;
    if (item.length > limit || item.padding > limit - item.length ||
        offsetFromEnd > limit - item.length - item.padding)
      return {};
    offsetFromEnd += item.length + item.padding;
  }

  if (size == 0 || offsetFromEnd >= limit)
    return {};

  GainMap gainMap;
  gainMap.image.resize(static_cast<size_t>(size));
  const int64_t offset = fileSize - static_cast<int64_t>(offsetFromEnd);
  if (file.Seek(offset) != offset ||
      file.Read(gainMap.image.data(), gainMap.image.size()) !=
          static_cast<ssize_t>(gainMap.image.size()) ||
      !gainMap.image.starts_with("\xFF\xD8"))
  {
    CLog::Log(LOGDEBUG, "UltraHdr: {} has no JPEG where its XMP says the gain map is", path);
    return {};
  }

  // The gain map's own XMP says how to apply it
  file.Seek(offset);
  const std::string gainMapXmp = XMP::Read(file);
  if (XMP::GetValue(gainMapXmp, "hdrgm:Version").empty() ||
      StringUtils::EqualsNoCase(XMP::GetValue(gainMapXmp, "hdrgm:BaseRenditionIsHDR"), "True"))
    return {};

  GainMapParams& params = gainMap.params;
  params.min = GetFloat(gainMapXmp, "hdrgm:GainMapMin", params.min);
  params.max = GetFloat(gainMapXmp, "hdrgm:GainMapMax", params.max);
  params.gamma = GetFloat(gainMapXmp, "hdrgm:Gamma", params.gamma);
  params.offsetSdr = GetFloat(gainMapXmp, "hdrgm:OffsetSDR", params.offsetSdr);
  params.offsetHdr = GetFloat(gainMapXmp, "hdrgm:OffsetHDR", params.offsetHdr);
  params.capacityMin = GetFloat(gainMapXmp, "hdrgm:HDRCapacityMin", params.capacityMin);
  params.capacityMax = GetFloat(gainMapXmp, "hdrgm:HDRCapacityMax", params.capacityMax);
  for (const float value : {params.min, params.max, params.gamma, params.offsetSdr,
                            params.offsetHdr, params.capacityMin, params.capacityMax})
  {
    if (!std::isfinite(value))
      return {};
  }
  if (params.gamma <= 0.0f || params.capacityMax <= params.capacityMin)
    return {};

  return gainMap;
}
