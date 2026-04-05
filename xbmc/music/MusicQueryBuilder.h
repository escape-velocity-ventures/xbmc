/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/Database.h"
#include "utils/SortUtils.h"

#include <set>
#include <string>

class CDbUrl;
class CMusicDatabase;
class CVariant;

namespace dbiplus
{
class Dataset;
} // namespace dbiplus

/*!
 \ingroup music
 \brief Builds SQL queries and JSON-RPC result sets for the music database.

 CMusicQueryBuilder extracts SQL generation and JSON-RPC query logic from
 CMusicDatabase into a focused class. Methods that need database access
 take a CMusicDatabase reference; pure SQL generators are static.
 */
class CMusicQueryBuilder
{
public:
  /*! \brief Build SQL for sort subquery from ignore article token list.
   \param strField original name or title field that articles could be removed from
   \param db the music database (for PrepareSQL access)
   \return SQL string e.g.  WHEN strField LIKE 'the_' ESCAPE '_' THEN SUBSTR(strArtist, 5)
   */
  static std::string GetIgnoreArticleSQL(const std::string& strField,
                                         const CMusicDatabase& db);

  /*! \brief Build SQL for sort name scalar subquery from sort attributes and ignore article list.
   \param strAlias alias name of scalar subquery field
   \param sortAttributes the sort attributes e.g. SortAttributeIgnoreArticle
   \param strField original name or title field that articles could be removed from
   \param strSortField sort name or title field to be used instead of original (when data not null)
   \param db the music database (for PrepareSQL access)
   \return SQL string for CASE expression
   */
  static std::string SortnameBuildSQL(const std::string& strAlias,
                                      const SortAttribute& sortAttributes,
                                      const std::string& strField,
                                      const std::string& strSortField,
                                      const CMusicDatabase& db);

  /*! \brief Build SQL for sorting field naturally and case-insensitively (in SQLite).
   \param strField field name
   \param sortOrder the sort order
   \param db the music database (for PrepareSQL access)
   \return SQL string for ORDER BY clause
   */
  static std::string AlphanumericSortSQL(const std::string& strField,
                                         const SortOrder& sortOrder,
                                         const CMusicDatabase& db);

  /*! \brief Populate filter with ORDER BY clause and extra scalar query fields needed for sort.
   \param type the media type (e.g. MediaTypeArtist)
   \param sorting the sort description
   \param filter [in/out] the filter to populate with order/fields
   \param db the music database (for PrepareSQL and sort SQL access)
   \return the number of fields added to the filter
   */
  static int GetOrderFilter(const std::string& type,
                            const SortDescription& sorting,
                            CDatabase::Filter& filter,
                            const CMusicDatabase& db);

  /*! \brief Build a filter from a music database URL and its options.
   \param musicUrl the database URL with filter options
   \param filter [in/out] the filter to populate
   \param sorting [in/out] the sort description (may be modified by xsp options)
   \param db the music database (for name lookups and PrepareSQL)
   \return true on success
   */
  static bool GetFilter(CDbUrl& musicUrl,
                        CDatabase::Filter& filter,
                        SortDescription& sorting,
                        CMusicDatabase& db);

  /*! \brief Retrieve artists matching criteria and return as JSON-RPC result.
   \param fields set of requested JSON field names
   \param baseDir the base directory URL with filter options
   \param result [out] the JSON-RPC result
   \param total [out] total number of matching artists (before limits)
   \param sortDescription the sort description
   \param db the music database
   \return true on success
   */
  static bool GetArtistsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                    const std::string& baseDir,
                                    CVariant& result,
                                    int& total,
                                    const SortDescription& sortDescription,
                                    CMusicDatabase& db);

  /*! \brief Retrieve albums matching criteria and return as JSON-RPC result.
   \param fields set of requested JSON field names
   \param baseDir the base directory URL with filter options
   \param result [out] the JSON-RPC result
   \param total [out] total number of matching albums (before limits)
   \param sortDescription the sort description
   \param db the music database
   \return true on success
   */
  static bool GetAlbumsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                   const std::string& baseDir,
                                   CVariant& result,
                                   int& total,
                                   const SortDescription& sortDescription,
                                   CMusicDatabase& db);

  /*! \brief Retrieve songs matching criteria and return as JSON-RPC result.
   \param fields set of requested JSON field names
   \param baseDir the base directory URL with filter options
   \param result [out] the JSON-RPC result
   \param total [out] total number of matching songs (before limits)
   \param sortDescription the sort description
   \param db the music database
   \return true on success
   */
  static bool GetSongsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                  const std::string& baseDir,
                                  CVariant& result,
                                  int& total,
                                  const SortDescription& sortDescription,
                                  CMusicDatabase& db);
};
