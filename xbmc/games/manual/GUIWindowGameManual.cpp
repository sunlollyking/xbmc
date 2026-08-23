/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIWindowGameManual.h"

#include "ManualCache.h"
#include "ManualPages.h"
#include "ManualPosition.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIControl.h"
#include "guilib/GUIImage.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "jobs/JobManager.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace KODI::GAME;

namespace
{
constexpr const char* PROPERTY_IMAGE = "Manual.Image";
constexpr const char* PROPERTY_PAGE = "Manual.Page";
constexpr const char* PROPERTY_PAGE_COUNT = "Manual.PageCount";
constexpr const char* PROPERTY_PAGE_LABEL = "Manual.PageLabel";
constexpr const char* PROPERTY_TITLE = "Manual.Title";
constexpr const char* PROPERTY_STATUS = "Manual.Status";
constexpr const char* PROPERTY_ZOOM_LABEL = "Manual.ZoomLabel";

//! The image control the page is displayed in
constexpr int CONTROL_PAGE = 9000;

//! The height to render a whole page at. Chosen to fill a 1080p screen with a
//! little to spare, so a page is not visibly soft when shown whole.
constexpr unsigned int BASE_RENDER_HEIGHT = 1200;

//! Zoom steps. The first is the whole page; the rest are multiples of it.
constexpr std::array<float, 4> ZOOM_LEVELS = {1.0f, 1.5f, 2.0f, 3.0f};

//! How far one press moves the page, as a fraction of the viewport
constexpr float PAN_STEP = 0.2f;

/*!
 * \brief Reads a manual off the GUI thread
 *
 * Opening means reading and parsing the whole document, which for a large
 * manual on a network share is far too long to hold the interface for.
 */
class CManualOpenJob : public CJob
{
public:
  explicit CManualOpenJob(std::string path) : m_path(std::move(path)) {}

  const char* GetType() const override { return "manual-open"; }

  bool DoWork() override
  {
    m_pages = OpenManualPages(m_path);
    return m_pages != nullptr;
  }

  //! The job is destroyed as soon as the callback returns, so the pages have
  //! to be taken out of it there
  std::unique_ptr<IManualPages> TakePages() { return std::move(m_pages); }

private:
  const std::string m_path;
  std::unique_ptr<IManualPages> m_pages;
};
} // namespace

CGUIWindowGameManual::CGUIWindowGameManual() : CGUIWindow(WINDOW_GAME_MANUAL, "GameManual.xml")
{
  // The window is loaded when it is first shown rather than at startup
  m_loadType = LOAD_ON_GUI_INIT;
}

CGUIWindowGameManual::~CGUIWindowGameManual() = default;

bool CGUIWindowGameManual::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_INIT:
    {
      // The document to show is passed when the window is activated
      const std::string path = message.GetStringParam();
      if (!path.empty())
        m_manualPath = path;

      break;
    }
    case GUI_MSG_USER:
    {
      // A background open finished
      FinishOpen();
      return true;
    }
    default:
      break;
  }

  return CGUIWindow::OnMessage(message);
}

void CGUIWindowGameManual::OnInitWindow()
{
  CGUIWindow::OnInitWindow();

  // The skin's layout is what zooming works outwards from, so it is taken
  // before anything moves the control
  if (!m_haveBaseRect)
  {
    const CGUIControl* control = GetControl(CONTROL_PAGE);
    if (control != nullptr)
    {
      m_baseX = control->GetXPosition();
      m_baseY = control->GetYPosition();
      m_baseWidth = control->GetWidth();
      m_baseHeight = control->GetHeight();
      m_haveBaseRect = m_baseWidth > 0.0f && m_baseHeight > 0.0f;
    }
  }

  m_page = 0;
  m_pageCount = 0;
  m_zoomLevel = 0;
  m_panX = 0.0f;
  m_panY = 0.0f;
  m_pages.reset();

  ApplyZoom();
  ShowOpeningState();

  BeginOpen();
}

void CGUIWindowGameManual::OnDeinitWindow(int nextWindowID)
{
  // Remember how far the player got before anything is torn down
  if (m_pageCount > 0)
    CManualPosition::GetInstance().SetPage(m_manualPath, m_page);

  AbandonOpen();

  // A manual is several megabytes held in memory, and there is no reason to
  // keep it once it is off screen
  m_pages.reset();

  ClearProperties();

  m_manualPath.clear();
  m_page = 0;
  m_pageCount = 0;
  m_zoomLevel = 0;
  m_panX = 0.0f;
  m_panY = 0.0f;

  CGUIWindow::OnDeinitWindow(nextWindowID);
}

void CGUIWindowGameManual::BeginOpen()
{
  if (m_manualPath.empty())
  {
    CLog::Log(LOGERROR, "CGUIWindowGameManual: opened with no document");
    CServiceBroker::GetGUI()->GetWindowManager().PreviousWindow();
    return;
  }

  const std::string path = m_manualPath;

  std::unique_lock<std::mutex> lock(m_openMutex);
  m_openedPages.reset();
  m_openJobID = CServiceBroker::GetJobManager()->AddJob(new CManualOpenJob(path), this);
}

