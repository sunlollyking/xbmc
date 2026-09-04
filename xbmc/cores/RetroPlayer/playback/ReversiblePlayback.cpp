/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ReversiblePlayback.h"

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
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "filesystem/File.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "games/addons/GameClient.h"
#include "utils/MathUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using namespace KODI;
using namespace RETRO;

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
 * \brief Tell the player why something they asked for didn't happen
 *
 * Silently ignoring the request would read as a broken control.
 */
void NotifyBlockedByHardcore(uint32_t featureStringId)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  // "Hardcore mode", "{0:s} is not available" - the mode heads the toast so the
  // longest feature name still fits the notification's fixed width
  CGUIDialogKaiToast::QueueNotification(
      CGUIDialogKaiToast::Info, strings.Get(35301),
      StringUtils::Format(strings.Get(35305), strings.Get(featureStringId)), TOAST_DISPLAY_TIME_MS);
}
} // namespace

CReversiblePlayback::CReversiblePlayback(GAME::CGameClient* gameClient,
                                         CRPRenderManager& renderManager,
                                         CRPStreamManager& streamManager,
                                         CGUIGameMessenger& guiMessenger,
                                         double fps,
                                         size_t serializeSize)
  : m_gameClient(gameClient),
    m_renderManager(renderManager),
    m_streamManager(streamManager),
    m_guiMessenger(guiMessenger),
    m_gameLoop(this, fps),
    m_savestateDatabase(new CSavestateDatabase)
{
  UpdateMemoryStream();
  UpdateRunahead();

  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  gameSettings.RegisterObserver(this);
}

CReversiblePlayback::~CReversiblePlayback()
{
  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  gameSettings.UnregisterObserver(this);

  Deinitialize();
}

void CReversiblePlayback::Initialize()
{
  m_gameLoop.Start();
}

void CReversiblePlayback::Deinitialize()
{
  // Wait for autosave tasks
  for (std::future<void>& task : m_savestateThreads)
    task.wait();
  m_savestateThreads.clear();

  m_gameLoop.Stop();
}

