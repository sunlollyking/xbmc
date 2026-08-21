/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/GUIWindow.h"

#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \brief Shows the manual that came with a game
 *
 * The window holds only the position in the document. The page itself is put
 * on screen by an ordinary image control in the skin, pointed at the image
 * URL published as a window property, so Kodi's background loader fetches and
 * caches pages the same way it does any other image.
 *
 * Opened with the path to the document as the first window parameter.
 */
class CGUIWindowGameManual : public CGUIWindow
{
public:
  CGUIWindowGameManual();
  ~CGUIWindowGameManual() override;

  // Implementation of CGUIControl via CGUIWindow
  bool OnAction(const CAction& action) override;
  bool OnMessage(CGUIMessage& message) override;

protected:
  // Implementation of CGUIWindow
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;

private:
  /*!
   * \brief Move by a number of pages, stopping at either end
   *
   * Paging past the end is a no-op rather than a wrap, because a manual is
   * read in order and wrapping from the back cover to the front reads as a
   * glitch.
   *
   * \return True if the page changed
   */
  bool MovePage(int distance);

  /*!
   * \brief Publish the current position for the skin to display
   */
  void UpdateProperties();

  std::string m_manualPath;
  unsigned int m_page{0};
  unsigned int m_pageCount{0};
};

} // namespace GAME
} // namespace KODI
