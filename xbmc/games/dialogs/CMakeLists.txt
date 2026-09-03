set(SOURCES GUIDialogGameContentSettings.cpp
            GUIDialogGameInfo.cpp
            GUIDialogSelectGameClient.cpp
            GUIDialogSelectSavestate.cpp
)

set(HEADERS GUIDialogGameContentSettings.h
            GUIDialogGameInfo.h
            DialogGameDefines.h
            GUIDialogSelectGameClient.h
            GUIDialogSelectSavestate.h
)

core_add_library(gamedialogs)
