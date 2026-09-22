/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/GameThumbLoader.h"
#include "windows/GUIMediaWindow.h"

class CGUIDialogProgress;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 */
class CGUIWindowGames : public CGUIMediaWindow
{
public:
  CGUIWindowGames();
  ~CGUIWindowGames() override = default;

  // implementation of CGUIControl via CGUIMediaWindow
  bool OnMessage(CGUIMessage& message) override;

protected:
  // implementation of CGUIMediaWindow
  void SetupShares() override;
  bool OnClick(int iItem, const std::string& player = "") override;
  void GetContextButtons(int itemNumber, CContextButtons& buttons) override;
  bool OnContextButton(int itemNumber, CONTEXT_BUTTON button) override;
  bool OnAddMediaSource() override;
  bool GetDirectory(const std::string& strDirectory, CFileItemList& items) override;
  bool Update(const std::string& strDirectory, bool updateFilterPath = true) override;
  std::string GetStartFolder(const std::string& dir) override;

  bool OnClickMsg(int controlId, int actionId);
  void OnItemInfo(int itemNumber);
  //! \brief What a click on a game should do, from the games library settings
  static int SelectAction();

  /*!
   * \brief Ask which of them the person wants
   *
   * \return True when the choice has been carried out, false to play the game
   */
  static bool ChooseAction(const std::shared_ptr<CFileItem>& item);

  bool PlayGame(const CFileItem& item);
  bool CanPlay(const CFileItem& item) const;

  CGUIDialogProgress* m_dlgProgress = nullptr;

  CGameThumbLoader m_thumbLoader;
};
} // namespace GAME
} // namespace KODI
