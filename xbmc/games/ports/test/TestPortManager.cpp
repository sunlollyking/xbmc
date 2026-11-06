/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "URL.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/controllers/types/ControllerTree.h"
#include "games/ports/input/PortManager.h"
#include "games/ports/types/PortNode.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
const std::string PROFILE = "special://temp/testportmanager/";

CControllerTree OnePort()
{
  CPortNode port;
  port.SetPortID("1");
  port.SetAddress("/1");
  port.SetConnected(true);

  CControllerTree tree;
  tree.SetPorts({port});
  return tree;
}

//! A game of one platform played, port 1 unplugged, the game closed
void UnplugInGame(const std::string& platform)
{
  CPortManager manager;
  manager.Initialize(PROFILE);
  manager.SetControllerTree(OnePort());
  manager.LoadXML();
  manager.UsePlatform(platform, OnePort());
  manager.ConnectController("/1", false);
  manager.SaveXMLAsync();
  manager.Deinitialize();
}

bool PortConnected(const std::string& platform)
{
  CPortManager manager;
  manager.Initialize(PROFILE);
  manager.SetControllerTree(OnePort());
  manager.LoadXML();
  manager.UsePlatform(platform, OnePort());
  const bool connected = manager.GetControllerTree().GetPorts().front().IsConnected();
  manager.Deinitialize();
  return connected;
}

class TestPortManager : public testing::Test
{
protected:
  void SetUp() override { ASSERT_TRUE(XFILE::CDirectory::Create(PROFILE)); }
  void TearDown() override { XFILE::CDirectory::RemoveRecursive(CURL(PROFILE)); }
};
} // namespace

TEST_F(TestPortManager, KeepsAPortSetupForEachPlatform)
{
  UnplugInGame("wii");

  EXPECT_TRUE(XFILE::CFile::Exists(PROFILE + "ports.wii.xml"));
  EXPECT_FALSE(XFILE::CFile::Exists(PROFILE + "ports.xml"));
  EXPECT_FALSE(PortConnected("wii"));
  // Another platform, and a game no platform is known for, keep the game client's setup
  EXPECT_TRUE(PortConnected("gamecube"));
  EXPECT_TRUE(PortConnected(""));
}

TEST_F(TestPortManager, StartsAPlatformFromTheGameClientsSetup)
{
  UnplugInGame("");

  EXPECT_TRUE(XFILE::CFile::Exists(PROFILE + "ports.xml"));
  EXPECT_FALSE(PortConnected("gamecube"));
}
