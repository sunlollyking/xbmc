/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ReversiblePlayback.h"

#include "SavestateCapture.h"
#include "ServiceBroker.h"
#include "XBDateTime.h"
#include "addons/AddonVersion.h"
#include "cores/RetroPlayer/guibridge/GUIGameMessenger.h"
#include "cores/RetroPlayer/rendering/RPRenderManager.h"
#include "cores/RetroPlayer/savestates/ISavestate.h"
#include "cores/RetroPlayer/savestates/SavestateDatabase.h"
#include "cores/RetroPlayer/streams/RPStreamManager.h"
#include "cores/RetroPlayer/streams/memory/DeltaPairMemoryStream.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/File.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "games/addons/GameClient.h"
#include "games/addons/disc/GameClientDiscModel.h"
#include "games/addons/disc/GameClientDiscs.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/MathUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using namespace KODI;
using namespace RETRO;
using GAME::RestoreResult;

#define REWIND_FACTOR 0.25 // Rewind at 25% of gameplay speed

namespace
{
/*!
 * \brief The largest savestate run-ahead will work with
 *
 * Every frame costs one state taken and one put back, so the price is the size
 * of the state. Clients at or under 1 MB were measured running at full speed
 * two frames ahead; nothing larger has been shown to. Set just above the
 * largest proven size, and meant to be raised once the bigger clients have been
 * measured rather than left as a permanent ceiling.
 */
constexpr size_t MAX_RUNAHEAD_STATE_SIZE = 2 * 1024 * 1024;

constexpr unsigned int TOAST_DISPLAY_TIME_MS = 5000;

/*!
 * \brief Whether hardcore mode is currently blocking gameplay assistance
 *
 * RetroAchievements requires save state loading, rewind, slow motion and
 * cheats to be unavailable while hardcore is on. Saving states is still
 * allowed, and so is fast forward.
 */
bool HardcoreRestrictionsApply()
{
  return CServiceBroker::GetGameServices().GameSettings().GetAchievementsHardcore();
}

/*!
 * \brief Tell the player why what they asked for didn't happen
 *
 * Silently ignoring the request would read as a broken control.
 */
void NotifyBlockedByHardcore(uint32_t featureStringId)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  // "Hardcore mode", "{0:s} is not available". The mode heads the toast so the
  // longest feature name still fits the notification's fixed width.
  CGUIDialogKaiToast::QueueNotification(
      CServiceBroker::GetGameServices().GameSettings().GetRAUserPicUrl(), strings.Get(35700),
      StringUtils::Format(strings.Get(35305), strings.Get(featureStringId)), TOAST_DISPLAY_TIME_MS);
}
} // namespace

CReversiblePlayback::CReversiblePlayback(GAME::CGameClient* gameClient,
                                         CRPRenderManager& renderManager,
                                         CGUIGameMessenger& guiMessenger,
                                         double fps,
                                         size_t serializeSize,
                                         CRPStreamManager* streamManager /* = nullptr */)
  : m_gameClient(gameClient),
    m_renderManager(renderManager),
    m_streamManager(streamManager),
    m_guiMessenger(guiMessenger),
    m_gameLoop(this, fps),
    m_savestateDatabase(new CSavestateDatabase),
    m_memorySize(serializeSize),
    m_gamePath(gameClient->GetGamePath()),
    m_gameClientId(gameClient->ID()),
    m_gameClientVersion(gameClient->Version().asString())
{
  InitializeSaveWorker();
  UpdateMemoryStream();

  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  gameSettings.RegisterObserver(this);

  UpdateRunahead();
}

CReversiblePlayback::~CReversiblePlayback()
{
  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  gameSettings.UnregisterObserver(this);

  Deinitialize();
  m_saveWorker.reset();
}

void CReversiblePlayback::InitializeSaveWorker()
{
  if (m_saveWorker)
    return;
  if (m_memorySize == 0)
    m_memorySize = m_gameClient->GetSerializeSize();
  if (m_memorySize != 0 && !m_gamePath.empty())
  {
    auto snapshot = std::make_unique<Snapshot>();
    snapshot->memory =
        std::make_unique<uint32_t[]>((m_memorySize + sizeof(uint32_t) - 1) / sizeof(uint32_t));
    m_saveWorker = std::make_unique<CSavestateWorker<Snapshot>>(
        std::move(snapshot),
        [this](Snapshot& captured)
        {
          try
          {
            if (!captured.discarded)
            {
              m_saveSucceeded.store(false);
              m_saveSucceeded.store(CommitSavestate(captured));
            }
          }
          catch (const std::exception& e)
          {
            CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Save failed: {}", e.what());
          }
          catch (...)
          {
            CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Save failed");
          }
          captured.video.clear();
        });
  }
}

void CReversiblePlayback::Initialize()
{
  UpdateMemoryStream();
  m_gameLoop.Start();
}

void CReversiblePlayback::Quiesce()
{
  m_gameLoop.Quiesce();

  std::unique_lock lock(m_mutex);
  CancelAutosave();
  m_autosaveCapture.Reset();
  if (m_saveWorker)
    m_saveWorker->Drain();
}

