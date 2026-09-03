/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlatformCatalogue.h"

#include "filesystem/SpecialProtocol.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <cctype>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* CATALOGUE_PATH = "special://xbmc/system/games/platforms.xml";
constexpr unsigned int CATALOGUE_VERSION = 1;

std::string ElementText(const tinyxml2::XMLElement* parent, const char* name)
{
  std::string value;
  XMLUtils::GetString(parent, name, value);
  StringUtils::Trim(value);
  return value;
}

int ElementInt(const tinyxml2::XMLElement* parent, const char* name)
{
  int value = 0;
  XMLUtils::GetInt(parent, name, value);
  return value;
}
} // namespace

std::string CPlatformCatalogue::NameKey(std::string_view name)
{
  std::string key;
  key.reserve(name.size());
  for (const char c : name)
  {
    if (std::isalnum(static_cast<unsigned char>(c)))
      key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return key;
}

bool CPlatformCatalogue::Load()
{
  return Load(CSpecialProtocol::TranslatePath(CATALOGUE_PATH));
}

bool CPlatformCatalogue::Load(const std::string& path)
{
  CXBMCTinyXML2 doc;
  if (!doc.LoadFile(path))
  {
    CLog::Log(LOGERROR, "GAME: Cannot read the platform catalogue at {}", path);
    return false;
  }

  const tinyxml2::XMLElement* root = doc.RootElement();
  if (root == nullptr || std::string_view(root->Value()) != "platforms")
  {
    CLog::Log(LOGERROR, "GAME: {} is not a platform catalogue", path);
    return false;
  }

  const unsigned int version = root->UnsignedAttribute("version", 0);
  if (version > CATALOGUE_VERSION)
  {
    CLog::Log(LOGWARNING, "GAME: Platform catalogue version {} is newer than {}; reading what "
                          "is understood",
              version, CATALOGUE_VERSION);
  }

  std::vector<PlatformInfo> platforms;
  std::map<std::string, size_t> byKey;
  std::map<std::string, size_t> bySlug;

  for (const tinyxml2::XMLElement* el = root->FirstChildElement("platform"); el != nullptr;
       el = el->NextSiblingElement("platform"))
  {
    PlatformInfo platform;
    platform.slug = el->Attribute("id") ? el->Attribute("id") : "";
    platform.name = ElementText(el, "name");
    if (platform.slug.empty() || platform.name.empty())
    {
      CLog::Log(LOGWARNING, "GAME: Skipping a catalogue platform with no id or name");
      continue;
    }
    if (bySlug.contains(platform.slug))
    {
      CLog::Log(LOGWARNING, "GAME: Skipping duplicate catalogue platform {}", platform.slug);
      continue;
    }

    platform.sortName = ElementText(el, "sortname");
    if (platform.sortName.empty())
      platform.sortName = platform.name;
    platform.manufacturer = ElementText(el, "manufacturer");
    platform.type = CGameLibraryTypes::PlatformTypeFromString(ElementText(el, "type"));
    platform.media = CGameLibraryTypes::MediaFormatFromString(ElementText(el, "media"));
    platform.family = ElementText(el, "family");
    platform.released = ElementInt(el, "released");
    platform.discontinued = ElementInt(el, "discontinued");
    platform.overview = ElementText(el, "overview");

    for (const std::string& ext : StringUtils::Split(ElementText(el, "extensions"), ' '))
    {
      std::string e = StringUtils::ToLower(ext);
      if (!e.empty() && e.front() == '.')
        e.erase(0, 1);
      if (!e.empty())
        platform.extensions.emplace_back(std::move(e));
    }

    for (const tinyxml2::XMLElement* alias = el->FirstChildElement("alias"); alias != nullptr;
         alias = alias->NextSiblingElement("alias"))
    {
      if (alias->GetText() != nullptr)
      {
        std::string a = alias->GetText();
        StringUtils::Trim(a);
        if (!a.empty())
          platform.aliases.emplace_back(std::move(a));
      }
    }

    if (const tinyxml2::XMLElement* ids = el->FirstChildElement("ids"); ids != nullptr)
    {
      for (const tinyxml2::XMLAttribute* attr = ids->FirstAttribute(); attr != nullptr;
           attr = attr->Next())
      {
        if (attr->Value() != nullptr && *attr->Value() != '\0')
          platform.providerIds[attr->Name()] = attr->Value();
      }
    }

    const size_t index = platforms.size();
    bySlug[platform.slug] = index;

    // A name that two platforms share is nobody's name
    auto claim = [&byKey, index](std::string_view name)
    {
      const std::string key = NameKey(name);
      if (key.empty())
        return;
      auto [it, inserted] = byKey.emplace(key, index);
      if (!inserted && it->second != index)
        it->second = SIZE_MAX;
    };
    claim(platform.slug);
    claim(platform.name);
    for (const std::string& alias : platform.aliases)
      claim(alias);

    platforms.emplace_back(std::move(platform));
  }

  if (platforms.empty())
  {
    CLog::Log(LOGERROR, "GAME: The platform catalogue at {} holds no platforms", path);
    return false;
  }

  std::erase_if(byKey, [](const auto& kv) { return kv.second == SIZE_MAX; });

  m_platforms = std::move(platforms);
  m_byKey = std::move(byKey);
  m_bySlug = std::move(bySlug);

  CLog::Log(LOGINFO, "GAME: Loaded {} platforms and {} names from the catalogue",
            m_platforms.size(), m_byKey.size());
  return true;
}

const PlatformInfo* CPlatformCatalogue::GetPlatform(std::string_view slug) const
{
  const auto it = m_bySlug.find(std::string(slug));
  return it == m_bySlug.end() ? nullptr : &m_platforms[it->second];
}

const PlatformInfo* CPlatformCatalogue::FindByName(std::string_view name) const
{
  const auto it = m_byKey.find(NameKey(name));
  return it == m_byKey.end() ? nullptr : &m_platforms[it->second];
}

std::vector<const PlatformInfo*> CPlatformCatalogue::FindByExtensions(
    const std::vector<std::string>& extensions) const
{
  std::vector<std::pair<size_t, const PlatformInfo*>> scored;

  for (const PlatformInfo& platform : m_platforms)
  {
    size_t hits = 0;
    for (const std::string& ext : extensions)
    {
      if (std::ranges::find(platform.extensions, ext) != platform.extensions.end())
        ++hits;
    }
    if (hits > 0)
      scored.emplace_back(hits, &platform);
  }

  std::ranges::stable_sort(scored, [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<const PlatformInfo*> platforms;
  platforms.reserve(scored.size());
  for (const auto& [hits, platform] : scored)
    platforms.emplace_back(platform);
  return platforms;
}

const PlatformInfo* CPlatformCatalogue::Suggest(const std::string& folderPath,
                                                const std::vector<std::string>& extensions) const
{
  // The folder's own name, then the folders above it
  std::string path = folderPath;
  URIUtils::RemoveSlashAtEnd(path);
  for (int depth = 0; depth < 3 && !path.empty(); ++depth)
  {
    const std::string name = URIUtils::GetFileName(path);
    if (const PlatformInfo* platform = FindByName(name); platform != nullptr)
      return platform;

    const std::string parent = URIUtils::GetParentPath(path);
    if (parent.empty() || parent == path)
      break;
    path = parent;
    URIUtils::RemoveSlashAtEnd(path);
  }

  // Extensions only settle it when one platform clearly owns them
  const std::vector<const PlatformInfo*> byExtension = FindByExtensions(extensions);
  if (byExtension.size() == 1)
    return byExtension.front();

  return nullptr;
}
