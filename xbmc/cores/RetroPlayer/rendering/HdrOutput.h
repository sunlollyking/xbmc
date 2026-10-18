/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

struct VideoPicture;

namespace KODI::RETRO
{
/*!
 * \brief Describe the HDR10 output a game is shown in, for the windowing
 *        system's HDR calls
 *
 * \param picture Filled with BT.2020 colours, the PQ transfer at 10 bits and
 *        static metadata for the brightest light the game is shown with
 * \param paperWhite Luminance in nits of the game's ordinary colours
 * \param peak Luminance in nits of its brightest light
 */
void GetHdrOutputPicture(VideoPicture& picture, unsigned int paperWhite, unsigned int peak);
} // namespace KODI::RETRO
