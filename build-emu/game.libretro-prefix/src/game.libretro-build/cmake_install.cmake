# Install script for directory: /home/itsmechrissyd/xbmc/build-emu/build/game.libretro

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/itsmechrissyd/xbmc/cmake/addons/output/addons")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.5.0"
      "$ENV{DESTDIR}/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.5.0;/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.0")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro" TYPE SHARED_LIBRARY FILES
    "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/game.libretro.so.22.5.0"
    "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/game.libretro.so.22.0"
    )
  foreach(file
      "$ENV{DESTDIR}/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.5.0"
      "$ENV{DESTDIR}/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so.22.0"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro/game.libretro.so")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/home/itsmechrissyd/xbmc/build-emu/build/depends/lib/kodi/addons/game.libretro" TYPE SHARED_LIBRARY FILES "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/game.libretro.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/CMakeFiles/game.libretro.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/home/itsmechrissyd/xbmc/build-emu/build/depends/share/kodi/addons/game.libretro;/home/itsmechrissyd/xbmc/build-emu/build/depends/share/kodi/addons/game.libretro")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/home/itsmechrissyd/xbmc/build-emu/build/depends/share/kodi/addons" TYPE DIRECTORY FILES
    "/home/itsmechrissyd/xbmc/build-emu/build/game.libretro/game.libretro"
    "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/game.libretro"
    REGEX ".+\\.xml\\.in(clude)?$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
