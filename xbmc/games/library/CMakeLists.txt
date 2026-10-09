set(SOURCES GameAgeRatings.cpp
            GameFileIdentity.cpp
            GameLibraryTypes.cpp
            GameListXml.cpp
            GameNameParser.cpp
            GameScraper.cpp
            PlatformCatalogue.cpp
            ReleasePolicy.cpp)

set(HEADERS GameAgeRatings.h
            GameFileIdentity.h
            GameLibraryTypes.h
            GameListXml.h
            GameNameParser.h
            GameScraper.h
            PlatformCatalogue.h
            ReleasePolicy.h)

core_add_library(games_library)
