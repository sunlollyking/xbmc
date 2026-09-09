# Kodi / RetroPlayer — RetroAchievements hardcore evaluation build

Kodi 22.0, Linux x86_64 (X11, OpenGL).

RetroAchievements support lives in the **game add-on**, not in Kodi. The add-on
(`game.libretro`) owns `rc_client`, identifies the game and talks to the
RetroAchievements server; Kodi receives events and enforces the restrictions
hardcore requires. Client name reported to the server: `KodiRetroPlayer`.

## What is in this build

* **Hardcore mode** -- the setting, everything it withholds, and a toggle in the
  achievements window that asks before restarting the game
* **Encore mode** -- earned achievements armed again for a replay
* **On-screen indicators** -- the achievement currently being attempted, and how
  far along a measured one is
* **Leaderboards** -- the game's boards, each with the top ten and the player's
  own place
* **The achievements window** -- the full set for the game being played, earned
  first, with points, rarity and unlock dates
* **Rich presence**, and notifications for unlocks, leaderboard attempts,
  mastery and subset completion

Everything except hardcore mode is already in Kodi's master branch, and the
add-on half of it is in game.libretro's. Hardcore is the only part held back,
and the only thing this build adds on top: one commit to each project.

## The emulator is unmodified

Worth saying plainly, because it is the part RetroAchievements already knows.

`game.libretro` is a wrapper. It translates between Kodi's game add-on API and
the libretro API, and owns `rc_client`; it does not touch the emulator's code.
Each emulator is a separate add-on that fetches a libretro release verbatim and
builds it as-is. For the FCEUmm shipped here, the whole of that definition is
one line:

    fceumm https://github.com/libretro/libretro-fceumm/archive/3a84a6fd0ba20dd4877c06b1d58741172148395f.tar.gz

No patches are applied to it. The resulting `fceumm_libretro.so` is renamed
into the add-on and loaded unchanged, so the emulation, its memory map and its
save state format are the same ones any other libretro frontend would give you.
Nothing in this build changes how a game runs.

## How this build identifies itself

RetroAchievements gates hardcore on the client it is told about, so the
User-Agent names the integration, the host, the emulator and rcheevos, in that
order:

    KodiRetroPlayer/22.7.0 (Kodi/22.0 (X11; Linux x86_64) ...) FCEUmm/(SVN) rcheevos/12.3
    KodiRetroPlayer/22.7.0 (Kodi/22.0 (X11; Linux x86_64) ...) Nestopia/1.53.2 rcheevos/12.3

`KodiRetroPlayer` is the client name to allow, and the version after it is the
`game.libretro` add-on's, so it means the same thing whichever emulator is
loaded. The emulator names itself from `retro_get_system_info()`, the same
answer any libretro frontend would report, so an unlock can be attributed to
the core that earned it.

Nothing else is needed on this side: hardcore is already wired end to end, and
approving `KodiRetroPlayer` is enough to make it work.

## What happens today, and what we are asking for

A hardcore session on Super Mario Bros., signed in, with this build. Unlocks
and leaderboard attempts reach the server and it answers -- the placement below
came back from it, and nothing was refused:

    rc_client: Awarding achievement 3152: If I Were a Rich Man
    CGameClientCheevos: earned achievement 3152 "If I Were a Rich Man" (1 points) in hardcore mode
    rc_client: Submitting 327.36 (32736) for leaderboard 171647: World 1-1 Speedrun
    CGameClientCheevos: leaderboard 171647 "World 1-1 Speedrun" submitted 327.36

What the server also does is this, as the game loads:

    CCheevos: skipping system notice 'Warning: Unknown Emulator'
    rc_client: Awarding achievement 101000001: Warning: Unknown Emulator
    CGameClientCheevos: achievement 101000001 "Warning: Unknown Emulator" was already earned

That is your own warning, saying the client is not one you recognise. The
add-on keeps it out of the player's notifications rather than showing it, but
it is exactly the state we are asking to leave.

