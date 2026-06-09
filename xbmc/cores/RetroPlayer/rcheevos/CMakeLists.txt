# rcheevos v12.3.0 - vendored RetroAchievements client library
# https://github.com/RetroAchievements/rcheevos

file(GLOB RCHEEVOS_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rcheevos/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rapi/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rhash/*.c
)

# Exclude raintegration (requires Windows SDK) and test files
list(FILTER RCHEEVOS_SOURCES EXCLUDE REGEX "rc_client_raintegration\\.c$")
list(FILTER RCHEEVOS_SOURCES EXCLUDE REGEX "rc_validate\\.c$")

add_library(rcheevos STATIC ${RCHEEVOS_SOURCES})

target_include_directories(rcheevos
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(rcheevos PRIVATE
  RC_DISABLE_LUA=1
)

set_target_properties(rcheevos PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)