void CReversiblePlayback::Deinitialize()
{
  Quiesce();
  m_gameLoop.Stop();

  std::unique_lock lock(m_mutex);
  m_memoryStream.reset();
  m_discStateHistory.Clear();
}

void CReversiblePlayback::SeekTimeMs(unsigned int timeMs)
{
  std::unique_lock lock(m_mutex);
  if (m_restoreFailed)
    return;

  const double previousSpeed = m_gameLoop.GetSpeed();
  const int offsetTimeMs = timeMs - GetTimeMs();
  const int offsetFrames = MathUtils::round_int(offsetTimeMs / 1000.0 * m_gameLoop.FPS());

  if (offsetFrames > 0)
  {
    const uint64_t frames = std::min(static_cast<uint64_t>(offsetFrames), m_futureFrameCount);
    if (frames > 0)
    {
      m_gameLoop.SetSpeed(0.0);
      if (AdvanceFrames(frames) != RestoreResult::StateUncertain)
        m_gameLoop.SetSpeed(previousSpeed);
    }
  }
  else if (offsetFrames < 0)
  {
    const uint64_t frames = std::min(static_cast<uint64_t>(-offsetFrames), m_pastFrameCount);
    if (frames > 0)
    {
      m_gameLoop.SetSpeed(0.0);
      if (RewindFrames(frames) != RestoreResult::StateUncertain)
        m_gameLoop.SetSpeed(previousSpeed);
    }
  }
}

double CReversiblePlayback::GetSpeed() const
{
  return m_gameLoop.GetSpeed();
}

void CReversiblePlayback::SetSpeed(double speedFactor)
{
  if (HardcoreRestrictionsApply())
  {
    // Rewind runs the game backwards, so it arrives here as a negative speed
    if (speedFactor < 0.0)
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Refusing to rewind in hardcore mode");
      NotifyBlockedByHardcore(35309); // "Rewind"
      m_gameLoop.SetSpeed(1.0);
      return;
    }

    // Slow motion is withheld, fast forward is not. Pausing is fine.
    if (speedFactor > 0.0 && speedFactor < 1.0)
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Refusing to slow down in hardcore mode");
      NotifyBlockedByHardcore(35701); // "Slow motion"
      m_gameLoop.SetSpeed(1.0);
      return;
    }
  }

  std::unique_lock lock(m_mutex);
  if (speedFactor != 0.0)
  {
    if (m_restoreFailed)
      return;
  }

  if (speedFactor >= 0.0)
    m_gameLoop.SetSpeed(speedFactor);
  else
    m_gameLoop.SetSpeed(speedFactor * REWIND_FACTOR);
}

void CReversiblePlayback::PauseAsync()
{
  m_gameLoop.PauseAsync();
}

std::string CReversiblePlayback::GetSavestatePath(bool autosave,
                                                  const std::string& path,
                                                  const CDateTime& created)
{
  std::unique_lock lock(m_savestateMutex);
  std::string savePath = path;
  if (autosave && savePath.empty())
    savePath = m_autosavePath;
  if (!autosave && savePath == m_autosavePath)
    m_autosavePath.clear();
  if (savePath.empty())
    savePath = CSavestateDatabase::MakeSavestatePath(m_gamePath, created);
  if (autosave)
    m_autosavePath = savePath;
  return savePath;
}

std::string CReversiblePlayback::CreateSavestate(bool autosave, const std::string& savestatePath)
{
  std::unique_lock lock(m_mutex);
  m_saveSucceeded.store(false);
  if (m_restoreFailed)
    return "";
  InitializeSaveWorker();
  if (!m_saveWorker)
    return "";

  // Explicit saves may wait for storage; the periodic path only uses try-locks.
  CancelAutosave();
  std::unique_ptr<Snapshot> snapshot;
  if (m_rewindFrameRendered)
  {
    snapshot = m_saveWorker->Acquire();
    m_saveSucceeded.store(false);
    auto clientLock = m_gameClient->LockForSnapshot();
    const auto* discModel =
        m_memoryStream ? m_discStateHistory.Get(m_memoryStream->GetDiscStateID()) : nullptr;
    if (!m_memoryStream || !m_memoryStream->CurrentFrame() ||
        m_memoryStream->FrameSize() != m_memorySize ||
        (m_memoryStream->GetDiscStateID() != 0 && !discModel) ||
        (discModel && !(m_gameClient->Discs().GetDiscsForSnapshot() == *discModel)))
    {
      m_saveWorker->Release(snapshot);
      return "";
    }
    // The preview ran ahead of the rewind cursor; persist the cursor's machine and media.
    std::memcpy(snapshot->memory.get(), m_memoryStream->CurrentFrame(), m_memorySize);
    CaptureMetadata(*snapshot);
    snapshot->frames = m_memoryStream->GetFrameCounter();
    snapshot->wallClock = snapshot->frames / m_gameLoop.FPS();
    snapshot->discState = discModel ? std::make_optional(discModel->GetState()) : std::nullopt;
    snapshot->achievements.clear();
  }
  else
  {
    snapshot = CaptureSavestate(*m_saveWorker, *m_gameClient, m_memorySize,
                                [this](Snapshot& captured) { CaptureMetadata(captured); });
  }
  m_saveSucceeded.store(false);
  if (!snapshot)
    return "";
  snapshot->autosave = autosave;
  snapshot->rewind = m_rewindFrameRendered;
  snapshot->serializeUs = 0;
  snapshot->captureUs = 0;
  snapshot->path = GetSavestatePath(autosave, savestatePath, snapshot->created);
  const std::string savePath = snapshot->path;
  m_renderManager.CacheVideoFrame(savePath);
  m_saveWorker->Submit(snapshot);
  return savePath;
}

