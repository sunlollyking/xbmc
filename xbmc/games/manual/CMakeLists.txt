# Nothing here can be built without a PDF library, and a manual viewer with no
# renderer is not worth compiling in
if(TARGET ${APP_NAME_LC}::Poppler)
  set(SOURCES GUIWindowGameManual.cpp
              PdfDocument.cpp
              PdfDocumentCache.cpp
              PdfImageFileLoader.cpp)

  set(HEADERS GUIWindowGameManual.h
              PdfDocument.h
              PdfDocumentCache.h
              PdfImageFileLoader.h)

  core_add_library(games_manual)
endif()
