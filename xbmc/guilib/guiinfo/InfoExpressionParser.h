/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/TimeFormat.h"

#include <string>
#include <vector>

namespace KODI::GUILIB::GUIINFO
{

/*!
 * \brief Pure expression parser for info label strings.
 *
 * Handles the syntactic parsing of info label expressions like
 * "Container(600).ListItem(1).Title" into structured components.
 * This class has no runtime dependencies (no ServiceBroker, no singletons).
 *
 * Works alongside CInfoLabelRegistry: the registry handles simple
 * "category.property" -> constant lookups, while this parser handles
 * complex parameterized expressions that need structural decomposition.
 */
class CInfoExpressionParser
{
public:
  /*!
   * \brief Represents a single component of a parsed info expression.
   *
   * Each component has a name and optional comma-separated parameters.
   * For example, "Container(600)" becomes Property{name="container", params={"600"}}.
   */
  class Property
  {
  public:
    Property(const std::string& property, const std::string& parameters);

    const std::string& Name() const { return m_name; }

    /*!
     * \brief Get the nth parameter (0-indexed).
     * \param n Parameter index.
     * \return The parameter string, or an empty string if index is out of range.
     */
    const std::string& param(size_t n = 0) const;

    /*!
     * \brief Get the number of parameters.
     * \return Parameter count.
     */
    unsigned int num_params() const;

  private:
    std::string m_name;
    std::vector<std::string> m_params;
  };

  /*!
   * \brief Split an info string into its constituent parts and parameters.
   *
   * Format is:
   *   info1(params1).info2(params2).info3(params3) ...
   *
   * where the parameters are an optional comma-separated parameter list.
   * Handles nested parentheses correctly.
   *
   * \param infoString The original string (e.g. "Container(600).ListItem(1).Title").
   * \param info The resulting vector of Property components.
   */
  static void SplitInfoString(const std::string& infoString,
                               std::vector<Property>& info);

  /*!
   * \brief Translate a time format string to its enum value.
   *
   * Handles formats like "hh", "mm:ss", "hh:mm:ss", "secs", "mins", etc.
   *
   * \param format The format string (case-insensitive).
   * \return The corresponding TIME_FORMAT enum value, or TIME_FORMAT_GUESS if unknown.
   */
  static TIME_FORMAT TranslateTimeFormat(const std::string& format);

  /*!
   * \brief Translate a list separator name to its string representation.
   *
   * Maps symbolic names to actual separator strings:
   *   "comma" -> ", "
   *   "pipe"  -> " | "
   *   "slash" -> " / "
   *   "cr"    -> "\n"
   *   etc.
   *
   * \param param The separator name (case-insensitive).
   * \return The separator string, or empty string if unrecognized.
   */
  static std::string TranslateListSeparator(const std::string& param);
};

} // namespace KODI::GUILIB::GUIINFO
