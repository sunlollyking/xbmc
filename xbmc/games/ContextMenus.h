/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ContextMenuItem.h"

#include <memory>

namespace CONTEXTMENU
{

struct CGamePlay : CStaticContextMenuAction
{
  CGamePlay() : CStaticContextMenuAction(208) {} // "Play"
  bool IsVisible(const CFileItem& item) const override;
  bool Execute(const std::shared_ptr<CFileItem>& item) const override;
};

struct CGameMarkBeaten : CStaticContextMenuAction
{
  CGameMarkBeaten() : CStaticContextMenuAction(35770) {} // "Mark as beaten"
  bool IsVisible(const CFileItem& item) const override;
  bool Execute(const std::shared_ptr<CFileItem>& item) const override;
};

struct CGameMarkNotBeaten : CStaticContextMenuAction
{
  CGameMarkNotBeaten() : CStaticContextMenuAction(35771) {} // "Mark as not beaten"
  bool IsVisible(const CFileItem& item) const override;
  bool Execute(const std::shared_ptr<CFileItem>& item) const override;
};

struct CGameMarkCompleted : CStaticContextMenuAction
{
  CGameMarkCompleted() : CStaticContextMenuAction(35669) {} // "Mark as completed"
  bool IsVisible(const CFileItem& item) const override;
  bool Execute(const std::shared_ptr<CFileItem>& item) const override;
};

struct CGameMarkNotCompleted : CStaticContextMenuAction
{
  CGameMarkNotCompleted() : CStaticContextMenuAction(35670) {} // "Mark as not completed"
  bool IsVisible(const CFileItem& item) const override;
  bool Execute(const std::shared_ptr<CFileItem>& item) const override;
};

} // namespace CONTEXTMENU
