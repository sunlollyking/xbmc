/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoFilters.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "cores/RetroPlayer/guibridge/GUIGameVideoHandle.h"
#include "cores/RetroPlayer/rendering/RenderVideoSettings.h"
#include "cores/RetroPlayer/shaders/ShaderPresetFactory.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "games/GameServices.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/XBMCTinyXML.h"
#include "utils/log.h"

#include <memory>
#include <string>
#include <vector>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* PRESETS_ADDON_NAME = "game.shader.presets";
constexpr const char* ICON_VIDEO = "";

struct ScalingMethodProperties
{
  int nameIndex;
  int categoryIndex;
  RETRO::SCALINGMETHOD scalingMethod;
};

const std::vector<ScalingMethodProperties> scalingMethods = {
    {16301, 16296, RETRO::SCALINGMETHOD::NEAREST},
    {16302, 16297, RETRO::SCALINGMETHOD::LINEAR},
};

struct VideoFilterProperties
{
  std::string path;
  std::string name;
  std::string folder;
};

TiXmlNode* GetFirstChildOfNode(TiXmlNode& node, const char* childName)
{
  TiXmlNode* ret{node.FirstChild(childName)};
  if (ret)
    return ret->FirstChild();

  return ret;
}

void AddScalingMethods(CFileItemList& items, RETRO::CGUIGameVideoHandle* videoHandle)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  for (const auto& scalingMethodProps : scalingMethods)
  {
    // With a game running, only what it can actually do is offered. Without
    // one there is nothing to ask, so both are offered.
    if (videoHandle != nullptr &&
        !videoHandle->SupportsScalingMethod(scalingMethodProps.scalingMethod))
      continue;

    RETRO::CRenderVideoSettings videoSettings;
    videoSettings.SetScalingMethod(scalingMethodProps.scalingMethod);

    CFileItemPtr item = std::make_shared<CFileItem>(strings.Get(scalingMethodProps.nameIndex));
    item->SetLabel2(strings.Get(scalingMethodProps.categoryIndex));
    item->SetProperty("game.videofilter", CVariant{videoSettings.GetVideoFilter()});
    item->SetArt("icon", ICON_VIDEO);
    items.Add(std::move(item));
  }
}

void AddShaderPresets(CFileItemList& items)
{
  //! @todo Have the add-on give us the xml as a string (or parse it)
  std::string xmlFilename;
#if defined(HAS_GLES)
  xmlFilename = "ShaderPresetsGLSLP_GLES.xml";
#elif defined(HAS_GL)
  xmlFilename = "ShaderPresetsGLSLP.xml";
#else
  xmlFilename = "ShaderPresetsHLSLP.xml";
#endif

  const std::string homeAddonPath = CSpecialProtocol::TranslatePath(
      URIUtils::AddFileToFolder("special://home", "addons", PRESETS_ADDON_NAME));
  const std::string systemAddonPath = CSpecialProtocol::TranslatePath(
      URIUtils::AddFileToFolder("special://xbmc", "addons", PRESETS_ADDON_NAME));
  const std::string binAddonPath = CSpecialProtocol::TranslatePath(
      URIUtils::AddFileToFolder("special://xbmcbinaddons", PRESETS_ADDON_NAME));

  std::string xmlPath;
  std::unique_ptr<CXBMCTinyXML> xml;

  for (const auto& basePath : {homeAddonPath, systemAddonPath, binAddonPath})
  {
    xmlPath = URIUtils::AddFileToFolder(basePath, "resources", xmlFilename);

    CLog::LogF(LOGDEBUG, "Looking for shader preset XML at {}", CURL::GetRedacted(xmlPath));

    if (XFILE::CFile::Exists(xmlPath))
    {
      xml = std::make_unique<CXBMCTinyXML>(xmlPath);
      if (xml->LoadFile())
        break;

      CLog::LogF(LOGERROR, "Couldn't load shader presets from XML, {}", CURL::GetRedacted(xmlPath));
      xml.reset();
    }
  }

  if (!xml)
    return;

  std::vector<VideoFilterProperties> videoFilters;

  auto root = xml->RootElement();
  TiXmlNode* child = nullptr;

  while ((child = root->IterateChildren(child)))
  {
    VideoFilterProperties videoFilter;

    if (child->FirstChild() == nullptr)
      continue;

    const TiXmlNode* pathNode{GetFirstChildOfNode(*child, "path")};
    if (pathNode)
      videoFilter.path =
          URIUtils::AddFileToFolder(URIUtils::GetBasePath(xmlPath), pathNode->Value());

    const TiXmlNode* nameNode{GetFirstChildOfNode(*child, "name")};
    if (nameNode)
      videoFilter.name = nameNode->Value();

    const TiXmlNode* folderNode{GetFirstChildOfNode(*child, "folder")};
    if (folderNode)
      videoFilter.folder = folderNode->Value();

    videoFilters.emplace_back(videoFilter);
  }

  CLog::Log(LOGDEBUG, "Loaded {} shader presets from default XML, {}", videoFilters.size(),
            CURL::GetRedacted(xmlPath));

  for (const auto& videoFilter : videoFilters)
  {
    if (!CServiceBroker::GetGameServices().VideoShaders().CanLoadPreset(videoFilter.path))
      continue;

    auto item{std::make_shared<CFileItem>(videoFilter.name)};
    item->SetLabel2(videoFilter.folder);
    item->SetProperty("game.videofilter", CVariant{videoFilter.path});
    item->SetArt("icon", ICON_VIDEO);

    items.Add(std::move(item));
  }
}
} // namespace

void GAME::GetVideoFilters(CFileItemList& items, RETRO::CGUIGameVideoHandle* videoHandle)
{
  AddScalingMethods(items, videoHandle);
  AddShaderPresets(items);
}
