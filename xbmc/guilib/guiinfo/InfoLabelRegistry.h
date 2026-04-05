/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <unordered_map>

namespace KODI::GUILIB::GUIINFO
{

/*!
 * \brief Data-driven registry for info label lookups.
 *
 * Replaces O(n) linear scans through InfoMap arrays with O(1) hash lookups.
 * Only handles simple "category.property" -> constant mappings. Parameterized
 * labels (e.g. Container(x).ListItem(n).Property) remain in the original
 * dispatch code in CGUIInfoManager::TranslateSingleString().
 */
class CInfoLabelRegistry
{
public:
  /*!
   * \brief Look up a simple info label by its full dot-notation key.
   * \param label The label string, e.g. "player.hasaudio" or "system.platform.linux".
   *              Must already be lowercased.
   * \return The info label constant, or 0 if not found.
   */
  static int LookupLabel(const std::string& label);

  /*!
   * \brief Look up a music player property by name.
   * \param property The property name, e.g. "title", "album". Already lowercased.
   * \return The info label constant, or 0 if not found.
   */
  static int LookupMusicPlayerProperty(const std::string& property);

  /*!
   * \brief Look up a video player property by name.
   * \param property The property name, e.g. "title", "genre". Already lowercased.
   * \return The info label constant, or 0 if not found.
   */
  static int LookupVideoPlayerProperty(const std::string& property);

  /*!
   * \brief Look up a player property by name.
   * \param property The property name, e.g. "hasmedia", "playing". Already lowercased.
   * \return The info label constant, or 0 if not found.
   */
  static int LookupPlayerProperty(const std::string& property);

private:
  static const std::unordered_map<std::string, int> s_labelMap;
  static const std::unordered_map<std::string, int> s_musicPlayerMap;
  static const std::unordered_map<std::string, int> s_videoPlayerMap;
  static const std::unordered_map<std::string, int> s_playerMap;
};

} // namespace KODI::GUILIB::GUIINFO
