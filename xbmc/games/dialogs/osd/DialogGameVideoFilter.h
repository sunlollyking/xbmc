/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DialogGameVideoSelect.h"
#include "FileItem.h"
#include "FileItemList.h"

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 */
class CDialogGameVideoFilter : public CDialogGameVideoSelect
{
public:
  CDialogGameVideoFilter();
  ~CDialogGameVideoFilter() override = default;

protected:
  // implementation of CGUIWindow via CDialogGameVideoSelect
  void OnInitWindow() override;

  // implementation of CDialogGameVideoSelect
  std::string GetHeading() override;
  void PreInit() override;
  void GetItems(CFileItemList& items) override;
  void OnItemFocus(unsigned int index) override;
  unsigned int GetFocusedItem() const override;
  void PostExit() override;
  bool OnClickAction() override;
  void RefreshList() override;
  void SaveSettings() override;

private:
  void InitGetMoreButton();
  void OnGetMore();
  void OnGetMoreComplete();

  CFileItemList m_items;

  static std::string GetLocalizedString(uint32_t code);

  struct VideoFilterProperties
  {
    std::string path;
    std::string name;
    std::string folder;
  };

  // GUI state
  std::string m_initialVideoFilter;
  unsigned int m_focusedItemIndex{0};
  bool m_regenerateList{false};
};
} // namespace GAME
} // namespace KODI
