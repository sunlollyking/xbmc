set(SOURCES GameClientTable.cpp
            GameDatabase.cpp
            GameDatabaseArt.cpp
            GameDatabaseGames.cpp
            GameDatabaseNav.cpp
            GameDatabasePaths.cpp
            GameDatabasePlatforms.cpp
            GameLibraryDDL.cpp
            PathDefaults.cpp
            VideoFilterTable.cpp)

set(HEADERS GameClientTable.h
            GameDatabase.h
            GameDatabaseColumns.h
            GameLibraryDDL.h
            PathDefaults.h
            VideoFilterTable.h)

core_add_library(games_database)
