/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameBuiltins.h"

#include "FileItem.h"
#include "ServiceBroker.h"
#include "application/Application.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "games/GameManual.h"
#include "games/tags/GameInfoTag.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <string>
#include <vector>

namespace
{

/*!
 * \brief A name for the game that the player will recognise
 *
 * The filename is preferred over the title from the info tag, because the
 * filename is what has to match for a manual to be found. Telling someone no
 * manual was found for "Sonic The Hedgehog" when the file is called
 * "Sonic The Hedgehog (USA)" would send them looking in the wrong place.
 */
std::string GetGameName(const CFileItem& item)
{
  const std::string path = item.GetDynPath();
  if (!path.empty())
  {
    std::string name = URIUtils::GetFileName(path);
    URIUtils::RemoveExtension(name);
    if (!name.empty())
      return name;
  }

  if (item.HasGameInfoTag())
    return item.GetGameInfoTag()->GetTitle();

  return item.GetLabel();
}

/*! \brief Show the manual for the game being played.
 *  \param params (ignored)
 */
int ShowGameManual(const std::vector<std::string>& params)
{
  const CFileItem& item = g_application.CurrentFileItem();
  const std::string gameName = GetGameName(item);

  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  const std::string manualPath = KODI::GAME::CGameManual::GetManualPath(item);
  if (manualPath.empty())
  {
    CLog::Log(LOGDEBUG, "ShowGameManual: no manual beside \"{}\"", item.GetDynPath());

    // "Manual", "No manual found for {0:s}"
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, strings.Get(35311),
                                          StringUtils::Format(strings.Get(35312), gameName));
    return 0;
  }

  CLog::Log(LOGINFO, "ShowGameManual: found manual \"{}\"", manualPath);

  //! @todo Open the manual in the viewer once it exists. Until then the lookup
  //! is reported, so that a library can be checked without a renderer.
  CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, strings.Get(35311),
                                        URIUtils::GetFileName(manualPath));

  return 0;
}

} // namespace

CBuiltins::CommandMap CGameBuiltins::GetOperations()
{
  return {
      {"showgamemanual", {"Show the manual for the game being played", 0, ShowGameManual}},
  };
}
