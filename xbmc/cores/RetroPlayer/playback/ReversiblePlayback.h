/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLoop.h"
#include "IPlayback.h"
#include "threads/CriticalSection.h"
#include "utils/Observer.h"

#include <atomic>
#include <future>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <vector>

class CDateTime;

namespace KODI
{
namespace GAME
{
class CGameClient;
}

namespace RETRO
{
class CGUIGameMessenger;
class CRPRenderManager;
class CRPStreamManager;
class CSavestateDatabase;
class IMemoryStream;

class CReversiblePlayback : public IPlayback, public IGameLoopCallback, public Observer
{
public:
  CReversiblePlayback(GAME::CGameClient* gameClient,
                      CRPRenderManager& renderManager,
                      CRPStreamManager& streamManager,
                      CCheevos* cheevos,
                      CGUIGameMessenger& guiMessenger,
                      double fps,
                      size_t serializeSize);

  ~CReversiblePlayback() override;

  // implementation of IPlayback
  void Initialize() override;
  void Deinitialize() override;
  bool CanPause() const override { return true; }
  bool CanSeek() const override { return true; }
  unsigned int GetTimeMs() const override { return m_playTimeMs; }
  unsigned int GetTotalTimeMs() const override { return m_totalTimeMs; }
  unsigned int GetCacheTimeMs() const override { return m_cacheTimeMs; }
  void SeekTimeMs(unsigned int timeMs) override;
  double GetSpeed() const override;
  void SetSpeed(double speedFactor) override;
  void PauseAsync() override;
  std::string CreateSavestate(bool autosave, const std::string& savestatePath = "") override;
  bool LoadSavestate(const std::string& savestatePath) override;

  // implementation of IGameLoopCallback
  void FrameEvent() override;
  void RewindEvent() override;
  void EndEvent() override;

  // implementation of Observer
  void Notify(const Observable& obs, const ObservableMessage msg) override;

private:
  /*!
   * \brief Run one frame, showing the player one from several frames later
   *
   * The emulator is run forward past the frame that is really happening, and
   * the picture and sound from that later frame are what reach the player;
   * the emulator is then put back to where it truly is. The player sees their
   * input take effect earlier than the emulator could otherwise manage,
   * because the frame they are shown already contains the answer to it.
   *
   * Costs a whole extra run of the client per hidden frame.
   *
   * \param frames How many frames ahead to look, at least 1
   *
   * \return True if the sequence ran and the frame is accounted for
   */
  bool RunaheadFrameEvent(unsigned int frames);

  /*!
   * \brief How many frames to run ahead right now, or 0 for none
   *
   * Asked afresh each frame: a client cannot always say how large its state
   * is until it has run one (Dolphin builds the machine it serializes during
   * the game's boot), so a client that is ineligible at the first frame may
   * become eligible at the second.
   */
  unsigned int GetRunaheadFrames() const;

  //! \param serialized A state already taken from the client this frame, to
  //!                   save serializing it a second time, or empty to ask
  void AddFrame(const std::vector<uint8_t>& serialized = {});
  void UpdateFrameRate();
  void RewindFrames(uint64_t frames);
  void AdvanceFrames(uint64_t frames);
  void UpdatePlaybackStats();
  void UpdateMemoryStream();
  void UpdateRunahead();
  void CommitSavestate(bool autosave,
                       const std::string& savePath,
                       const CDateTime& nowUTC,
                       uint64_t timestampFrames);

  // Construction parameter
  GAME::CGameClient* const m_gameClient;
  CRPRenderManager& m_renderManager;
  CRPStreamManager& m_streamManager;
  CCheevos* const m_cheevos;
  CGUIGameMessenger& m_guiMessenger;

  // Gameplay functionality
  CGameLoop m_gameLoop;
  std::unique_ptr<IMemoryStream> m_memoryStream;
  CCriticalSection m_mutex;

  // Savestate functionality
  std::unique_ptr<CSavestateDatabase> m_savestateDatabase;
  std::string m_autosavePath{};
  std::vector<std::future<void>> m_savestateThreads;
  CCriticalSection m_savestateMutex;

  // Run-ahead functionality
  //
  // The settings observer runs on whatever thread changed the setting, and the
  // sequence runs on the game loop, so what the observer touches is limited to
  // these three. The buffers belong to the game loop alone: freeing them from
  // the observer pulled the state out from under a sequence that was midway
  // through running, and the client was handed a pointer to freed memory.
  std::atomic<bool> m_runaheadEnabled{false};
  std::atomic<unsigned int> m_runaheadFrameCount{0};
  std::atomic<bool> m_runaheadFailed{false};

  //! \brief Whether the state-too-large refusal has been said once already
  mutable std::atomic<bool> m_runaheadStateTooLarge{false};

  // Owned by the game loop thread
  std::vector<uint8_t> m_runaheadState;
  std::vector<uint8_t> m_runaheadAchievementState;

  // Playback stats
  uint64_t m_totalFrameCount = 0;
  uint64_t m_pastFrameCount = 0;
  uint64_t m_futureFrameCount = 0;
  unsigned int m_playTimeMs = 0;
  unsigned int m_totalTimeMs = 0;
  unsigned int m_cacheTimeMs = 0;
};
} // namespace RETRO
} // namespace KODI