void CReversiblePlayback::SeekTimeMs(unsigned int timeMs)
{
  const int offsetTimeMs = timeMs - GetTimeMs();
  const int offsetFrames = MathUtils::round_int(offsetTimeMs / 1000.0 * m_gameLoop.FPS());

  if (offsetFrames > 0)
  {
    const uint64_t frames = std::min(static_cast<uint64_t>(offsetFrames), m_futureFrameCount);
    if (frames > 0)
    {
      m_gameLoop.SetSpeed(0.0);
      AdvanceFrames(frames);
      m_gameLoop.SetSpeed(1.0);
    }
  }
  else if (offsetFrames < 0)
  {
    // Seeking backwards is a rewind by another name, and it reaches
    // RewindFrames() without passing through SetSpeed(). Reachable from
    // JSON-RPC and the Python player API, so it is guarded in its own right
    // rather than relying on the buffer being empty.
    if (HardcoreRestrictionsApply())
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Refusing to seek backwards in hardcore mode");
      NotifyBlockedByHardcore(35309); // "Rewind"
      return;
    }

    const uint64_t frames = std::min(static_cast<uint64_t>(-offsetFrames), m_pastFrameCount);
    if (frames > 0)
    {
      m_gameLoop.SetSpeed(0.0);
      RewindFrames(frames);
      m_gameLoop.SetSpeed(1.0);
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

    // Slow motion is blocked, fast forward is not. Pausing (0.0) is fine.
    if (speedFactor > 0.0 && speedFactor < 1.0)
    {
      CLog::Log(LOGDEBUG, "RetroPlayer[SAVE]: Refusing to slow down in hardcore mode");
      NotifyBlockedByHardcore(35310); // "Slow motion"
      m_gameLoop.SetSpeed(1.0);
      return;
    }
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

std::string CReversiblePlayback::CreateSavestate(bool autosave,
                                                 const std::string& savestatePath /* = "" */)
{
  const size_t memorySize = m_gameClient->GetSerializeSize();

  // Game client must support serialization
  if (memorySize == 0)
    return "";

  //! @todo Handle savestates for standalone game clients
  if (m_gameClient->GetGamePath().empty())
  {
    return "";
  }

  // Take a timestamp of the system clock
  const CDateTime nowUTC = CDateTime::GetUTCDateTime();

  // Record the frame count
  const uint64_t timestampFrames = m_totalFrameCount;

  // Get the savestate path
  std::string savePath(savestatePath);
  {
    std::unique_lock lock(m_savestateMutex);

    if (autosave && savePath.empty())
      savePath = m_autosavePath;

    // Clear autosave path so the next autosave is created in a new slot and
    // does not overwrite the newly-created manual save
    if (!autosave && savePath == m_autosavePath)
      m_autosavePath.clear();

    // If path is still unknown, calculate it now
    if (savePath.empty())
      savePath = CSavestateDatabase::MakeSavestatePath(m_gameClient->GetGamePath(), nowUTC);

    // Update autosave path
    if (autosave)
      m_autosavePath = savePath;
  }

  // Capture the current video frame
  m_renderManager.CacheVideoFrame(savePath);

  {
    std::unique_lock lock(m_savestateMutex);

    // Prune any finished autosave threads
    m_savestateThreads.erase(std::remove_if(m_savestateThreads.begin(), m_savestateThreads.end(),
                                            [](std::future<void>& task) {
                                              return task.wait_for(std::chrono::seconds(0)) ==
                                                     std::future_status::ready;
                                            }),
                             m_savestateThreads.end());

    // Save async to not block game loop
    std::future<void> task =
        std::async(std::launch::async, [this, autosave, savePath, nowUTC, timestampFrames]()
                   { CommitSavestate(autosave, savePath, nowUTC, timestampFrames); });

    m_savestateThreads.emplace_back(std::move(task));
  }

  return savePath;
}

void CReversiblePlayback::CommitSavestate(bool autosave,
                                          const std::string& savePath,
                                          const CDateTime& nowUTC,
                                          uint64_t timestampFrames)
{
  std::unique_ptr<ISavestate> savestate = CSavestateDatabase::AllocateSavestate();
  std::unique_ptr<ISavestate> loadedSavestate;

  const size_t memorySize = m_gameClient->GetSerializeSize();
  uint8_t* const memoryData = savestate->GetMemoryBuffer(memorySize);

  // Separate from the emulator's memory; see savestate.fbs
  std::vector<uint8_t> achievementState;

  // Both payloads under one client lock, so the game loop cannot advance
  // between them and pair one frame's memory with another frame's progress.
  //
  // m_mutex before the client lock, which is the order the game loop uses:
  // AddFrame() holds m_mutex across CGameClient::Serialize(), and that takes
  // the client lock. Taking them the other way round here deadlocks the two
  // against each other whenever an autosave lands mid-frame, which also hangs
  // shutdown because Deinitialize() waits on the save.
  {
    std::unique_lock lock(m_mutex);
    std::unique_lock clientLock = m_gameClient->LockForSnapshot();

    // Copy the savestate memory
    if (m_memoryStream && m_memoryStream->CurrentFrame() != nullptr)
    {
      std::memcpy(memoryData, m_memoryStream->CurrentFrame(), memorySize);
    }
    else
    {
      if (!m_gameClient->Serialize(memoryData, memorySize))
        return;
    }

    m_gameClient->SerializeAchievementState(achievementState);
  }

  if (!achievementState.empty())
  {
    if (uint8_t* const achievementData = savestate->GetAchievementBuffer(achievementState.size()))
      std::memcpy(achievementData, achievementState.data(), achievementState.size());
  }

  // Attempt to get existing properties
  {
    std::unique_lock lock(m_savestateMutex);
    if (!savePath.empty() && XFILE::CFile::Exists(savePath))
    {
      loadedSavestate = CSavestateDatabase::AllocateSavestate();
      if (!m_savestateDatabase->GetSavestate(savePath, *loadedSavestate))
        loadedSavestate.reset();
    }
  }

  const std::string caption =
      CServiceBroker::GetGameServices().AchievementRuntime().GetRichPresence();
  const std::string gameFileName = URIUtils::GetFileName(m_gameClient->GetGamePath());
  const double timestampWallClock =
      (timestampFrames /
       m_gameClient->GetFrameRate()); //! @todo Accumulate playtime instead of deriving it
  const std::string gameClientId = m_gameClient->ID();
  const std::string gameClientVersion = m_gameClient->Version().asString();

  savestate->SetType(autosave ? SAVE_TYPE::AUTO : SAVE_TYPE::MANUAL);
  savestate->SetLabel(loadedSavestate ? loadedSavestate->Label() : "");
  savestate->SetCaption(caption);
  savestate->SetCreated(nowUTC);
  savestate->SetGameFileName(gameFileName);
  savestate->SetTimestampFrames(timestampFrames);
  savestate->SetTimestampWallClock(timestampWallClock);
  savestate->SetGameClientID(gameClientId);
  savestate->SetGameClientVersion(gameClientVersion);

  m_renderManager.SaveVideoFrame(savePath, *savestate);

  savestate->Finalize();

  bool success;
  {
    std::unique_lock lock(m_savestateMutex);
    success = m_savestateDatabase->AddSavestate(savePath, m_gameClient->GetGamePath(), *savestate);
  }

  if (success)
  {
    std::string thumbnailPath = CSavestateDatabase::MakeThumbnailPath(savePath);
    m_renderManager.SaveThumbnail(thumbnailPath);
  }

  // Notify the GUI that the metadata for this savestate should be refreshed
  m_guiMessenger.RefreshSavestates(savePath, savestate.get());
}

bool CReversiblePlayback::LoadSavestate(const std::string& savestatePath)
{
  const size_t memorySize = m_gameClient->GetSerializeSize();

  // Loading a save state is always blocked in hardcore; creating one is not,
  // so that players can still keep a state for later
  if (HardcoreRestrictionsApply())
  {
    CLog::Log(LOGINFO, "RetroPlayer[SAVE]: Refusing to load a savestate in hardcore mode");
    NotifyBlockedByHardcore(35308); // "Loading save states"
    return false;
  }

  const size_t memorySize = m_gameClient->SerializeSize();

  // Game client must support serialization
  if (memorySize == 0)
    return false;

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
      {
        std::unique_lock lock(m_mutex);
        if (m_memoryStream)
        {
          m_memoryStream->SetFrameCounter(savestate->TimestampFrames());
          std::memcpy(m_memoryStream->BeginFrame(), savestate->GetMemoryData(), memorySize);
          m_memoryStream->SubmitFrame();
        }
      }

      if (m_gameClient->Deserialize(savestate->GetMemoryData(), memorySize))
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
        m_totalFrameCount = savestate->TimestampFrames();
        bSuccess = true;
        if (savestate->Type() == SAVE_TYPE::AUTO)
          m_autosavePath = savestatePath;
      }
    }
  }

  return bSuccess;
}

