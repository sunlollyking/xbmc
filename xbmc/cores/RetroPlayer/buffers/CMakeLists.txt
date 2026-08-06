set(SOURCES BaseRenderBuffer.cpp
            BaseRenderBufferPool.cpp
            RenderBufferManager.cpp
)

set(HEADERS BaseRenderBuffer.h
            BaseRenderBufferPool.h
            IRenderBuffer.h
            IRenderBufferPool.h
            RenderBufferManager.h
)

if(TARGET ${APP_NAME_LC}::OpenGl)
  list(APPEND SOURCES RenderBufferOpenGL.cpp
                      RenderBufferPoolOpenGL.cpp)
  list(APPEND HEADERS RenderBufferOpenGL.h
                      RenderBufferPoolOpenGL.h)
endif()

if(TARGET ${APP_NAME_LC}::OpenGLES)
  list(APPEND SOURCES RenderBufferOpenGLES.cpp
                      RenderBufferPoolOpenGLES.cpp)
  list(APPEND HEADERS RenderBufferOpenGLES.h
                      RenderBufferPoolOpenGLES.h)
endif()

if(("gbm" IN_LIST CORE_PLATFORM_NAME_LC OR "wayland" IN_LIST CORE_PLATFORM_NAME_LC) AND TARGET ${APP_NAME_LC}::EGL)
  list(APPEND SOURCES RenderBufferDMA.cpp
                      RenderBufferPoolDMA.cpp)
  list(APPEND HEADERS RenderBufferDMA.h
                      RenderBufferPoolDMA.h)

  # The FBO pool creates a shared EGL context off the windowing system, so it
  # requires EGL in addition to desktop GL. GLES is out of scope for now.
  if(TARGET ${APP_NAME_LC}::OpenGl)
    list(APPEND SOURCES RenderBufferFBO.cpp
                        RenderBufferPoolFBO.cpp)
    list(APPEND HEADERS RenderBufferFBO.h
                        RenderBufferPoolFBO.h)
  endif()
endif()

core_add_library(rp-buffers)