void CReversiblePlayback::CaptureMetadata(Snapshot& snapshot)
{
  snapshot.created = CDateTime::GetUTCDateTime();
  snapshot.frames = m_totalFrameCount;
  snapshot.wallClock = snapshot.frames / m_gameLoop.FPS();
  snapshot.path.clear();
  snapshot.autosave = true;
  snapshot.discarded = false;
  snapshot.discState.reset();
  snapshot.achievements.clear();
  if (!m_rewindFrameRendered)
  {
    if (m_gameClient->SupportsDiscControl())
    {
      m_gameClient->Discs().RefreshDiscStateLive();
      snapshot.discState = m_gameClient->Discs().GetDiscsForSnapshot().GetState();
    }
    m_gameClient->SerializeAchievementState(snapshot.achievements);
  }
  m_renderManager.TryCaptureVideoFrame(snapshot.video);
}

void CReversiblePlayback::InvalidateAutosave()
{
  m_autosaveCapture.Cancel(m_pendingSnapshot, m_snapshotReady);
}

void CReversiblePlayback::CancelAutosave()
{
  InvalidateAutosave();
  if (m_pendingSnapshot)
    m_saveWorker->Submit(m_pendingSnapshot);
  m_snapshotReady = false;
}

bool CReversiblePlayback::WaitForSavestates()
{
  std::unique_lock lock(m_mutex);
  CancelAutosave();
  if (m_saveWorker && m_saveWorker->Drain())
  {
    m_saveSucceeded.store(false);
    CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Save worker failed");
    return false;
  }
  return m_saveSucceeded.load();
}

bool CReversiblePlayback::CommitSavestate(const Snapshot& snapshot)
{
  const auto started = std::chrono::steady_clock::now();
  const std::string savePath = snapshot.path.empty()
                                   ? GetSavestatePath(snapshot.autosave, "", snapshot.created)
                                   : snapshot.path;
  auto savestate = CSavestateDatabase::AllocateSavestate();
  std::unique_ptr<ISavestate> loadedSavestate;
  if (snapshot.discState)
    savestate->SetDiscState(*snapshot.discState);
  uint8_t* memoryData = savestate->GetMemoryBuffer(m_memorySize);
  std::memcpy(memoryData, snapshot.memory.get(), m_memorySize);
  if (!snapshot.achievements.empty())
  {
    if (uint8_t* data = savestate->GetAchievementBuffer(snapshot.achievements.size()))
      std::memcpy(data, snapshot.achievements.data(), snapshot.achievements.size());
  }

  {
    std::unique_lock lock(m_savestateMutex);
    if (XFILE::CFile::Exists(savePath))
    {
      loadedSavestate = CSavestateDatabase::AllocateSavestate();
      if (!m_savestateDatabase->GetSavestate(savePath, *loadedSavestate))
        loadedSavestate.reset();
    }
  }
  savestate->SetType(snapshot.autosave ? SAVE_TYPE::AUTO : SAVE_TYPE::MANUAL);
  savestate->SetLabel(loadedSavestate ? loadedSavestate->Label() : "");
  savestate->SetCaption(CServiceBroker::GetGameServices().AchievementRuntime().GetRichPresence());
  savestate->SetCreated(snapshot.created);
  savestate->SetGameFileName(URIUtils::GetFileName(m_gamePath));
  savestate->SetTimestampFrames(snapshot.frames);
  savestate->SetTimestampWallClock(snapshot.wallClock);
  savestate->SetGameClientID(m_gameClientId);
  savestate->SetGameClientVersion(m_gameClientVersion);

  if (!snapshot.video.empty())
    m_renderManager.CacheVideoFrame(savePath, snapshot.video);
  m_renderManager.SaveVideoFrame(savePath, *savestate, snapshot.video);
  savestate->Finalize();
  bool success;
  {
    std::unique_lock lock(m_savestateMutex);
    success = m_savestateDatabase->AddSavestate(savePath, m_gamePath, *savestate);
  }
  if (success)
    m_renderManager.SaveThumbnail(CSavestateDatabase::MakeThumbnailPath(savePath));
  else
    CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Failed to write savestate");
  m_guiMessenger.RefreshSavestates(savePath, savestate.get());
  CLog::Log(LOGDEBUG,
            "RetroPlayer[SAVE]: Frame {}: reused rewind {}, core {} us, capture/handoff {} us, "
            "background commit {} ms, success {}",
            snapshot.frames, snapshot.rewind, snapshot.serializeUs, snapshot.captureUs,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                  started)
                .count(),
            success);
  return success;
}

