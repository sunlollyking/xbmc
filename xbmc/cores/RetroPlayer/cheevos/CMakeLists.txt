set(SOURCES Cheevos.cpp)
set(HEADERS Cheevos.h
            RConsoleIDs.h)

core_add_library(retroplayer_cheevos)

target_link_libraries(retroplayer_cheevos PRIVATE rcheevos)
