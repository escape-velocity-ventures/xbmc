/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/SortUtils.h"

#include <string>

class CFileItemList;
class CMusicDatabase;
class CMusicDbUrl;
class Filter;

/*!
 \ingroup music
 \brief Handles navigation and filtered query operations for the music database.

 CMusicNavRepository encapsulates all navigation-related query methods that were
 previously part of CMusicDatabase. These methods handle filtered browsing of
 genres, sources, years, roles, artists, albums, discs, and songs.

 This class is a friend of CMusicDatabase and accesses its internal dataset
 pointers (m_pDB, m_pDS) and helper methods directly.
 */
class CMusicNavRepository
{
public:
  explicit CMusicNavRepository(CMusicDatabase& db);
  ~CMusicNavRepository() = default;

  // Non-copyable, non-movable (holds reference to database)
  CMusicNavRepository(const CMusicNavRepository&) = delete;
  CMusicNavRepository& operator=(const CMusicNavRepository&) = delete;
  CMusicNavRepository(CMusicNavRepository&&) = delete;
  CMusicNavRepository& operator=(CMusicNavRepository&&) = delete;

  /////////////////////////////////////////////////
  // Navigation queries
  /////////////////////////////////////////////////
  bool GetGenresNav(const std::string& strBaseDir,
                    CFileItemList& items,
                    const Filter& filter,
                    bool countOnly);
  bool GetSourcesNav(const std::string& strBaseDir,
                     CFileItemList& items,
                     const Filter& filter,
                     bool countOnly);
  bool GetYearsNav(const std::string& strBaseDir,
                   CFileItemList& items,
                   const Filter& filter);
  bool GetRolesNav(const std::string& strBaseDir,
                   CFileItemList& items,
                   const Filter& filter);
  bool GetArtistsNav(const std::string& strBaseDir,
                     CFileItemList& items,
                     const SortDescription& sortDescription,
                     bool albumArtistsOnly,
                     int idGenre,
                     int idAlbum,
                     int idSong,
                     const Filter& filter,
                     bool countOnly);
  bool GetCommonNav(const std::string& strBaseDir,
                    const std::string& table,
                    const std::string& labelField,
                    CFileItemList& items,
                    const Filter& filter,
                    bool countOnly);
  bool GetAlbumTypesNav(const std::string& strBaseDir,
                        CFileItemList& items,
                        const Filter& filter,
                        bool countOnly);
  bool GetMusicLabelsNav(const std::string& strBaseDir,
                         CFileItemList& items,
                         const Filter& filter,
                         bool countOnly);
  bool GetAlbumsNav(const std::string& strBaseDir,
                    CFileItemList& items,
                    const SortDescription& sortDescription,
                    int idGenre,
                    int idArtist,
                    const Filter& filter,
                    bool countOnly);
  bool GetDiscsNav(const std::string& strBaseDir,
                   CFileItemList& items,
                   const SortDescription& sortDescription,
                   int idAlbum,
                   const Filter& filter,
                   bool countOnly);
  bool GetAlbumsByYear(const std::string& strBaseDir, CFileItemList& items, int year);
  bool GetSongsNav(const std::string& strBaseDir,
                   CFileItemList& items,
                   const SortDescription& sortDescription,
                   int idGenre,
                   int idArtist,
                   int idAlbum);
  bool GetSongsByYear(const std::string& baseDir, CFileItemList& items, int year);

  /////////////////////////////////////////////////
  // Filtered queries (ByWhere)
  /////////////////////////////////////////////////
  bool GetSongsFullByWhere(const std::string& baseDir,
                           CFileItemList& items,
                           const SortDescription& sortDescription,
                           const Filter& filter,
                           bool artistData);
  bool GetAlbumsByWhere(const std::string& baseDir,
                        CFileItemList& items,
                        const SortDescription& sortDescription,
                        const Filter& filter,
                        bool countOnly);
  bool GetDiscsByWhere(const std::string& baseDir,
                       CFileItemList& items,
                       const SortDescription& sortDescription,
                       const Filter& filter,
                       bool countOnly);
  bool GetDiscsByWhere(CMusicDbUrl& musicUrl,
                       CFileItemList& items,
                       const SortDescription& sortDescription,
                       const Filter& filter,
                       bool countOnly);
  bool GetArtistsByWhere(const std::string& strBaseDir,
                         CFileItemList& items,
                         const SortDescription& sortDescription,
                         const Filter& filter,
                         bool countOnly);

  /////////////////////////////////////////////////
  // Count queries
  /////////////////////////////////////////////////
  int GetDiscsCount(const std::string& baseDir, const Filter& filter);
  int GetSongsCount(const Filter& filter);

private:
  CMusicDatabase& m_db;
};
