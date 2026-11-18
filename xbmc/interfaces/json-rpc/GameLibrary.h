/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemHandler.h"
#include "JSONRPC.h"

#include <string>

class CVariant;

namespace JSONRPC
{
/*!
 * \brief The GameLibrary JSON-RPC namespace
 *
 * Lists and changes the game library the way VideoLibrary does the video
 * library. Listings go through the same gamedb:// paths the GUI browses, so
 * a filter here is a filter there.
 */
class CGameLibrary : public CFileItemHandler
{
public:
  static JSONRPC_STATUS GetPlatforms(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetPlatformDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetGames(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetGameDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetGenres(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetDevelopers(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetPublishers(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetCollections(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetTags(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetYears(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS GetRegions(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS SetGameDetails(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RefreshGame(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS RemoveGame(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Scan(const CVariant& parameterObject, CVariant& result);
  static JSONRPC_STATUS Clean(const CVariant& parameterObject, CVariant& result);

private:
  static JSONRPC_STATUS GetFacet(const std::string& facet,
                                 const CVariant& parameterObject,
                                 CVariant& result);
};
} // namespace JSONRPC
