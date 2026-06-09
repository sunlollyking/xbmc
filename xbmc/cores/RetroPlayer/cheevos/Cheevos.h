/*
 *  Copyright (C) 2020-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "RConsoleIDs.h"
#include "../rcheevos/include/rc_client.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace KODI
{
namespace GAME
{
class CGameClient;
}

namespace RETRO
{
class CCheevos
{
public:
  ~CCheevos();
  CCheevos(GAME::CGameClient* gameClient,
           const std::string& userName,
           const std::string& loginToken);

  void ResetRuntime();
  void DoFrame();
  bool LoadData();
  void EnableRichPresence();
  std::string GetRichPresenceEvaluation();

  void ActivateAchievement();
  static void CallbackUrlId(const std::string& achievementUrl, unsigned int cheevoId);
  void CheckTriggeredAchievement();

  /*!
   * \brief Perform the actual HTTP login exchange with RetroAchievements
   *
   * Call this with the user's PASSWORD when they press the Login button.
   * On success the returned token is stored internally and persisted to
   * settings — the password is never stored.
   *
   * \param password  The user's account password (not a token)
   *
   * \return true on successful login.
   */
  bool RCLogin(const std::string& password);

private:
  void RichPresencePingThread();

  // Helper functions
  RConsoleID ConsoleID();

  GAME::CGameClient* m_gameClient;
  rc_client_t* m_rcClient{nullptr};

  // rcheevos callbacks
  static uint32_t RcheevosReadMemory(uint32_t address, uint8_t* buffer,
                                     uint32_t numBytes, rc_client_t* client);
  static void RcheevosServerCall(const rc_api_request_t* request,
                                 rc_client_server_callback_t callback,
                                 void* callbackData, rc_client_t* client);
  static void RcheevosLoginCallback(int result, const char* errorMessage,
                                    rc_client_t* client, void* userData);
  static void RcheevosGameLoadCallback(int result, const char* errorMessage,
                                       rc_client_t* client, void* userData);
  static void RcheevosEventHandler(const rc_client_event_t* event, rc_client_t* client);
  std::string m_userName;
  std::string m_loginToken;

  bool m_richPresenceLoaded{false};
  std::string m_richPresenceScript;

  // FIX: was static in the original — state leaked between game sessions
  std::unordered_map<unsigned, std::vector<std::string>> m_activatedCheevoMap;
  std::string m_gameTitle;
  unsigned int m_gameId{0};

  // Static map so CallbackUrlId can look up titles
  static std::unordered_map<unsigned, std::pair<std::string, std::string>> s_cheevoTitles;
  static std::mutex s_cheevoTitlesMutex;

  // Set true when RA flags this emulator as unsupported
  bool m_unsupportedEmulator{false};
  // Persistent achievement callback — must outlive game session, so stored as member
  std::function<void(const std::string&, unsigned int)> m_cheevoCallback;
  // Rich presence periodic ping
  std::atomic<bool> m_richPresenceRunning{false};
  std::thread m_richPresenceThread;
};
} // namespace RETRO
} // namespace KODI