bool CReversiblePlayback::LoadSavestate(const std::string& savestatePath)
{
  // Every route that loads a state comes through here - the in-game dialog,
  // JSON-RPC, the Python player API - so hardcore is answered once, rather
  // than at each caller. Creating a state is still allowed.
  if (HardcoreRestrictionsApply())
  {
    CLog::Log(LOGINFO, "RetroPlayer[SAVE]: Refusing to load a savestate in hardcore mode");
    NotifyBlockedByHardcore(35308); // "Loading save states"
    return false;
  }

  const size_t memorySize =
      m_gameClient->GetSerializeSize(GAME::CGameClient::SerializeSizeMode::Restore);

  // Game client must support serialization
  if (memorySize == 0)
    return false;

  std::unique_lock playbackLock(m_mutex);
  CancelAutosave();
  if (m_saveWorker)
    m_saveWorker->Drain();
  auto clientLock = m_gameClient->LockForSnapshot();
  bool bSuccess = false;

  std::unique_ptr<ISavestate> savestate = CSavestateDatabase::AllocateSavestate();
  if (m_savestateDatabase->GetSavestate(savestatePath, *savestate))
  {
    if (!savestate->PrepareMemoryData(memorySize))
    {
      CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Failed to prepare memory data");
    }
    else if (savestate->GetMemorySize() != memorySize)
    {
      CLog::Log(LOGERROR, "Invalid memory size, got {}, expected {}", savestate->GetMemorySize(),
                memorySize);
    }
    else
    {
      std::optional<GAME::CGameClientDiscModel> discModel;
      if (const auto discState = savestate->GetDiscState())
      {
        discModel.emplace();
        if (!m_gameClient->SupportsDiscControl() ||
            !m_gameClient->Discs().GetDiscsForSnapshot().ResolveState(*discState, *discModel))
        {
          CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Failed to resolve saved disc state");
          return false;
        }
        if (!m_gameClient->Discs().IsMediaSupported(*discModel))
        {
          CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Saved media is unsupported by the active core");
          return false;
        }
        CLog::Log(LOGDEBUG,
                  "RetroPlayer[SAVE]: Restoring disc state: slots={} selected={} ejected={}",
                  discState->slots.size(), discState->selectedSlot, discState->trayEjected);
      }

      const RestoreResult result = m_gameClient->Deserialize(savestate->GetMemoryData(), memorySize,
                                                             discModel ? &*discModel : nullptr);
      if (result == RestoreResult::Restored)
      {
        // After the emulator, so the runtime matches its machine state, and
        // unconditionally: a savestate written before this existed, or while
        // signed out, carries none, but the client still has to be told the
        // machine state jumped. Left untold, the progress it holds for the
        // timeline being abandoned would survive the restore.
        const uint8_t* const achievementData = savestate->GetAchievementData();
        const size_t achievementSize = savestate->GetAchievementSize();

        if (!m_gameClient->DeserializeAchievements(achievementData, achievementSize) &&
            achievementData != nullptr && achievementSize != 0)
        {
          // State the runtime would not take, from another runtime version or
          // a damaged file. Ask for a reset rather than leaving it: what it
          // still holds describes the timeline just abandoned, and carrying
          // that forward is how an achievement gets awarded unearned. The
          // savestate itself is fine, so the load is not failed for it.
          CLog::Log(LOGWARNING, "RetroPlayer[SAVE]: Achievement state refused, resetting runtime");

          m_gameClient->DeserializeAchievements(nullptr, 0);
        }

        if (m_memoryStream)
        {
          const uint64_t maxFrames = m_memoryStream->MaxFrameCount();
          m_memoryStream->Init(memorySize, maxFrames);
          m_discStateHistory.Clear();
          std::memcpy(m_memoryStream->BeginFrame(), savestate->GetMemoryData(), memorySize);
          const uint32_t discStateId =
              m_gameClient->SupportsDiscControl()
                  ? m_discStateHistory.Intern(m_gameClient->Discs().GetDiscsForSnapshot())
                  : 0;
          m_memoryStream->SubmitFrame(discStateId, savestate->TimestampFrames());
          UpdatePlaybackStats();
        }
        m_totalFrameCount = savestate->TimestampFrames();
        m_restoreFailed = false;
        m_rewindFrameRendered = false;
        bSuccess = true;
        if (savestate->Type() == SAVE_TYPE::AUTO)
        {
          std::unique_lock savestateLock(m_savestateMutex);
          m_autosavePath = savestatePath;
        }
      }
      else if (result == RestoreResult::StateUncertain)
      {
        LatchRestoreFailure();
      }
    }
  }

  return bSuccess;
}

