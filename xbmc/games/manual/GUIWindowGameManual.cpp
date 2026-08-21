/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIWindowGameManual.h"

#include "PdfDocumentCache.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "imagefiles/ImageFileURL.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace KODI::GAME;

namespace
{
constexpr const char* PROPERTY_IMAGE = "Manual.Image";
constexpr const char* PROPERTY_PAGE = "Manual.Page";
constexpr const char* PROPERTY_PAGE_COUNT = "Manual.PageCount";
constexpr const char* PROPERTY_PAGE_LABEL = "Manual.PageLabel";
constexpr const char* PROPERTY_TITLE = "Manual.Title";

//! The height to render pages at. Chosen to fill a 1080p screen with a little
//! to spare, so a page is not visibly soft when shown whole.
constexpr const char* RENDER_HEIGHT = "1200";
} // namespace

CGUIWindowGameManual::CGUIWindowGameManual() : CGUIWindow(WINDOW_GAME_MANUAL, "GameManual.xml")
{
  // The window is loaded when it is first shown rather than at startup
  m_loadType = LOAD_ON_GUI_INIT;
}

CGUIWindowGameManual::~CGUIWindowGameManual() = default;

bool CGUIWindowGameManual::OnMessage(CGUIMessage& message)
{
  if (message.GetMessage() == GUI_MSG_WINDOW_INIT)
  {
    // The document to show is passed when the window is activated
    const std::string path = message.GetStringParam();
    if (!path.empty())
    {
      m_manualPath = path;
      m_page = 0;
    }
  }

  return CGUIWindow::OnMessage(message);
}

void CGUIWindowGameManual::OnInitWindow()
{
  CGUIWindow::OnInitWindow();

  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  // Opening the document here rather than when the action fires means the page
  // count is known before the first page is asked for, so the position can be
  // shown straight away instead of appearing once a page has loaded
  m_pageCount = CPdfDocumentCache::GetInstance().GetPageCount(m_manualPath);

  if (m_pageCount == 0)
  {
    CLog::Log(LOGERROR, "CGUIWindowGameManual: no readable pages in \"{}\"", m_manualPath);

    // "Manual", "This page could not be displayed"
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, strings.Get(35310),
                                          strings.Get(35314));

    CServiceBroker::GetGUI()->GetWindowManager().PreviousWindow();
    return;
  }

  if (m_page >= m_pageCount)
    m_page = 0;

  UpdateProperties();
}

void CGUIWindowGameManual::OnDeinitWindow(int nextWindowID)
{
  // A manual is several megabytes held in memory, and there is no reason to
  // keep it once it is off screen
  CPdfDocumentCache::GetInstance().Clear();

  ClearProperties();

  m_manualPath.clear();
  m_page = 0;
  m_pageCount = 0;

  CGUIWindow::OnDeinitWindow(nextWindowID);
}

bool CGUIWindowGameManual::MovePage(int distance)
{
  if (m_pageCount == 0)
    return false;

  const int target = static_cast<int>(m_page) + distance;
  if (target < 0 || target >= static_cast<int>(m_pageCount))
    return false;

  m_page = static_cast<unsigned int>(target);

  UpdateProperties();

  return true;
}

void CGUIWindowGameManual::UpdateProperties()
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  IMAGE_FILES::CImageFileURL imageURL = IMAGE_FILES::CImageFileURL::FromFile(m_manualPath, "pdf");
  imageURL.AddOption("page", std::to_string(m_page));
  imageURL.AddOption("height", RENDER_HEIGHT);

  SetProperty(PROPERTY_IMAGE, imageURL.ToString());

  // Pages are counted from zero internally but read from one
  SetProperty(PROPERTY_PAGE, static_cast<int>(m_page + 1));
  SetProperty(PROPERTY_PAGE_COUNT, static_cast<int>(m_pageCount));

  // "Page {0:d} of {1:d}"
  SetProperty(PROPERTY_PAGE_LABEL,
              StringUtils::Format(strings.Get(35313), m_page + 1, m_pageCount));

  // The extension is dropped literally rather than with RemoveExtension(),
  // which only strips extensions Kodi has been told about by an add-on and so
  // would leave ".pdf" on the end of every manual's title
  std::string title = URIUtils::GetFileName(m_manualPath);
  const size_t extension = title.find_last_of('.');
  if (extension != std::string::npos)
    title.erase(extension);

  SetProperty(PROPERTY_TITLE, title);
}

bool CGUIWindowGameManual::OnAction(const CAction& action)
{
  switch (action.GetID())
  {
    case ACTION_MOVE_RIGHT:
    case ACTION_NEXT_ITEM:
    case ACTION_PAGE_DOWN:
      MovePage(1);
      return true;

    case ACTION_MOVE_LEFT:
    case ACTION_PREV_ITEM:
    case ACTION_PAGE_UP:
      MovePage(-1);
      return true;

    default:
      break;
  }

  return CGUIWindow::OnAction(action);
}
