/*
 *  Copyright (C) 2017-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace tinyxml2
{
class XMLElement;
class XMLNode;
} // namespace tinyxml2

namespace KODI
{
namespace KEYMAP
{
/*!
 * \ingroup keymap
 */
class CIRTranslator
{
public:
  CIRTranslator() = default;

  /*!
   * \brief Clears the map
   */
  void Clear();

  static uint32_t TranslateButton(const tinyxml2::XMLElement* pButton);
  static uint32_t TranslateString(std::string strButton);

private:
  void MapRemote(tinyxml2::XMLNode* pRemote, const std::string& szDevice);
  static uint32_t ApplyModifiersToButton(const tinyxml2::XMLElement* pButton, uint32_t iButtonCode);

  //using IRButtonMap = std::map<uint32_t, std::string>;

  //std::map<std::string, std::shared_ptr<IRButtonMap>> m_irRemotesMap;
};
} // namespace KEYMAP
} // namespace KODI
