/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DbUrl.h"

#include <string>
#include <string_view>
#include <vector>

class CVariant;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief What a gamedb:// path lists
 */
enum class GameDbNode
{
  NONE,
  OVERVIEW, // the top level, or a platform's top level
  PLATFORMS,
  GAMES,
  RELEASES,
  GENRES,
  YEARS,
  DEVELOPERS,
  PUBLISHERS,
  COLLECTIONS,
  TAGS,
  REGIONS,
  PLAYERS,
  AGERATINGS,
  CATEGORIES,
};

/*!
 * \ingroup games
 *
 * \brief A gamedb:// URL
 *
 * The path is a tree whose shape is the same at the root and under a platform:
 *
 *   gamedb://
 *   gamedb://platforms/
 *   gamedb://platforms/<id>/
 *   gamedb://[platforms/<id>/]titles/[<gameid>/]
 *   gamedb://[platforms/<id>/]<facet>/[<value>/]
 *   gamedb://[platforms/<id>/]<list>/
 *   gamedb://collections/<id>/
 *
 * A facet is genres, years, developers, publishers, collections, tags,
 * regions, players, ageratings or categories, and choosing a value lists the
 * games that have it. A list is a ready-made selection: recentlyadded,
 * recentlyplayed, neverplayed, favourites, completed, multiplayer, coop,
 * achievements, inprogress, hacks or homebrew.
 *
 * Parsing turns every path segment into a URL option (platformid, gameid,
 * genreid, year, ...) so that one filter builder serves every node, and the
 * path can be rebuilt from the options.
 */
class CGameDbUrl : public CDbUrl
{
public:
  CGameDbUrl();
  ~CGameDbUrl() override;

  /*!
   * \brief What this URL lists
   */
  GameDbNode GetNode() const { return m_node; }

  /*!
   * \brief The name of what this URL lists, e.g. "games", "platforms", "genres"
   */
  const std::string& GetItemType() const { return m_itemType; }

  /*!
   * \brief The ready-made selection the path asked for, or empty
   */
  const std::string& GetList() const { return m_list; }

  /*!
   * \brief Whether the URL is scoped to one platform
   */
  bool HasPlatform() const { return HasOption("platformid"); }

  /*!
   * \brief The facets a node offers, in display order
   */
  static const std::vector<std::string_view>& GetFacetNames();

  /*!
   * \brief The ready-made lists a node offers, in display order
   */
  static const std::vector<std::string_view>& GetListNames();

  /*!
   * \brief The node a path segment names, or NONE
   */
  static GameDbNode NodeFromSegment(std::string_view segment);

  /*!
   * \brief The path segment for a node
   */
  static std::string_view SegmentFromNode(GameDbNode node);

protected:
  bool parse() override;
  bool validateOption(const std::string& key, const CVariant& value) override;

private:
  GameDbNode m_node{GameDbNode::NONE};
  std::string m_itemType;
  std::string m_list;
};
} // namespace GAME
} // namespace KODI
