/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "InfoExpressionParser.h"

#include "Util.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace KODI::GUILIB::GUIINFO;

CInfoExpressionParser::Property::Property(const std::string& property,
                                          const std::string& parameters)
  : m_name(property)
{
  CUtil::SplitParams(parameters, m_params);
}

const std::string& CInfoExpressionParser::Property::param(size_t n /* = 0 */) const
{
  if (n < m_params.size())
    return m_params[n];
  return StringUtils::Empty;
}

unsigned int CInfoExpressionParser::Property::num_params() const
{
  return m_params.size();
}

void CInfoExpressionParser::SplitInfoString(const std::string& infoString,
                                            std::vector<Property>& info)
{
  // our string is of the form:
  // category[(params)][.info(params).info2(params)] ...
  // so we need to split on . while taking into account of () pairs
  unsigned int parentheses = 0;
  std::string property;
  std::string param;
  for (const char c : infoString)
  {
    if (c == '(')
    {
      if (!parentheses++)
        continue;
    }
    else if (c == ')')
    {
      if (!parentheses)
        CLog::Log(LOGERROR, "unmatched parentheses in {}", infoString);
      else if (!--parentheses)
        continue;
    }
    else if (c == '.' && !parentheses)
    {
      if (!property.empty()) // add our property and parameters
      {
        StringUtils::ToLower(property);
        info.emplace_back(property, param);
      }
      property.clear();
      param.clear();
      continue;
    }
    if (parentheses)
      param += c;
    else
      property += c;
  }

  if (parentheses)
    CLog::Log(LOGERROR, "unmatched parentheses in {}", infoString);

  if (!property.empty())
  {
    StringUtils::ToLower(property);
    info.emplace_back(property, param);
  }
}

TIME_FORMAT CInfoExpressionParser::TranslateTimeFormat(const std::string& format)
{
  if (format.empty())
    return TIME_FORMAT_GUESS;
  else if (StringUtils::EqualsNoCase(format, "hh"))
    return TIME_FORMAT_HH;
  else if (StringUtils::EqualsNoCase(format, "mm"))
    return TIME_FORMAT_MM;
  else if (StringUtils::EqualsNoCase(format, "ss"))
    return TIME_FORMAT_SS;
  else if (StringUtils::EqualsNoCase(format, "hh:mm"))
    return TIME_FORMAT_HH_MM;
  else if (StringUtils::EqualsNoCase(format, "mm:ss"))
    return TIME_FORMAT_MM_SS;
  else if (StringUtils::EqualsNoCase(format, "hh:mm:ss"))
    return TIME_FORMAT_HH_MM_SS;
  else if (StringUtils::EqualsNoCase(format, "hh:mm:ss xx"))
    return TIME_FORMAT_HH_MM_SS_XX;
  else if (StringUtils::EqualsNoCase(format, "h"))
    return TIME_FORMAT_H;
  else if (StringUtils::EqualsNoCase(format, "m"))
    return TIME_FORMAT_M;
  else if (StringUtils::EqualsNoCase(format, "h:mm:ss"))
    return TIME_FORMAT_H_MM_SS;
  else if (StringUtils::EqualsNoCase(format, "h:mm:ss xx"))
    return TIME_FORMAT_H_MM_SS_XX;
  else if (StringUtils::EqualsNoCase(format, "xx"))
    return TIME_FORMAT_XX;
  else if (StringUtils::EqualsNoCase(format, "secs"))
    return TIME_FORMAT_SECS;
  else if (StringUtils::EqualsNoCase(format, "mins"))
    return TIME_FORMAT_MINS;
  else if (StringUtils::EqualsNoCase(format, "hours"))
    return TIME_FORMAT_HOURS;
  return TIME_FORMAT_GUESS;
}

std::string CInfoExpressionParser::TranslateListSeparator(const std::string& param)
{
  if (StringUtils::EqualsNoCase(param, "comma"))
    return ", ";
  else if (StringUtils::EqualsNoCase(param, "pipe"))
    return " | ";
  else if (StringUtils::EqualsNoCase(param, "slash"))
    return " / ";
  else if (StringUtils::EqualsNoCase(param, "cr"))
    return "\n";
  else if (StringUtils::EqualsNoCase(param, "dash"))
    return " - ";
  else if (StringUtils::EqualsNoCase(param, "colon"))
    return " : ";
  else if (StringUtils::EqualsNoCase(param, "semicolon"))
    return "; ";
  else if (StringUtils::EqualsNoCase(param, "fullstop"))
    return ". ";
  else
  {
    CLog::Log(LOGERROR, "unhandled separator param {}", param);
    return {};
  }
}