void CReversiblePlayback::FrameEvent()
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

  m_gameClient->RunFrame();
  UpdateFrameRate();

  AddFrame();
}

unsigned int CReversiblePlayback::GetRunaheadFrames() const
{
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
  } streamRestore(m_streamManager);

  // The frame that is really happening. Its picture and sound are thrown away
  // -- the player is shown a later one instead -- but its input is the input
  // the whole sequence is predicting from, so this is the only frame that polls.
  m_streamManager.EnableAudio(false);
  m_streamManager.EnableVideo(false);

  m_gameClient->RunFrame();
  UpdateFrameRate();

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

  size_t achievementSize = 0;
  bool achievementsSaved = false;
  if (bProtectAchievements)
  {
    achievementSize = m_gameClient->GetAchievementStateSize();
    if (achievementSize > 0)
    {
      m_runaheadAchievementState.resize(achievementSize);
      achievementsSaved =
          m_gameClient->SerializeAchievements(m_runaheadAchievementState.data(), achievementSize);
    }
  }

  // Look into the future. These frames deliberately do not poll: they have to
  // answer to the same input as the frame that committed, or the picture the
  // player is shown predicts a button they never pressed.
  for (unsigned int frame = 1; frame <= frames; ++frame)
  {
    const bool bLastFrame = (frame == frames);

    // Only the furthest frame is seen and heard, so exactly one frame's worth
    // of sound is produced per frame of real time and the audio rate is
    // unchanged
    m_streamManager.EnableAudio(bLastFrame);
    m_streamManager.EnableVideo(bLastFrame);

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
    m_gameClient->DeserializeAchievements(m_runaheadAchievementState.data(), achievementSize);

  // The client is back at the state just serialized, so hand it to the rewind
  // buffer rather than asking for it a second time
  AddFrame(m_runaheadState);

  return true;
}

