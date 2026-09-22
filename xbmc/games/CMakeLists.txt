set(SOURCES AchievementRuntime.cpp
            ContextMenus.cpp
            GameThumbLoader.cpp
            GameManual.cpp
            GameServices.cpp
            GameSettings.cpp
            GameUtils.cpp
            VideoFilters.cpp)

set(HEADERS AchievementRuntime.h
            ContextMenus.h
            GameThumbLoader.h
            GameManual.h
            GameServices.h
            GameSettings.h
            GameTypes.h
            GameUtils.h
            VideoFilters.h)

core_add_library(games)
