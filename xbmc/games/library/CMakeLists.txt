set(SOURCES GameDbUrl.cpp
            GameFileIdentity.cpp
            GameInfoScanner.cpp
            GameLibraryQueue.cpp
            GameLibraryTypes.cpp
            GameNameParser.cpp
            GameScraper.cpp
            PlatformCatalogue.cpp
            ReleasePolicy.cpp)

set(HEADERS GameDbUrl.h
            GameFileIdentity.h
            GameInfoScanner.h
            GameLibraryQueue.h
            GameLibraryTypes.h
            GameNameParser.h
            GameScraper.h
            PlatformCatalogue.h
            ReleasePolicy.h)

core_add_library(games_library)
