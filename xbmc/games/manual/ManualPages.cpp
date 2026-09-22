/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ManualPages.h"

#include "ArchiveManualPages.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#if defined(HAVE_POPPLER)
#include "PdfManualPages.h"
#endif

#include <algorithm>
#include <array>

using namespace KODI::GAME;

namespace
{
//! Comic archives: a zip or rar of page images, which is how scanned manuals
//! are usually distributed
constexpr std::array<const char*, 2> ARCHIVE_EXTENSIONS = {".cbz", ".cbr"};

constexpr const char* PDF_EXTENSION = ".pdf";
} // namespace

bool KODI::GAME::IsManualExtension(const std::string& extension)
{
  std::string lower = extension;
  StringUtils::ToLower(lower);

  if (lower == PDF_EXTENSION)
    return true;

  return std::find(ARCHIVE_EXTENSIONS.begin(), ARCHIVE_EXTENSIONS.end(), lower) !=
         ARCHIVE_EXTENSIONS.end();
}

std::unique_ptr<IManualPages> KODI::GAME::OpenManualPages(const std::string& path)
{
  std::string extension = URIUtils::GetExtension(path);
  StringUtils::ToLower(extension);

  if (std::find(ARCHIVE_EXTENSIONS.begin(), ARCHIVE_EXTENSIONS.end(), extension) !=
      ARCHIVE_EXTENSIONS.end())
  {
    return CArchiveManualPages::Open(path);
  }

#if defined(HAVE_POPPLER)
  if (extension == PDF_EXTENSION)
    return CPdfManualPages::Open(path);
#endif

  // A PDF in a build with no PDF library, or something the finder should not
  // have offered in the first place
  return {};
}
