/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MotionPhoto.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "PictureXmp.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "utils/Crc32.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string_view>

using namespace XFILE;

namespace
{
constexpr std::string_view CLIP_FOLDER = "special://temp/motionphotos/";

/*!
 * \brief Size of the clip at the end of a Motion Photo, or 0 if the XMP doesn't describe one
 */
uint64_t GetClipSize(std::string_view xmp)
{
  using namespace KODI::PICTURE;

  if (XMP::GetValue(xmp, "GCamera:MotionPhoto") == "1")
  {
    for (const auto& item : XMP::GetContainerItems(xmp))
    {
      if (item.semantic == "MotionPhoto")
        return item.length;
    }
    return 0;
  }

  // Older MicroVideo format
  if (XMP::GetValue(xmp, "GCamera:MicroVideo") == "1")
    return XMP::GetUnsigned(xmp, "GCamera:MicroVideoOffset");

  return 0;
}

std::string GetLivePhotoKey(const std::string& path)
{
  std::string key = path;
  URIUtils::RemoveExtension(key);
  StringUtils::ToLower(key);
  return key;
}
} // namespace

namespace KODI::PICTURE
{
void GroupLivePhotos(CFileItemList& items)
{
  std::map<std::string, std::shared_ptr<CFileItem>, std::less<>> pictures;
  for (const auto& item : items)
  {
    if (!item->IsFolder() && item->IsPicture())
      pictures.try_emplace(GetLivePhotoKey(item->GetPath()), item);
  }

  if (pictures.empty())
    return;

  for (int i = items.Size() - 1; i >= 0; --i)
  {
    const std::shared_ptr<CFileItem> item = items[i];
    if (item->IsFolder() || !URIUtils::HasExtension(item->GetPath(), ".mov"))
      continue;

    const auto picture = pictures.find(GetLivePhotoKey(item->GetPath()));
    if (picture == pictures.end())
      continue;

    picture->second->SetProperty("motionclip", item->GetPath());
    items.Remove(i);
  }
}

std::string ExtractMotionPhotoClip(const std::string& path)
{
  if (!URIUtils::HasExtension(path, ".jpg|.jpeg"))
    return {};

  CFile file;
  if (!file.Open(path))
    return {};

  const int64_t fileSize = file.GetLength();
  const uint64_t clipSize = GetClipSize(XMP::Read(file));
  if (clipSize == 0 || clipSize >= static_cast<uint64_t>(fileSize))
    return {};

  const int64_t clipStart = fileSize - static_cast<int64_t>(clipSize);
  std::array<char, 8> boxHeader{};
  if (file.Seek(clipStart) != clipStart ||
      file.Read(boxHeader.data(), boxHeader.size()) != static_cast<ssize_t>(boxHeader.size()) ||
      std::string_view(boxHeader.data() + 4, 4) != "ftyp")
  {
    CLog::Log(LOGDEBUG, "MotionPhoto: {} has no MP4 where its XMP says the clip is", path);
    return {};
  }

  const std::string clipPath =
      StringUtils::Format("{}{:08x}-{}.mp4", CLIP_FOLDER, Crc32::Compute(path), clipSize);
  if (CFile::Exists(clipPath, false))
    return clipPath;

  // Only a complete copy gets the clip's name, so a copy cut short is never used
  const std::string partPath = clipPath + ".part";
  CDirectory::Create(std::string(CLIP_FOLDER));
  CFile clip;
  if (file.Seek(clipStart) != clipStart || !clip.OpenForWrite(partPath, true))
    return {};

  std::array<char, 64 * 1024> buffer;
  uint64_t remaining = clipSize;
  while (remaining > 0)
  {
    const size_t chunk = static_cast<size_t>(std::min<uint64_t>(remaining, buffer.size()));
    const ssize_t read = file.Read(buffer.data(), chunk);
    if (read <= 0 || clip.Write(buffer.data(), static_cast<size_t>(read)) != read)
      break;
    remaining -= static_cast<uint64_t>(read);
  }
  clip.Close();

  if (remaining > 0 || !CFile::Rename(partPath, clipPath))
  {
    CLog::Log(LOGERROR, "MotionPhoto: failed to copy the clip from {}", path);
    CFile::Delete(partPath);
    return {};
  }

  return clipPath;
}

void ClearMotionPhotoClips()
{
  if (CDirectory::Exists(std::string(CLIP_FOLDER)))
    CDirectory::RemoveRecursive(std::string(CLIP_FOLDER));
}
} // namespace KODI::PICTURE
