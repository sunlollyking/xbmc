/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/GUIDialog.h"

#include <memory>

class CFileItem;

namespace KODI
{
namespace GAME
{
class CGameInfoTag;

/*!
 * \ingroup games
 *
 * \brief Everything the library knows about one game, with what can be done
 *        to it
 *
 * The games counterpart of the video information dialog. The skin reads the
 * game through ListItem.*, and the buttons play it, refresh it, open its
 * releases, mark it a favourite, move it between not started, in progress and
 * completed, rate it, and choose the emulator and video filter it plays with.
 */
class CGUIDialogGameInfo : public CGUIDialog
{
public:
  CGUIDialogGameInfo();
  ~CGUIDialogGameInfo() override;

  // Implementation of CGUIWindow
  bool OnMessage(CGUIMessage& message) override;
  bool HasListItems() const override { return true; }
  std::shared_ptr<CFileItem> GetCurrentListItem(int offset = 0) override { return m_item; }

  /*!
   * \brief Show the dialog for a game
   *
   * \param item A library game, or a file with a game tag
   *
   * \return True if the user asked for the game to play; the caller plays it
   */
  static bool ShowFor(const std::shared_ptr<CFileItem>& item);

  /*!
   * \brief Whether the user asked for the game to play when the dialog closed
   */
  bool WantsToPlay() const { return m_play; }

protected:
  void OnInitWindow() override;

private:
  void SetItem(const std::shared_ptr<CFileItem>& item);
  void Reload();
  void OnRefresh();
  void OnReleases();
  void OnFavourite();
  //! \brief The three states a game is in, as a video is watched or not
  enum class PlayStateValue
  {
    NOT_STARTED = 0,
    IN_PROGRESS = 1,
    COMPLETED = 2,
  };

  static PlayStateValue PlayState(const CGameInfoTag& tag);
  void OnPlayState();

  //! \brief Every picture the library holds for this game, one to look at
  void OnArtwork();
  void OnUserRating();
  void OnGameClient();
  void OnVideoFilter();
  void UpdateButtons();

  std::shared_ptr<CFileItem> m_item;
  bool m_play{false};
};
} // namespace GAME
} // namespace KODI
