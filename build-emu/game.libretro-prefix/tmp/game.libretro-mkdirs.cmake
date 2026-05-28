# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/itsmechrissyd/xbmc/build-emu/build/game.libretro"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-build"
  "/home/itsmechrissyd/xbmc/cmake/addons/output/addons"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/tmp"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-stamp"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/itsmechrissyd/xbmc/build-emu/game.libretro-prefix/src/game.libretro-stamp${cfgdir}") # cfgdir has leading slash
endif()
