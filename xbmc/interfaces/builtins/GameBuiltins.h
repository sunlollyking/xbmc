/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "Builtins.h"

/*!
 * \brief Built-in commands for games
 *
 * Kept apart from the player built-ins because these act on the game being
 * played rather than on playback itself.
 */
class CGameBuiltins
{
public:
  /*! \brief Get the list of game related builtin functions
   *  \return The list of built-in functions
   */
  static CBuiltins::CommandMap GetOperations();
};
