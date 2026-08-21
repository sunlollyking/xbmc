# FindPoppler
# -----------
# Finds the Poppler PDF rendering library
#
# Kodi uses only the poppler-cpp frontend, to rasterise pages of game manuals.
#
# This will define the following target:
#
#   ${APP_NAME_LC}::Poppler - The Poppler library
#   LIBRARY::Poppler - ALIAS target for the Poppler library

if(NOT TARGET ${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME})

  include(cmake/scripts/common/ModuleHelpers.cmake)

  macro(buildmacroPoppler)

    # Only the parsing and rasterising is wanted. Every other frontend and
    # backend is switched off: it keeps the library small, and each one that is
    # not built is one less piece of attack surface for a file format that
    # arrives from outside Kodi.
    set(CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF
                   -DBUILD_GTK_TESTS=OFF
                   -DBUILD_QT5_TESTS=OFF
                   -DBUILD_QT6_TESTS=OFF
                   -DBUILD_CPP_TESTS=OFF
                   -DBUILD_MANUAL_TESTS=OFF
                   -DENABLE_CPP=ON
                   -DENABLE_UTILS=OFF
                   -DENABLE_GLIB=OFF
                   -DENABLE_QT5=OFF
                   -DENABLE_QT6=OFF
                   -DENABLE_NSS3=OFF
                   -DENABLE_GPGME=OFF
                   -DENABLE_LIBCURL=OFF
                   -DENABLE_BOOST=OFF
                   -DENABLE_LIBOPENJPEG=none
                   -DENABLE_LIBTIFF=OFF
                   -DENABLE_LCMS=ON
                   -DWITH_Cairo=OFF
                   -DWITH_NSS3=OFF
                   -DRUN_GPERF_IF_PRESENT=OFF)

    BUILD_DEP_TARGET()

    # Poppler installs its headers under include/poppler, and the cpp frontend
    # includes its siblings unqualified, so that directory is on the path too
    set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_INCLUDE_DIR ${DEP_LOCATION}/include/poppler/cpp
                                                          ${DEP_LOCATION}/include/poppler)

    # poppler-cpp is a thin frontend over libpoppler, and the static build
    # leaves the two apart, so the core library has to be linked explicitly
    find_library(POPPLER_CORE_LIBRARY NAMES poppler
                 PATHS ${DEP_LOCATION}/lib
                 NO_CACHE
                 ${SEARCH_QUIET})

    if(POPPLER_CORE_LIBRARY)
      list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES ${POPPLER_CORE_LIBRARY})
    endif()

    # A static libpoppler carries no record of what it needs, so its
    # dependencies are named here. Kodi already builds every one of them.
    #
    #   freetype/fontconfig  drawing text, and finding the fonts to draw it with
    #   libjpeg              decoding the scans inside a PDF, which are usually JPEG
    #   libpng/zlib          decoding the other images, and the compressed streams
    #   lcms2                colour management
    find_package(FreeType REQUIRED ${SEARCH_QUIET})
    list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES ${APP_NAME_LC}::FreeType)

    find_package(PkgConfig ${SEARCH_QUIET})
    if(PKG_CONFIG_FOUND)
      foreach(_poppler_dep fontconfig libjpeg libpng zlib lcms2)
        # A distinct prefix per module: pkg_check_modules caches its results
        # against the prefix, so reusing one silently keeps the first answer
        string(TOUPPER ${_poppler_dep} _poppler_dep_prefix)
        set(_poppler_dep_prefix PC_POPPLER_${_poppler_dep_prefix})

        pkg_check_modules(${_poppler_dep_prefix} ${_poppler_dep} ${SEARCH_QUIET})

        if(${_poppler_dep_prefix}_FOUND)
          list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LINK_LIBRARIES
               ${${_poppler_dep_prefix}_LDFLAGS})
        endif()

        unset(_poppler_dep_prefix)
      endforeach()
      unset(_poppler_dep)
    endif()
  endmacro()

  set(${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC poppler)

  # The library is called poppler, but the frontend Kodi links is poppler-cpp
  set(${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME_PC poppler-cpp)

  SETUP_BUILD_VARS()

  SETUP_FIND_SPECS()

  SEARCH_EXISTING_PACKAGES()

  if(ENABLE_INTERNAL_POPPLER)
    message(STATUS "Building ${${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC}: \(version \"${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_VER}\"\)")
    cmake_language(EVAL CODE "
      buildmacro${CMAKE_FIND_PACKAGE_NAME}()
    ")
  endif()

  if(${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME}_FOUND)
    if(TARGET PkgConfig::${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME} AND NOT TARGET ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
      add_library(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} ALIAS PkgConfig::${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME})
      add_library(LIBRARY::${CMAKE_FIND_PACKAGE_NAME} ALIAS PkgConfig::${${CMAKE_FIND_PACKAGE_NAME}_SEARCH_NAME})
    elseif(TARGET ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
      SETUP_BUILD_TARGET()

      add_dependencies(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})

      set_target_properties(${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME} PROPERTIES LIB_BUILD ON)

      add_library(LIBRARY::${CMAKE_FIND_PACKAGE_NAME} ALIAS ${APP_NAME_LC}::${CMAKE_FIND_PACKAGE_NAME})
    endif()

    list(APPEND ${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_COMPILE_DEFINITIONS HAVE_POPPLER)

    ADD_TARGET_COMPILE_DEFINITION()

    ADD_MULTICONFIG_BUILDMACRO()
  else()
    if(Poppler_FIND_REQUIRED)
      message(FATAL_ERROR "Poppler libraries were not found. You may want to use -DENABLE_INTERNAL_POPPLER=ON")
    endif()
  endif()
endif()
