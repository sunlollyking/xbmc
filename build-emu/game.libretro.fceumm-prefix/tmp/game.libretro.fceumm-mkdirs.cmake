# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/itsmechrissyd/xbmc/build-emu/build/game.libretro.fceumm"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src/game.libretro.fceumm-build"
  "/home/itsmechrissyd/xbmc/build-emu/.install"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/tmp"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src/game.libretro.fceumm-stamp"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src"
  "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src/game.libretro.fceumm-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src/game.libretro.fceumm-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/itsmechrissyd/xbmc/build-emu/game.libretro.fceumm-prefix/src/game.libretro.fceumm-stamp${cfgdir}") # cfgdir has leading slash
endif()
