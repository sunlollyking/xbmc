set(SOURCES GameAgeRatings.cpp
            GameFileIdentity.cpp
            GameLibraryTypes.cpp
            GameNameParser.cpp
            PlatformCatalogue.cpp
            ReleasePolicy.cpp)

set(HEADERS GameAgeRatings.h
            GameFileIdentity.h
            GameLibraryTypes.h
            GameNameParser.h
            PlatformCatalogue.h
            ReleasePolicy.h)

core_add_library(games_library)
