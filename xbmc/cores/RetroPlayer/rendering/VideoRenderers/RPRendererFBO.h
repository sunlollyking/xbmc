/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "RPBaseRenderer.h"
#include "cores/GameSettings.h"
#include "cores/RetroPlayer/buffers/BaseRenderBufferPool.h"
#include "cores/RetroPlayer/buffers/video/RenderBufferSysMem.h"
#include "cores/RetroPlayer/process/RPProcessInfo.h"

#include <atomic>
#include <chrono>
#include <stdint.h>
#include <vector>

#include "system_gl.h"

namespace KODI
{
namespace RETRO
{
class CRendererFactoryFBO : public IRendererFactory
{
public:
  ~CRendererFactoryFBO() override = default;

  // implementation of IRendererFactory
  std::string RenderSystemName() const override;
  CRPBaseRenderer* CreateRenderer(const CRenderSettings& settings,
                                  CRenderContext& context,
                                  std::shared_ptr<IRenderBufferPool> bufferPool) override;
  RenderBufferPoolVector CreateBufferPools(CRenderContext& context) override;
};

class CRPRendererFBO : public CRPBaseRenderer
{
public:
  CRPRendererFBO(const CRenderSettings& renderSettings,
                 CRenderContext& context,
                 std::shared_ptr<IRenderBufferPool> bufferPool);
  ~CRPRendererFBO() override;

  // implementation of CRPBaseRenderer
  bool Supports(RENDERFEATURE feature) const override;
  SCALINGMETHOD GetDefaultScalingMethod() const override { return SCALINGMETHOD::NEAREST; }

  static bool SupportsScalingMethod(SCALINGMETHOD method);

protected:
  // implementation of CRPBaseRenderer
  void RenderInternal(bool clear, uint8_t alpha) override;
  void FlushInternal() override;

  /*!
   * \brief Set the entire backbuffer to black
   */
  void ClearBackBuffer();

  /*!
   * \brief Draw black bars around the video quad
   *
   * This is more efficient than glClear() since it only sets pixels to
   * black that aren't going to be overwritten by the game.
   */
  void DrawBlackBars();

  virtual void Render(uint8_t alpha);

  GLenum m_textureTarget = GL_TEXTURE_2D;
  float m_clearColour = 0.0f;

  /*!
   * \brief The geometry a frame was drawn with, as far as logging cares
   *
   * Compared exactly rather than with a tolerance: every field is copied or
   * derived the same way each frame, so any difference at all is a real change
   * and worth seeing.
   */
  struct FrameGeometry
  {
    unsigned int frameWidth{0};
    unsigned int frameHeight{0};
    unsigned int textureWidth{0};
    unsigned int textureHeight{0};
    CRect sourceRect;
    CRect samplingRect;
    bool bottomLeftOrigin{false};

    bool operator==(const FrameGeometry& rhs) const
    {
      return frameWidth == rhs.frameWidth && frameHeight == rhs.frameHeight &&
             textureWidth == rhs.textureWidth && textureHeight == rhs.textureHeight &&
             sourceRect == rhs.sourceRect && samplingRect == rhs.samplingRect &&
             bottomLeftOrigin == rhs.bottomLeftOrigin;
    }
    bool operator!=(const FrameGeometry& rhs) const { return !(*this == rhs); }
  };

  //! \brief The geometry reported by the last line written to the log
  FrameGeometry m_loggedGeometry;

  //! \brief Set once anything has been logged, so the first frame always is
  bool m_bLoggedGeometry = false;

  //! \brief Changes seen since the last line was written
  unsigned int m_geometryChanges = 0;

  //! \brief When the last line was written, to keep a churning geometry quiet
  std::chrono::steady_clock::time_point m_lastGeometryLog;
};
} // namespace RETRO
} // namespace KODI
