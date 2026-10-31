/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CFileItemList;

namespace KODI
{
namespace RETRO
{
class CGUIGameVideoHandle;
}

namespace GAME
{

/*!
 * \ingroup games
 *
 * \brief The video filters a game can be drawn with
 *
 * Built here rather than inside the OSD dialog because the same list is needed
 * from the library, where no game is running and there is nothing to ask which
 * scaling methods it supports.
 *
 * Each item carries the filter in its "game.videofilter" property, which is
 * what gets stored and what the renderer is given.
 *
 * \param items Filled with one item per filter
 * \param videoHandle The running game, so that only the scaling methods it
 *        supports are offered. Without one, the basic scaling methods are
 *        offered anyway: they are the fallbacks every renderer has, and a
 *        filter that turns out not to work draws the game unfiltered.
 */
void GetVideoFilters(CFileItemList& items, RETRO::CGUIGameVideoHandle* videoHandle = nullptr);

} // namespace GAME
} // namespace KODI