**The ask is one thing: allow `KodiRetroPlayer` for hardcore.** The restrictions
below are already enforced, the mode already reaches `rc_client` before a game
is identified, and unlocks already carry the hardcore flag. Nothing else is
waiting on this side.

## How hardcore is enforced

Hardcore is a Kodi setting, reachable from Settings → Games → Achievements and
from the achievements window during a game. Turning it on tells `rc_client`,
which raises a reset, and Kodi restarts the game — a session begun in casual
mode cannot continue into hardcore.

| Restriction | Where |
| --- | --- |
| Loading save states refused | `CReversiblePlayback::LoadSavestate()` |
| Rewind refused | `CReversiblePlayback::SetSpeed()`, negative speed |
| Seeking backwards refused | `CReversiblePlayback::SeekTimeMs()` |
| Slow motion refused | `CReversiblePlayback::SetSpeed()`, 0 < speed < 1 (see below) |
| Rewind buffer never allocated | `CReversiblePlayback::UpdateMemoryStream()` |
| Rewind settings greyed out | `system/settings/settings.xml`, dependency |
| Cheats refused | `CGameLibRetro::SetCheat()` / `CheatReset()`, in the add-on |
| Resuming a save state drops to casual | `CRetroPlayer::CreatePlayback()`, before the load |

Every route that loads a save state — the in-game dialog, JSON-RPC, the Python
player API, resume-on-open — funnels through `LoadSavestate()`, so the refusal
is at one chokepoint rather than per caller. Seeking backwards is guarded in its
own right because it reaches the rewind path without passing through
`SetSpeed()`.

Cheats are refused in the add-on rather than in Kodi. Kodi has no cheat
interface at all, so there is nothing there to guard; the add-on is the last
point before the emulator core, which means a cheat interface added to Kodi
later cannot breach the terms by omission.

Slow motion is the same case, and the row above should be read that way. Kodi
has no slow-motion control for games: every route that sets a play speed --
the fast forward and rewind actions, the `PlayerControl` builtin and the
JSON-RPC `Player.SetSpeed` method -- offers the same ladder of whole multiples,
1, 2, 4, 8, 16 and 32 either way round, and `Player.SetTempo` does not reach
RetroPlayer at all. Nothing asks for a speed between 0 and 1 today. The refusal
is in place so that a control which does cannot arrive without it.

Each refusal shows the player a notification saying which feature was withheld
and why, rather than silently ignoring the request. Those notifications carry
the player's RetroAchievements avatar, as the sign-in one does, so it is clear
who is asking.

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

* Rewind from the player controls — refused, with a notification
* Game OSD → Save / Load → pick an existing state — refused
* Game OSD → Save / Load → Save — still allowed, as intended
* Fast-forward — still allowed
* Turn hardcore on from the achievements window — asks first, then resets the game
* Settings → Games → Enable rewind — greyed out while hardcore is on

Kodi's log records each refusal, for example:

    RetroPlayer[SAVE]: Refusing to load a savestate in hardcore mode
    RetroPlayer[SAVE]: Refusing to seek backwards in hardcore mode

## Source code

Both halves are public.

* Kodi: <https://github.com/sunlollyking/xbmc/tree/ra-hardcore-build>
* Add-on: <https://github.com/sunlollyking/game.libretro/tree/ra-hardcore-addon-v2>

Each is its project's own development branch plus a single commit adding
hardcore mode, so the difference this build introduces can be read on its own:

* <https://github.com/xbmc/xbmc> — the achievements window, encore mode,
  indicators and leaderboards
* <https://github.com/kodi-game/game.libretro> — the add-on that owns
  `rc_client` and talks to RetroAchievements

## Notes

Run-ahead is not in this build. Where it is present in other branches,
achievement processing is skipped on speculative frames so that a trigger cannot
fire on a frame the player never sees.