void CReversiblePlayback::RewindEvent()
{
  RewindFrames(1);

  m_gameClient->RunFrame();
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
  std::unique_lock lock(m_mutex);

  if (m_memoryStream)
  {
    const size_t frameSize = m_memoryStream->FrameSize();

    // Run-ahead has already taken a state from the client this frame. Copying
    // it is what keeps rewind and run-ahead together down to one serialize per
    // frame rather than two.
    if (serialized.size() == frameSize)
    {
      std::memcpy(m_memoryStream->BeginFrame(), serialized.data(), frameSize);
      m_memoryStream->SubmitFrame();
      UpdatePlaybackStats();
    }
    else if (m_gameClient->Serialize(m_memoryStream->BeginFrame(), frameSize))
    {
      m_memoryStream->SubmitFrame();
      UpdatePlaybackStats();
    }
  }

  m_totalFrameCount++;
}

void CReversiblePlayback::UpdateFrameRate()
{
  const double previousFrameRate = m_gameLoop.FPS();
  m_gameLoop.SetFrameRate(m_gameClient->GetFrameRate());

  if (m_gameLoop.FPS() != previousFrameRate)
    UpdateMemoryStream();
}

void CReversiblePlayback::RewindFrames(uint64_t frames)
{
  std::unique_lock lock(m_mutex);

  if (m_memoryStream)
  {
    m_memoryStream->RewindFrames(frames);
    m_gameClient->Deserialize(m_memoryStream->CurrentFrame(), m_memoryStream->FrameSize());
    UpdatePlaybackStats();
  }

  m_totalFrameCount -= std::min(m_totalFrameCount, frames);
}

void CReversiblePlayback::AdvanceFrames(uint64_t frames)
{
  std::unique_lock lock(m_mutex);

  if (m_memoryStream)
  {
    m_memoryStream->AdvanceFrames(frames);
    m_gameClient->Deserialize(m_memoryStream->CurrentFrame(), m_memoryStream->FrameSize());
    UpdatePlaybackStats();
  }

  m_totalFrameCount += frames;
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

void CReversiblePlayback::UpdateMemoryStream()
{
  std::unique_lock lock(m_mutex);

  bool bRewindEnabled = false;

  GAME::CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  if (m_gameClient->GetSerializeSize() > 0)

  // Hardcore forbids rewind, so the buffer isn't just unused - it shouldn't be
  // allocated at all. It costs the savestate size for every frame of the
  // rewind window, which runs to gigabytes on consoles with large states.
  if (m_gameClient->SerializeSize() > 0 && !HardcoreRestrictionsApply())
    bRewindEnabled = gameSettings.RewindEnabled();

  if (bRewindEnabled)
  {
    unsigned int rewindBufferSec = gameSettings.MaxRewindTimeSec();
    if (rewindBufferSec < 10)
      rewindBufferSec = 10; // Sanity check

    unsigned int frameCount = MathUtils::round_int(rewindBufferSec * m_gameLoop.FPS());

    if (!m_memoryStream)
    {
      const size_t memorySize = m_gameClient->GetSerializeSize();

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
    m_memoryStream.reset();

    // Reset playback stats
    m_pastFrameCount = 0;
    m_futureFrameCount = 0;
    m_playTimeMs = 0;
    m_totalTimeMs = 0;
    m_cacheTimeMs = 0;
  }
}