void CGUIWindowGameManual::AbandonOpen()
{
  unsigned int jobID = 0;

  {
    std::unique_lock<std::mutex> lock(m_openMutex);

    // Clearing the id is what tells a late completion it is unwanted: the
    // callback only publishes pages for the job the window is still waiting on
    jobID = m_openJobID;
    m_openJobID = 0;
    m_openedPages.reset();
  }

  if (jobID != 0)
    CServiceBroker::GetJobManager()->CancelJob(jobID);
}

void CGUIWindowGameManual::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  {
    std::unique_lock<std::mutex> lock(m_openMutex);

    // The window may have closed, or moved to another manual, while this was
    // running. Either way its result is no longer wanted.
    if (jobID != m_openJobID)
      return;

    m_openJobID = 0;

    if (success)
    {
      auto* openJob = static_cast<CManualOpenJob*>(job);
      m_openedPages = openJob->TakePages();
    }
  }

  // Everything from here has to happen on the GUI thread
  CGUIMessage message(GUI_MSG_USER, GetID(), 0);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(message, GetID());
}

void CGUIWindowGameManual::FinishOpen()
{
  std::unique_ptr<IManualPages> pages;

  {
    std::unique_lock<std::mutex> lock(m_openMutex);
    pages = std::move(m_openedPages);
  }

  if (!pages)
  {
    CLog::Log(LOGERROR, "CGUIWindowGameManual: no readable pages in \"{}\"", m_manualPath);

    const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

    // "Manual", "This page could not be displayed"
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, strings.Get(35310),
                                          strings.Get(35314));

    CServiceBroker::GetGUI()->GetWindowManager().PreviousWindow();
    return;
  }

  m_pages = std::move(pages);
  m_pageCount = m_pages->GetPageCount();

  // Reading a manual is what keeps it: eviction takes the ones gone longest
  // without being opened, so the one reached for every session outlives one
  // downloaded later and never looked at
  CManualCache::GetInstance().Touch(m_manualPath);

  // Pick up where the player left off, in case the manual has been shortened
  // since it was last read
  m_page = std::min(CManualPosition::GetInstance().GetPage(m_manualPath), m_pageCount - 1);

  UpdateProperties();
}

void CGUIWindowGameManual::ShowOpeningState()
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  // An empty image is what the skin keys its placeholder off
  SetProperty(PROPERTY_IMAGE, "");
  SetProperty(PROPERTY_PAGE_LABEL, "");
  SetProperty(PROPERTY_ZOOM_LABEL, "");

  // "Opening manual…"
  SetProperty(PROPERTY_STATUS, strings.Get(35315));

  std::string title = URIUtils::GetFileName(m_manualPath);
  const size_t extension = title.find_last_of('.');
  if (extension != std::string::npos)
    title.erase(extension);

  SetProperty(PROPERTY_TITLE, title);
}

bool CGUIWindowGameManual::MovePage(int distance)
{
  if (m_pageCount == 0)
    return false;

  const int target = static_cast<int>(m_page) + distance;
  if (target < 0 || target >= static_cast<int>(m_pageCount))
    return false;

  m_page = static_cast<unsigned int>(target);

  // A new page starts at the top, since carrying the previous page's offset
  // over would drop the reader into the middle of it
  m_panX = 0.0f;
  m_panY = 0.0f;
  ApplyZoom();

  UpdateProperties();

  return true;
}

bool CGUIWindowGameManual::Zoom(int steps)
{
  if (m_pageCount == 0)
    return false;

  const int target = static_cast<int>(m_zoomLevel) + steps;
  if (target < 0 || target >= static_cast<int>(ZOOM_LEVELS.size()))
    return false;

  m_zoomLevel = static_cast<size_t>(target);

  if (IsFitted())
  {
    m_panX = 0.0f;
    m_panY = 0.0f;
  }

  ApplyZoom();

  // The page is re-requested at a resolution matching the new zoom, so that
  // magnifying it shows more detail rather than larger pixels
  UpdateProperties();

  return true;
}

bool CGUIWindowGameManual::GetPageSize(float& width, float& height)
{
  if (!m_haveBaseRect)
    return false;

  const auto* image = dynamic_cast<const CGUIImage*>(GetControl(CONTROL_PAGE));
  if (image == nullptr)
    return false;

  const float textureWidth = image->GetTextureWidth();
  const float textureHeight = image->GetTextureHeight();
  if (textureWidth <= 0.0f || textureHeight <= 0.0f)
    return false;

  // The page fills as much of the skin's area as its shape allows, then grows
  // with the zoom
  const float fit = std::min(m_baseWidth / textureWidth, m_baseHeight / textureHeight);
  const float zoom = ZOOM_LEVELS[m_zoomLevel];

  width = textureWidth * fit * zoom;
  height = textureHeight * fit * zoom;

  return true;
}

