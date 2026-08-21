set(SOURCES ArchiveManualPages.cpp
            GUIDialogGameManuals.cpp
            GUIWindowGameManual.cpp
            ManualPages.cpp
            ManualPosition.cpp)

set(HEADERS ArchiveManualPages.h
            GUIDialogGameManuals.h
            GUIWindowGameManual.h
            ManualPages.h
            ManualPosition.h)

# Comic archives are read with Kodi's own archive support, so the viewer is
# useful without a PDF library. Only the PDF half needs one.
if(TARGET ${APP_NAME_LC}::Poppler)
  list(APPEND SOURCES PdfDocument.cpp
                      PdfDocumentCache.cpp
                      PdfImageFileLoader.cpp
                      PdfManualPages.cpp)

  list(APPEND HEADERS PdfDocument.h
                      PdfDocumentCache.h
                      PdfImageFileLoader.h
                      PdfManualPages.h)
endif()

core_add_library(games_manual)
