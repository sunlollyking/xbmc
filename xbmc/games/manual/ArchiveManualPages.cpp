/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ArchiveManualPages.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "utils/CharsetConverter.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace KODI::GAME;

namespace
{
//! What a page inside a comic archive can be. Kept to a fixed list rather than
//! asking Kodi what it can display, so that a stray file next to the pages -
//! a ReadMe, a thumbnail database - cannot become page one.
constexpr std::array<const char*, 6> PAGE_EXTENSIONS = {".jpg", ".jpeg", ".png",
                                                        ".gif", ".bmp",  ".webp"};

//! An archive with more entries than this is not a manual
constexpr size_t MAX_PAGES = 2000;

bool IsPageImage(const std::string& path)
{
  std::string extension = URIUtils::GetExtension(path);
  StringUtils::ToLower(extension);

  return std::find(PAGE_EXTENSIONS.begin(), PAGE_EXTENSIONS.end(), extension) !=
         PAGE_EXTENSIONS.end();
}

/*!
 * \brief The archive protocol for a comic archive
 *
 * A .cbz is a zip and a .cbr is a rar. Kodi reads zips itself; rar needs an
 * archive add-on, so a .cbr may simply not open on a given install.
 */
std::string GetArchiveProtocol(const std::string& path)
{
  std::string extension = URIUtils::GetExtension(path);
  StringUtils::ToLower(extension);

  if (extension == ".cbz")
    return "zip";
  if (extension == ".cbr")
    return "rar";

  return {};
}
} // namespace

CArchiveManualPages::CArchiveManualPages(std::vector<std::string> pages) : m_pages(std::move(pages))
{
}

std::unique_ptr<CArchiveManualPages> CArchiveManualPages::Open(const std::string& path)
{
  const std::string protocol = GetArchiveProtocol(path);
  if (protocol.empty())
    return {};

  const CURL archiveURL = URIUtils::CreateArchivePath(protocol, CURL(path), "");

  CFileItemList items;
  if (!XFILE::CDirectory::GetDirectory(archiveURL.Get(), items, "", XFILE::DIR_FLAG_DEFAULTS))
  {
    CLog::Log(LOGERROR, "CArchiveManualPages: could not read \"{}\"", path);
    return {};
  }

  // Paired with a wide copy of the name, because the natural ordering below
  // works on wide strings and converting inside the comparison would repeat
  // the same conversions on every step of the sort
  std::vector<std::pair<std::wstring, std::string>> pages;

  for (int i = 0; i < items.Size(); ++i)
  {
    const CFileItemPtr& item = items[i];

    // Pages are expected at the top level. A folder inside is left alone
    // rather than recursed, because a manual with a directory tree in it is
    // not something the ordering below could make sense of.
    if (item->IsFolder())
      continue;

    const std::string& itemPath = item->GetPath();
    if (!IsPageImage(itemPath))
      continue;

    std::wstring name;
    g_charsetConverter.utf8ToW(URIUtils::GetFileName(itemPath), name);

    pages.emplace_back(std::move(name), itemPath);
  }

  if (pages.empty())
  {
    CLog::Log(LOGERROR, "CArchiveManualPages: no pages inside \"{}\"", path);
    return {};
  }

  if (pages.size() > MAX_PAGES)
  {
    CLog::Log(LOGERROR, "CArchiveManualPages: \"{}\" holds {} images, too many for a manual", path,
              pages.size());
    return {};
  }

  // The format carries no page order beyond the filenames, so they are sorted
  // the way a person reading the archive would expect. Natural ordering keeps
  // page 9 before page 10, which a plain sort would not.
  std::sort(pages.begin(), pages.end(), [](const auto& lhs, const auto& rhs)
            { return StringUtils::AlphaNumericCompare(lhs.first, rhs.first) < 0; });

  std::vector<std::string> ordered;
  ordered.reserve(pages.size());
  for (auto& page : pages)
    ordered.emplace_back(std::move(page.second));

  CLog::Log(LOGDEBUG, "CArchiveManualPages: opened \"{}\", {} page(s)", path, ordered.size());

  return std::unique_ptr<CArchiveManualPages>(new CArchiveManualPages(std::move(ordered)));
}

unsigned int CArchiveManualPages::GetPageCount() const
{
  return static_cast<unsigned int>(m_pages.size());
}

std::string CArchiveManualPages::GetPageImage(unsigned int page, unsigned int /*height*/) const
{
  if (page >= m_pages.size())
    return {};

  // Already an image, so it goes to the skin as it is and Kodi scales it
  return m_pages[page];
}
