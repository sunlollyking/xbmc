set(SOURCES GUIDialogGameContentSettings.cpp
            GUIDialogSelectGameClient.cpp
            GUIDialogSelectSavestate.cpp
)

set(HEADERS GUIDialogGameContentSettings.h
            DialogGameDefines.h
            GUIDialogSelectGameClient.h
            GUIDialogSelectSavestate.h
)

core_add_library(gamedialogs)
