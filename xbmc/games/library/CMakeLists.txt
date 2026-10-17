set(SOURCES GameAgeRatings.cpp
            GameDbUrl.cpp
            GameFileIdentity.cpp
            GameInfoScanner.cpp
            GameLibraryQueue.cpp
            GameLibraryTypes.cpp
            GameListXml.cpp
            GameNameParser.cpp
            GameScraper.cpp
            PlatformCatalogue.cpp
            ReleasePolicy.cpp)

set(HEADERS GameAgeRatings.h
            GameDbUrl.h
            GameFileIdentity.h
            GameInfoScanner.h
            GameLibraryQueue.h
            GameLibraryTypes.h
            GameListXml.h
            GameNameParser.h
            GameScraper.h
            PlatformCatalogue.h
            ReleasePolicy.h)

core_add_library(games_library)
