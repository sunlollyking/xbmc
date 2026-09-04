# Kodi / RetroPlayer — RetroAchievements hardcore evaluation build

Kodi 22.0 BETA2, Linux x86_64 (X11, OpenGL).

RetroAchievements support lives in the **game add-on**, not in Kodi. The add-on
(`game.libretro`) owns `rc_client`, identifies the game and talks to the
RetroAchievements server; Kodi receives events and enforces the restrictions
hardcore requires. Client name reported to the server: `KodiRetroPlayer`.

## What is in this build

* Hardcore mode
* Encore mode
* On-screen challenge and progress indicators
* Leaderboards, with standings

## How hardcore is enforced

Hardcore is a Kodi setting (Settings → Games → Achievements → Hardcore mode).
Turning it on tells `rc_client`, which raises a reset, and Kodi restarts the
game — a session begun in casual mode cannot continue into hardcore.

| Restriction | Where |
| --- | --- |
| Loading save states refused | `CReversiblePlayback::LoadSavestate()` |
| Rewind refused | `CReversiblePlayback::SetSpeed()`, negative speed |
| Seeking backwards refused | `CReversiblePlayback::SeekTimeMs()` |
| Slow motion refused | `CReversiblePlayback::SetSpeed()`, 0 < speed < 1 |
| Rewind buffer never allocated | `CReversiblePlayback::UpdateMemoryStream()` |
| Cheats refused | `CGameLibRetro::SetCheat()` / `CheatReset()`, in the add-on |
| Resuming a save state drops to casual | `CRetroPlayer::OpenFile()`, before the load |

Every route that loads a save state — the in-game dialog, JSON-RPC, the Python
player API, resume-on-open — funnels through `LoadSavestate()`, so the refusal
is at one chokepoint rather than per caller. Seeking backwards is guarded in its
own right because it reaches the rewind path without passing through
`SetSpeed()`.

Cheats are refused in the add-on rather than in Kodi. Kodi has no cheat
interface at all, so there is nothing there to guard; the add-on is the last
point before the emulator core, which means a cheat interface added to Kodi
later cannot breach the terms by omission.

Each refusal shows the player a notification saying which feature was blocked
and why, rather than silently ignoring the request.

## Deliberately still allowed

Saving states, pausing, and fast-forward. Achievement progress is captured with
the emulator state under a single lock, so a save cannot pair one frame's memory
with another frame's progress.

## Leaderboards

`rc_client` activates leaderboards only when hardcore is on, and this build does
not set `allow_leaderboards_in_softcore`. Outside hardcore the leaderboards
window says attempts are read-only; in hardcore that notice is gone and attempts
count.

## Running it

    tar -xf kodi-retroachievements-<version>-linux-x86_64.tar.xz
    cd kodi-retroachievements-<version>
    ./run-kodi.sh

The script points Kodi at a self-contained profile in the extracted folder, so
it will not touch an existing `~/.kodi`.

Sign in at Settings → Games → Achievements, then start a game. The add-on
identifies it and signs in as part of loading.

## Verifying the restrictions

With hardcore on and a game running:

* Rewind or slow motion from the player controls — refused, with a notification
* Game OSD → Save / Load → pick an existing state — refused
* Game OSD → Save / Load → Save — still allowed, as intended
* Fast-forward — still allowed
* Turn hardcore on while a game is running — the game resets

Kodi's log records each refusal, for example:

    RetroPlayer[SAVE]: Refusing to load a savestate in hardcore mode
    RetroPlayer[SAVE]: Refusing to seek backwards in hardcore mode

## Notes

Run-ahead is not in this build. Where it is present in other branches, achievement
processing is skipped on speculative frames so that a trigger cannot fire on a
frame the player never sees.