void CReversiblePlayback::FrameEvent()
{
  std::unique_lock lock(m_mutex);
  if (!m_restoreFailed && !m_rewindFrameRendered)
  {
    // Input scanning calls back into the client from another thread.
    lock.unlock();
    m_gameClient->PollInput();
    lock.lock();
  }
  std::unique_lock clientLock = m_gameClient->LockForSnapshot();

  if (m_restoreFailed)
    return;

  // The rewind preview has already run and updated the frame rate.
  if (!m_rewindFrameRendered)
  {
    if (const unsigned int runaheadFrames = GetRunaheadFrames(); runaheadFrames > 0)
    {
      if (RunaheadFrameEvent(runaheadFrames))
        return;

      // The sequence could not be completed, so fall through and run the frame
      // the ordinary way rather than dropping it
    }
    else if (!m_runaheadState.empty())
    {
      // Run-ahead has been turned off, and this is the thread that owns the
      // buffers, so this is where they are safe to release
      m_runaheadState.clear();
      m_runaheadState.shrink_to_fit();
      m_runaheadAchievementState.clear();
      m_runaheadAchievementState.shrink_to_fit();
    }

    m_gameClient->RunFrame(false);
    UpdateFrameRate();

    if (!m_memoryStreamSized)
      UpdateMemoryStream();
  }

  InitializeSaveWorker();
  AddFrame();
}

void CReversiblePlayback::RewindEvent()
{
  m_gameClient->PollInput();

  std::unique_lock lock(m_mutex);
  std::unique_lock clientLock = m_gameClient->LockForSnapshot();

  if (m_restoreFailed || RewindFrames(1) != RestoreResult::Restored)
    return;

  m_gameClient->RunFrame(false);
  m_rewindFrameRendered = true;
  UpdateFrameRate();
}

void CReversiblePlayback::EndEvent()
{
  // Deliberately does not destroy the rendering context.
  //
  // The game loop ends before the client is unloaded, and a hardware-rendering
  // client releases its GPU resources as it unloads. Destroying the context
  // here leaves those calls to land on whatever context is current by then --
  // Kodi's own -- where they unbind the vertex array object every one of its
  // draws depends on, and the GUI renders nothing from that point on.
  //
  // The context is destroyed when the rendering stream closes, which happens
  // while the client is unloading and its context is still current.
}

void CReversiblePlayback::AddFrame(const std::vector<uint8_t>& serialized /* = {} */)
{
  // Playback lock precedes the client lock for every snapshot and timeline change.
  auto clientLock = m_gameClient->LockForSnapshot();
  int64_t serializeUs = 0;
  bool captured = false;
  if (m_memoryStream)
  {
    const size_t frameSize = m_memoryStream->FrameSize();
    const bool measure = m_autosaveCapture.IsPending();
    const auto started =
        measure ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};

    // Run-ahead has already taken a state from the client this frame. Copying
    // it is what keeps rewind and run-ahead together down to one serialize per
    // frame rather than two.
    if (serialized.size() == frameSize)
    {
      std::memcpy(m_memoryStream->BeginFrame(), serialized.data(), frameSize);
      captured = true;
    }
    else
    {
      captured = m_gameClient->Serialize(m_memoryStream->BeginFrame(), frameSize);
    }

    if (measure)
      serializeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count();

    if (captured)
    {
      uint32_t discStateId = 0;
      if (m_gameClient->SupportsDiscControl())
      {
        m_gameClient->Discs().RefreshDiscStateLive();
        discStateId = m_discStateHistory.Intern(m_gameClient->Discs().GetDiscsForSnapshot());
      }
      m_memoryStream->SubmitFrame(discStateId, m_totalFrameCount + 1);
      UpdatePlaybackStats();
    }
  }
  ++m_totalFrameCount;
  m_rewindFrameRendered = false;
  ProcessAutosave(captured, serializeUs);
}

void CReversiblePlayback::ProcessAutosave(bool serialized, int64_t serializeUs)
{
  if (!m_saveWorker)
    return;

  if (m_snapshotReady)
  {
    if (m_saveWorker->TrySubmit(m_pendingSnapshot))
      m_snapshotReady = false;
    return;
  }
  if (!m_autosaveCapture.IsPending())
    return;
  if (!m_pendingSnapshot)
    m_pendingSnapshot = m_saveWorker->TryAcquire();
  if (!m_pendingSnapshot)
    return;

  auto& snapshot = *m_pendingSnapshot;
  const auto started = std::chrono::steady_clock::now();
  m_snapshotReady = m_autosaveCapture.CaptureFrame(
      m_memoryStream.get(), serialized, snapshot.memory,
      [this, &snapshot]
      {
        const auto coreStarted = std::chrono::steady_clock::now();
        const bool success = m_gameClient->Serialize(
            reinterpret_cast<uint8_t*>(snapshot.memory.get()), m_memorySize);
        snapshot.serializeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::steady_clock::now() - coreStarted)
                                   .count();
        return success;
      },
      [this, &snapshot, serialized, serializeUs]
      {
        CaptureMetadata(snapshot);
        snapshot.rewind = serialized;
        if (serialized)
          snapshot.serializeUs = serializeUs;
        snapshot.captureUs = 0;
      });
  snapshot.captureUs += std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - started)
                            .count();
  if (m_snapshotReady)
  {
    if (m_saveWorker->TrySubmit(m_pendingSnapshot))
      m_snapshotReady = false;
  }
}

