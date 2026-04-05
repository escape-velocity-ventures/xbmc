/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/Database.h"

namespace KODI::DATABASE
{
class CMusicSchemaManager
{
public:
  CMusicSchemaManager() = delete;

  /*!
   * \brief Create the tables of the music database
   * \param[in] db the database
   */
  static void CreateTables(CDatabase& db);

  /*!
   * \brief Create the indexes, triggers and views of the music database
   * \param[in] db the database
   */
  static void CreateAnalytics(CDatabase& db);

  /*!
   * \brief Create triggers for tracking removed artist links
   * \param[in] db the database
   */
  static void CreateRemovedLinkTriggers(CDatabase& db);

  /*!
   * \brief Create the database views for songs, albums, artists
   * \param[in] db the database
   */
  static void CreateViews(CDatabase& db);

  /*!
   * \brief Create native MySQL/MariaDB functions for sorting
   * \param[in] db the database
   */
  static void CreateNativeDBFunctions(CDatabase& db);
};
} // namespace KODI::DATABASE
