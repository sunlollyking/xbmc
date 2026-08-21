/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/GUIWindow.h"
#include "jobs/IJobCallback.h"
#include "jobs/Job.h"

#include <memory>
#include <mutex>
#include <string>

namespace KODI
{
namespace GAME
{
class IManualPages;

/*!
 * \brief Shows the manual that came with a game
 *
 * The window holds the position in the document and the zoom. The page itself
 * is put on screen by an ordinary image control in the skin, pointed at the
 * image URL published as a window property, so Kodi's background loader
 * fetches and caches pages the same way it does any other image.
 *
 * Opening the document reads and parses the whole file, so it happens on a
 * background job: a large manual on a network share would otherwise freeze the
 * interface for as long as the read took.
 *
 * Opened with the path to the document as the first window parameter.
 */
class CGUIWindowGameManual : public CGUIWindow, public IJobCallback
{
public:
  CGUIWindowGameManual();
  ~CGUIWindowGameManual() override;

  // Implementation of CGUIControl via CGUIWindow
  bool OnAction(const CAction& action) override;
  bool OnMessage(CGUIMessage& message) override;

  // Implementation of IJobCallback
  void OnJobComplete(unsigned int jobID, bool success, CJob* job) override;

  // Implementation of CGUIControl via CGUIWindow
  void Process(unsigned int currentTime, CDirtyRegionList& dirtyregions) override;

protected:
  // Implementation of CGUIWindow
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;

private:
  /*!
   * \brief Start reading the document in the background
   */
  void BeginOpen();

  /*!
   * \brief Take the pages a finished job produced and show the first one
   */
  void FinishOpen();

  /*!
   * \brief Stop caring about a job that is still running
   *
   * The window outlives any one document, so a job that finishes after the
   * viewer has moved on must not be allowed to publish its pages.
   */
  void AbandonOpen();

  /*!
   * \brief Move by a number of pages, stopping at either end
   *
   * Paging past the end is a no-op rather than a wrap, because a manual is
   * read in order and wrapping from the back cover to the front reads as a
   * glitch.
   *
   * \return True if the page changed
   */
  bool MovePage(int distance);

  /*!
   * \brief Change the zoom by a number of steps, stopping at either end
   *
   * \return True if the zoom changed
   */
  bool Zoom(int steps);

  /*!
   * \brief Pan the zoomed page, in multiples of a step
   *
   * \return True if the view moved, false if it was already against the edge
   */
  bool Pan(int right, int down);

  /*!
   * \brief Whether the page is shown whole
   *
   * While it is, the arrows turn pages. Once zoomed in they move around the
   * page instead, which is the same way Kodi's picture viewer behaves.
   */
  bool IsFitted() const { return m_zoomLevel == 0; }

  /*!
   * \brief The size the page is drawn at, at the current zoom
   *
   * Taken from the loaded texture, because a page's shape is not known until
   * it has been read, and differs from page to page in a scanned manual.
   *
   * \return False if no page has loaded yet
   */
  bool GetPageSize(float& width, float& height);

  /*!
   * \brief Position and size the image control for the current zoom and pan
   *
   * Also holds the pan within the page's edges, so that it is the one place
   * the limits are worked out.
   */
  void ApplyZoom();

  /*!
   * \brief Publish the current position for the skin to display
   */
  void UpdateProperties();

  /*!
   * \brief Show the placeholder while the document is being read
   */
  void ShowOpeningState();

  std::string m_manualPath;
  std::unique_ptr<IManualPages> m_pages;

  unsigned int m_page{0};
  unsigned int m_pageCount{0};

  //! Index into the zoom levels; 0 is the whole page
  size_t m_zoomLevel{0};

  //! Offset of the zoomed page from centred, in pixels
  float m_panX{0.0f};
  float m_panY{0.0f};

  //! The image control's geometry as the skin laid it out, which zooming
  //! works outwards from
  bool m_haveBaseRect{false};
  float m_baseX{0.0f};
  float m_baseY{0.0f};
  float m_baseWidth{0.0f};
  float m_baseHeight{0.0f};

  //! Guards the handover from the job thread
  std::mutex m_openMutex;
  unsigned int m_openJobID{0};

  std::unique_ptr<IManualPages> m_openedPages;
};

} // namespace GAME
} // namespace KODI