void CReversiblePlayback::UpdateFrameRate()
{
  const double previousFrameRate = m_gameLoop.FPS();
  m_gameLoop.SetFrameRate(m_gameClient->GetFrameRate());

  if (m_gameLoop.FPS() != previousFrameRate)
    UpdateMemoryStream();
}

RestoreResult CReversiblePlayback::RewindFrames(uint64_t frames)
{
  std::unique_lock lock(m_mutex);
  InvalidateAutosave();
  std::unique_lock clientLock = m_gameClient->LockForSnapshot();

  RestoreResult result = RestoreResult::Rejected;
  if (m_memoryStream)
  {
    const uint64_t rewound = m_memoryStream->RewindFrames(frames);
    if (rewound > 0)
    {
      const RestoreResult targetResult = RestoreFrame();
      if (targetResult == RestoreResult::Restored)
      {
        m_totalFrameCount = m_memoryStream->GetFrameCounter();
        UpdatePlaybackStats();
        return RestoreResult::Restored;
      }
      else
      {
        const uint64_t rolledBack = m_memoryStream->AdvanceFrames(rewound);
        if (rolledBack != rewound || (targetResult == RestoreResult::StateUncertain &&
                                      RestoreFrame() != RestoreResult::Restored))
        {
          result = RestoreResult::StateUncertain;
          LatchRestoreFailure();
        }
      }
    }
    UpdatePlaybackStats();
  }

  return result;
}

RestoreResult CReversiblePlayback::AdvanceFrames(uint64_t frames)
{
  std::unique_lock lock(m_mutex);
  InvalidateAutosave();
  std::unique_lock clientLock = m_gameClient->LockForSnapshot();

  RestoreResult result = RestoreResult::Rejected;
  if (m_memoryStream)
  {
    const uint64_t advanced = m_memoryStream->AdvanceFrames(frames);
    if (advanced > 0)
    {
      const RestoreResult targetResult = RestoreFrame();
      if (targetResult == RestoreResult::Restored)
      {
        m_totalFrameCount = m_memoryStream->GetFrameCounter();
        UpdatePlaybackStats();
        return RestoreResult::Restored;
      }
      else
      {
        const uint64_t rolledBack = m_memoryStream->RewindFrames(advanced);
        if (rolledBack != advanced || (targetResult == RestoreResult::StateUncertain &&
                                       RestoreFrame() != RestoreResult::Restored))
        {
          result = RestoreResult::StateUncertain;
          LatchRestoreFailure();
        }
      }
    }
    UpdatePlaybackStats();
  }

  return result;
}

RestoreResult CReversiblePlayback::RestoreFrame()
{
  const uint32_t discStateId = m_memoryStream->GetDiscStateID();
  const auto* discModel = m_discStateHistory.Get(discStateId);
  if (discStateId != 0 && !discModel)
  {
    CLog::Log(LOGERROR, "RetroPlayer[DISC]: Missing rewind disc state {}", discStateId);
    return RestoreResult::Rejected;
  }
  if (discModel && !(m_gameClient->Discs().GetDiscsForSnapshot() == *discModel))
  {
    const auto selected = discModel->GetSelectedDiscIndex();
    CLog::Log(LOGDEBUG,
              "RetroPlayer[DISC]: Restoring rewind disc state {}: slots={} selected={} ejected={}",
              discStateId, discModel->Size(), selected ? static_cast<int64_t>(*selected) : -1,
              discModel->IsEjected());
  }
  const RestoreResult result = m_gameClient->Deserialize(m_memoryStream->CurrentFrame(),
                                                         m_memoryStream->FrameSize(), discModel);
  if (result == RestoreResult::Restored)
    m_rewindFrameRendered = false;
  return result;
}

void CReversiblePlayback::LatchRestoreFailure()
{
  InvalidateAutosave();
  if (!m_restoreFailed)
    CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Machine state restore failed, pausing playback");
  m_restoreFailed = true;
  m_gameLoop.PauseAsync();
}

void CReversiblePlayback::UpdatePlaybackStats()
{
  m_pastFrameCount = m_memoryStream->PastFramesAvailable();
  m_futureFrameCount = m_memoryStream->FutureFramesAvailable();

  const uint64_t played = m_pastFrameCount + (m_memoryStream->CurrentFrame() ? 1 : 0);
  const uint64_t total = m_memoryStream->MaxFrameCount();
  const uint64_t cached = m_futureFrameCount;

  m_playTimeMs = MathUtils::round_int(1000.0 * played / m_gameLoop.FPS());
  m_totalTimeMs = MathUtils::round_int(1000.0 * total / m_gameLoop.FPS());
  m_cacheTimeMs = MathUtils::round_int(1000.0 * cached / m_gameLoop.FPS());
}

