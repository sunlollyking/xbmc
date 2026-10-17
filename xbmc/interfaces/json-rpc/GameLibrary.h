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
  static JSONRPC_STATUS GetPlatforms(const std::string& method,
                                     ITransportLayer* transport,
                                     IClient* client,
                                     const CVariant& parameterObject,
                                     CVariant& result);
  static JSONRPC_STATUS GetPlatformDetails(const std::string& method,
                                           ITransportLayer* transport,
                                           IClient* client,
                                           const CVariant& parameterObject,
                                           CVariant& result);
  static JSONRPC_STATUS GetGames(const std::string& method,
                                 ITransportLayer* transport,
                                 IClient* client,
                                 const CVariant& parameterObject,
                                 CVariant& result);
  static JSONRPC_STATUS GetGameDetails(const std::string& method,
                                       ITransportLayer* transport,
                                       IClient* client,
                                       const CVariant& parameterObject,
                                       CVariant& result);
  static JSONRPC_STATUS GetFacet(const std::string& method,
                                 ITransportLayer* transport,
                                 IClient* client,
                                 const CVariant& parameterObject,
                                 CVariant& result);
  static JSONRPC_STATUS SetGameDetails(const std::string& method,
                                       ITransportLayer* transport,
                                       IClient* client,
                                       const CVariant& parameterObject,
                                       CVariant& result);
  static JSONRPC_STATUS RefreshGame(const std::string& method,
                                    ITransportLayer* transport,
                                    IClient* client,
                                    const CVariant& parameterObject,
                                    CVariant& result);
  static JSONRPC_STATUS RemoveGame(const std::string& method,
                                   ITransportLayer* transport,
                                   IClient* client,
                                   const CVariant& parameterObject,
                                   CVariant& result);
  static JSONRPC_STATUS Scan(const std::string& method,
                             ITransportLayer* transport,
                             IClient* client,
                             const CVariant& parameterObject,
                             CVariant& result);
  static JSONRPC_STATUS Clean(const std::string& method,
                              ITransportLayer* transport,
                              IClient* client,
                              const CVariant& parameterObject,
                              CVariant& result);
};
} // namespace JSONRPC
