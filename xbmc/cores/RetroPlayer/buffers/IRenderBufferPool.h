/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/RetroPlayer/RetroPlayerTypes.h"

extern "C"
{
#include <libavutil/pixfmt.h>
}

#include <memory>
#include <stdint.h>

namespace KODI
{
namespace RETRO
{
class CRenderBufferManager;
class CRenderVideoSettings;
class CRPBaseRenderer;
class IRenderBuffer;

/*!
 * \brief The rendering context a game client asked for
 *
 * A rendering-system-agnostic description of the client's request, so the
 * buffer layer doesn't have to reach into the Game API for it.
 */
struct HwContextProperties
{
  //! \brief Request a core profile rather than a compatibility profile
  bool coreProfile{true};

  //! \brief Request an OpenGL ES context rather than desktop OpenGL
  bool embedded{false};

  //! \brief Minimum context version, or 0 to let the driver decide
  unsigned int versionMajor{0};
  unsigned int versionMinor{0};

  //! \brief The core needs a depth attachment on the framebuffer
  bool depth{false};

  //! \brief The core needs a stencil attachment on the framebuffer
  bool stencil{false};
};

class IRenderBufferPool : public std::enable_shared_from_this<IRenderBufferPool>
{
public:
  virtual ~IRenderBufferPool() = default;

  virtual void RegisterRenderer(CRPBaseRenderer* renderer) = 0;
  virtual void UnregisterRenderer(CRPBaseRenderer* renderer) = 0;
  virtual bool HasVisibleRenderer() const = 0;

  virtual bool Configure(AVPixelFormat format) = 0;

  virtual bool IsConfigured() const = 0;

  virtual bool IsCompatible(const CRenderVideoSettings& renderSettings) const = 0;

  /*!
   * \brief Get a free buffer from the pool, sets ref count to 1
   *
   * \param width The horizontal pixel count of the buffer
   * \param height The vertical pixel could of the buffer
   *
   * \return The allocated buffer, or nullptr on failure
   */
  virtual IRenderBuffer* GetBuffer(unsigned int width, unsigned int height) = 0;

  /*!
   * \brief Called by buffer when ref count goes to zero
   *
   * \param buffer A fully dereferenced buffer
   */
  virtual void Return(IRenderBuffer* buffer) = 0;

  virtual void Prime(unsigned int width, unsigned int height) = 0;

  virtual void Flush() = 0;

  virtual DataAccess GetMemoryAccess() const { return DataAccess::READ_WRITE; }
  virtual DataAlignment GetMemoryAlignment() const { return DataAlignment::DATA_UNALIGNED; }

  /*!
   * \brief Call in GetBuffer() before returning buffer to caller
   */
  virtual std::shared_ptr<IRenderBufferPool> GetPtr() { return shared_from_this(); }

  /*!
   * \brief Create the resources tied to the rendering context
   *
   * Called on the thread that will render into the pool's buffers, before the
   * client is told the context is ready. Pools that own a GL context must
   * create it and make it current here, as a context can only be current on
   * one thread. Pools with no context of their own need do nothing.
   *
   * \param properties The context the client asked for
   *
   * \return True if the context is ready, or the pool has none to create
   */
  virtual bool CreateContext(const HwContextProperties& properties) { return true; }

  /*!
   * \brief Release resources tied to the rendering context
   *
   * This function is called when the render context is being destroyed.
   * Implementations should free any context-specific resources so that the
   * pool can be safely recreated.
   *
   * Must run on the same thread that called CreateContext().
   */
  virtual void DestroyContext() {}
};
} // namespace RETRO
} // namespace KODI