bool CGUIWindowGameManual::Pan(int right, int down)
{
  if (IsFitted())
    return false;

  const float beforeX = m_panX;
  const float beforeY = m_panY;

  m_panX -= right * m_baseWidth * PAN_STEP;
  m_panY -= down * m_baseHeight * PAN_STEP;

  // ApplyZoom() is what holds the page against its edges, so the clamped
  // values come back from there rather than being worked out twice
  ApplyZoom();

  return std::fabs(m_panX - beforeX) > 0.01f || std::fabs(m_panY - beforeY) > 0.01f;
}

void CGUIWindowGameManual::ApplyZoom()
{
  if (!m_haveBaseRect)
    return;

  CGUIControl* control = GetControl(CONTROL_PAGE);
  if (control == nullptr)
    return;

  float width = 0.0f;
  float height = 0.0f;

  if (!GetPageSize(width, height))
  {
    // The page has not loaded yet, so there is nothing to size against. It is
    // left where the skin put it until the texture arrives, at which point
    // Process() calls back here.
    control->SetPosition(m_baseX, m_baseY);
    control->SetWidth(m_baseWidth);
    control->SetHeight(m_baseHeight);
    return;
  }

  // The page can move until its edge would come inside the viewport. A page
  // smaller than the viewport in one direction cannot move in it at all.
  const float limitX = std::max(0.0f, (width - m_baseWidth) / 2.0f);
  const float limitY = std::max(0.0f, (height - m_baseHeight) / 2.0f);

  m_panX = std::clamp(m_panX, -limitX, limitX);
  m_panY = std::clamp(m_panY, -limitY, limitY);

  // The control is made the size of the page itself rather than of the area
  // the skin set aside. That leaves no letterboxing inside it, so panning
  // moves the page by exactly as much as it appears to move.
  const float centreX = m_baseX + m_baseWidth / 2.0f + m_panX;
  const float centreY = m_baseY + m_baseHeight / 2.0f + m_panY;

  control->SetPosition(centreX - width / 2.0f, centreY - height / 2.0f);
  control->SetWidth(width);
  control->SetHeight(height);
}

void CGUIWindowGameManual::Process(unsigned int currentTime, CDirtyRegionList& dirtyregions)
{
  // A page's shape is only known once its texture has loaded, and it changes
  // from page to page. Reapplying every frame keeps the geometry right without
  // having to be told when a texture arrives.
  ApplyZoom();

  CGUIWindow::Process(currentTime, dirtyregions);
}

void CGUIWindowGameManual::UpdateProperties()
{
  if (!m_pages || m_pageCount == 0)
    return;

  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  const float zoom = ZOOM_LEVELS[m_zoomLevel];

  // Rendering at the zoomed size keeps the page sharp. The loader clamps this
  // to the GPU's texture limit, so asking for more than it can hold is safe.
  const auto renderHeight =
      static_cast<unsigned int>(std::lround(BASE_RENDER_HEIGHT * static_cast<double>(zoom)));

  SetProperty(PROPERTY_IMAGE, m_pages->GetPageImage(m_page, renderHeight));

  // The document is open, so the placeholder has nothing left to say
  SetProperty(PROPERTY_STATUS, "");

  // Pages are counted from zero internally but read from one
  SetProperty(PROPERTY_PAGE, static_cast<int>(m_page + 1));
  SetProperty(PROPERTY_PAGE_COUNT, static_cast<int>(m_pageCount));

  // "Page {0:d} of {1:d}"
  SetProperty(PROPERTY_PAGE_LABEL,
              StringUtils::Format(strings.Get(35313), m_page + 1, m_pageCount));

  if (IsFitted())
  {
    // A page shown whole is the normal case and needs no announcement
    SetProperty(PROPERTY_ZOOM_LABEL, "");
  }
  else
  {
    // "Zoom {0:d}%"
    SetProperty(PROPERTY_ZOOM_LABEL,
                StringUtils::Format(strings.Get(35316), std::lround(zoom * 100.0f)));
  }
}

bool CGUIWindowGameManual::OnAction(const CAction& action)
{
  switch (action.GetID())
  {
    case ACTION_ZOOM_IN:
      Zoom(1);
      return true;

    case ACTION_ZOOM_OUT:
      Zoom(-1);
      return true;

    case ACTION_MOVE_RIGHT:
      // Whole page: turn it. Zoomed in: move across it, and only turn once
      // the edge has been reached, so a page can be read right to left
      // without dropping out of zoom.
      if (IsFitted() || !Pan(1, 0))
        MovePage(1);
      return true;

    case ACTION_MOVE_LEFT:
      if (IsFitted() || !Pan(-1, 0))
        MovePage(-1);
      return true;

    case ACTION_MOVE_UP:
      Pan(0, -1);
      return true;

    case ACTION_MOVE_DOWN:
      Pan(0, 1);
      return true;

    case ACTION_NEXT_ITEM:
    case ACTION_PAGE_DOWN:
      MovePage(1);
      return true;

    case ACTION_PREV_ITEM:
    case ACTION_PAGE_UP:
      MovePage(-1);
      return true;

    default:
      break;
  }

  return CGUIWindow::OnAction(action);
}