void CReversiblePlayback::Notify(const Observable& obs, const ObservableMessage msg)
{
  switch (msg)
  {
    case ObservableMessageSettingsChanged:
      UpdateMemoryStream();
      UpdateRunahead();
      break;
    default:
      break;
  }
}

void CReversiblePlayback::UpdateMemoryStream()
{
  std::unique_lock lock(m_mutex);

  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  // Hardcore forbids rewind, so the buffer isn't merely unused - it shouldn't
  // be allocated at all. It costs a fraction of the savestate size for every
  // frame of the rewind window, which is not free on consoles with large
  // states.
  const bool rewindEnabled = gameSettings.RewindEnabled() && !HardcoreRestrictionsApply();
  const size_t memorySize = rewindEnabled ? m_gameClient->GetSerializeSize() : 0;

  if (rewindEnabled && memorySize > 0)
  {
    unsigned int rewindBufferSec = gameSettings.MaxRewindTimeSec();
    if (rewindBufferSec < 10)
      rewindBufferSec = 10; // Sanity check

    unsigned int frameCount = MathUtils::round_int(rewindBufferSec * m_gameLoop.FPS());

    if (!m_memoryStream)
    {
      // Ceiling, not the real cost: the buffer keeps xor deltas of changed
      // words only. Worth logging because a large state and a long window put
      // that ceiling in the gigabytes.
      CLog::Log(LOGINFO,
                "RetroPlayer[SAVE]: Rewind buffer: {} frames of up to {} bytes ({:.1f} MB "
                "worst case) for {} seconds at {:.2f} fps",
                frameCount, memorySize,
                static_cast<double>(memorySize) * frameCount / (1024.0 * 1024.0), rewindBufferSec,
                m_gameLoop.FPS());

      m_memoryStream = std::make_unique<CDeltaPairMemoryStream>();
      m_memoryStream->Init(memorySize, frameCount);
    }

    if (m_memoryStream->MaxFrameCount() != frameCount)
    {
      m_memoryStream->SetMaxFrameCount(frameCount);
    }
  }
  else
  {
    InvalidateAutosave();
    m_memoryStream.reset();
    m_discStateHistory.Clear();

    // Reset playback stats
    m_pastFrameCount = 0;
    m_futureFrameCount = 0;
    m_playTimeMs = 0;
    m_totalTimeMs = 0;
    m_cacheTimeMs = 0;
  }

  m_memoryStreamSized = !rewindEnabled || m_memoryStream != nullptr;
}

unsigned int CReversiblePlayback::GetRunaheadFrames() const
{
  if (m_streamManager == nullptr)
    return 0;

  if (!m_runaheadEnabled || m_runaheadFrameCount == 0 || m_runaheadFailed)
    return 0;

  // Only while the game is running forward at its own speed. Fast-forward,
  // slow motion and rewind have all had the streams reconfigured underneath
  // them by OnSpeedChange, and looking into the future of a game being wound
  // backwards means nothing.
  if (m_gameLoop.GetSpeed() != 1.0)
    return 0;

  // A client that cannot serialize cannot be put back, and one that has not
  // run yet may not be able to say how large its state is
  const size_t memorySize = m_gameClient->GetSerializeSize();
  if (memorySize == 0)
    return 0;

  // Run-ahead takes a state and puts one back every single frame, so its cost
  // follows the size of that state while the emulation it hides does not.
  // Measured on a Ryzen 9: every client with a state of 1 MB or less -- NES,
  // Game Boy, Master System, Mega Drive, SNES, 32X -- runs at full speed two
  // frames ahead, the copying disappearing into the frame budget entirely.
  // Above that it has not been shown to work, and a client that cannot keep up
  // does not fail cleanly: it quietly runs at a fraction of full speed, which
  // reads as a broken emulator rather than a setting that costs too much.
  //
  // So this refuses rather than letting the player find out. The limit is set
  // just above the largest state proven to work, and is deliberately cautious;
  // it should move once the cost has been measured properly for the big ones.
  if (memorySize > MAX_RUNAHEAD_STATE_SIZE)
  {
    if (!m_runaheadStateTooLarge)
    {
      m_runaheadStateTooLarge = true;
      CLog::Log(LOGINFO,
                "RetroPlayer[SAVE]: Run-ahead held off: {} needs {:.1f} MB a frame, and the limit "
                "is {:.1f} MB. The emulator would run below full speed.",
                m_gameClient->ID(), static_cast<double>(memorySize) / (1024.0 * 1024.0),
                static_cast<double>(MAX_RUNAHEAD_STATE_SIZE) / (1024.0 * 1024.0));
    }
    return 0;
  }

  return m_runaheadFrameCount;
}

