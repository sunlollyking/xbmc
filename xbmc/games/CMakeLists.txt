set(SOURCES AchievementRuntime.cpp
            GameThumbLoader.cpp
            GameManual.cpp
            GameServices.cpp
            GameSettings.cpp
            GameUtils.cpp
            VideoFilters.cpp)

set(HEADERS AchievementRuntime.h
            GameThumbLoader.h
            GameManual.h
            GameServices.h
            GameSettings.h
            GameTypes.h
            GameUtils.h
            VideoFilters.h)

core_add_library(games)
