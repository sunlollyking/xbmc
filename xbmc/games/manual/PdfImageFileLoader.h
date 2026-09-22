/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "imagefiles/SpecialImageFileLoader.h"

namespace KODI
{
namespace GAME
{

/*!
 * \brief Renders a page of a PDF as an image
 *
 * Loads `image://pdf@<document>?page=<n>&height=<pixels>`.
 *
 * Going through the image pipeline rather than rendering into a window means a
 * page is fetched on the background loader, cached, and displayed by an
 * ordinary skin image control. Paging through a manual then costs a skin
 * author nothing, and works the same on every renderer Kodi supports.
 *
 * `page` counts from zero and defaults to the first page. `height` is the
 * height to render at in pixels, and defaults to something reasonable for a
 * full screen view; the page keeps its aspect ratio either way.
 */
class CPdfImageFileLoader : public IMAGE_FILES::ISpecialImageFileLoader
{
public:
  CPdfImageFileLoader() = default;
  ~CPdfImageFileLoader() override = default;

  // Implementation of ISpecialImageFileLoader
  bool CanLoad(const std::string& specialType) const override;
  std::unique_ptr<CTexture> Load(const IMAGE_FILES::CImageFileURL& imageFile) const override;
};

} // namespace GAME
} // namespace KODI