bool CReversiblePlayback::RunaheadFrameEvent(unsigned int frames)
{
  const size_t memorySize = m_gameClient->GetSerializeSize();

  // Whatever happens below, the player must not be left muted or blind
  struct CStreamRestore
  {
    explicit CStreamRestore(CRPStreamManager& streamManager) : m_streamManager(streamManager) {}
    ~CStreamRestore()
    {
      m_streamManager.EnableAudio(true);
      m_streamManager.EnableVideo(true);
    }
    CRPStreamManager& m_streamManager;
  } streamRestore(*m_streamManager);

  // The frame that is really happening. Its picture and sound are thrown away
  // -- the player is shown a later one instead -- but its input, polled by
  // FrameEvent(), is the input the whole sequence is predicting from.
  m_streamManager->EnableAudio(false);
  m_streamManager->EnableVideo(false);

  m_gameClient->RunFrame(false);
  UpdateFrameRate();

  if (!m_memoryStreamSized)
    UpdateMemoryStream();

  // Where the game truly is, and where it will be put back to
  m_runaheadState.resize(memorySize);
  if (!m_gameClient->Serialize(m_runaheadState.data(), memorySize))
  {
    // Without a state to return to, running further would carry the game away
    // from where it belongs. The frame that just ran still counts.
    CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Run-ahead disabled: client failed to serialize");
    m_runaheadFailed = true;
    AddFrame();
    return true;
  }

  // Saving and restoring the achievement runtime around the sequence is the
  // fallback, for clients that cannot run a frame without side effects. It puts
  // the runtime back, but it cannot unsend what the runtime already announced:
  // a challenge that ended on a speculative frame is reported ended, and then
  // reported started again by the restore, so the indicator flickers at frame
  // rate -- and an achievement unlocked on a frame that never happened has
  // already been queued for submission. A client that runs speculative frames
  // properly never announces any of it, so there is nothing to undo and none of
  // this work is done.
  const bool bProtectAchievements = !m_gameClient->RunsSpeculativeFrames();

  bool achievementsSaved = false;
  if (bProtectAchievements)
    achievementsSaved = m_gameClient->SerializeAchievementState(m_runaheadAchievementState);

  // Look into the future. These frames deliberately do not poll: they have to
  // answer to the same input as the frame that committed, or the picture the
  // player is shown predicts a button they never pressed.
  for (unsigned int frame = 1; frame <= frames; ++frame)
  {
    const bool bLastFrame = (frame == frames);

    // Only the furthest frame is seen and heard, so exactly one frame's worth
    // of sound is produced per frame of real time and the audio rate is
    // unchanged
    m_streamManager->EnableAudio(bLastFrame);
    m_streamManager->EnableVideo(bLastFrame);

    m_gameClient->RunFrame(false, true);
  }

  // Put the game back to where it really is. RestoreState() rather than
  // Deserialize(): the state came from this client moments ago and the disc
  // has not moved, so none of the disc handling a loaded savestate needs
  // applies sixty times a second.
  if (!m_gameClient->RestoreState(m_runaheadState.data(), memorySize))
  {
    // The game is now several frames further on than it should be. That is
    // survivable -- those frames really did run -- but the prediction cannot
    // be trusted again, so stop.
    CLog::Log(LOGERROR, "RetroPlayer[SAVE]: Run-ahead disabled: client failed to restore state");
    m_runaheadFailed = true;
    AddFrame();
    return true;
  }

  if (achievementsSaved)
    m_gameClient->DeserializeAchievements(m_runaheadAchievementState.data(),
                                          m_runaheadAchievementState.size());

  // The client is back at the state just serialized, so hand it to the rewind
  // buffer rather than asking for it a second time
  AddFrame(m_runaheadState);

  return true;
}

void CReversiblePlayback::UpdateRunahead()
{
  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  const bool bEnabled = gameSettings.RunaheadEnabled();
  const unsigned int frameCount = bEnabled ? gameSettings.RunaheadFrames() : 0;

  if (bEnabled == m_runaheadEnabled && frameCount == m_runaheadFrameCount)
    return;

  // Deliberately does not touch the state buffers. A sequence may be running
  // on the game loop this instant, holding a pointer into them and about to
  // hand it to the client; the game loop releases them itself once it sees
  // run-ahead is off.
  m_runaheadEnabled = bEnabled;
  m_runaheadFrameCount = frameCount;

  // A player who turned this on gets to try again after a client refused it,
  // and gets told again why if it is simply too big
  m_runaheadFailed = false;
  m_runaheadStateTooLarge = false;

  if (!bEnabled || frameCount == 0)
  {
    CLog::Log(LOGINFO, "RetroPlayer[SAVE]: Run-ahead disabled");
    return;
  }

  // Said out loud because the cost is a multiple of the whole emulator and is
  // otherwise invisible until the game will not hold its frame rate. Rewind
  // adds nothing to it -- the two share the one state taken per frame.
  CLog::Log(LOGINFO,
            "RetroPlayer[SAVE]: Run-ahead: {} frame(s) ahead, {} client run(s) per displayed "
            "frame, 1 serialize and 1 restore",
            frameCount, frameCount + 1);
}
