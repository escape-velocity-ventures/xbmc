/*
 *  Copyright (C) 2005-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

class CAlbum;
class CFileItem;
class CFileItemList;
class CMusicDatabase;

/*!
 \ingroup music
 \brief Service class encapsulating playlist and playback statistics operations.

 CMusicPlaylistService provides access to top-played, recently-played,
 and recently-added content, as well as play count tracking. It delegates
 database access to its owning CMusicDatabase instance via friend access.

 \sa CMusicDatabase
 */
class CMusicPlaylistService
{
public:
  explicit CMusicPlaylistService(CMusicDatabase& db);
  ~CMusicPlaylistService() = default;

  /////////////////////////////////////////////////
  // Top 100
  /////////////////////////////////////////////////

  /*! \brief Get the top 100 most played songs
   \param strBaseDir the base directory for building paths
   \param items [out] the resulting song items
   \return true on success, false on failure
   */
  bool GetTop100(const std::string& strBaseDir, CFileItemList& items);

  /*! \brief Get the top 100 most played albums
   \param albums [out] the resulting albums
   \return true on success, false on failure
   */
  bool GetTop100Albums(std::vector<CAlbum>& albums);

  /*! \brief Get songs from the top 100 most played albums
   \param strBaseDir the base directory for building paths
   \param items [out] the resulting song items
   \return true on success, false on failure
   */
  bool GetTop100AlbumSongs(const std::string& strBaseDir, CFileItemList& items);

  /////////////////////////////////////////////////
  // Recently added
  /////////////////////////////////////////////////

  /*! \brief Get recently added albums
   \param albums [out] the resulting albums
   \param limit maximum number of albums (0 = use settings default)
   \return true on success, false on failure
   */
  bool GetRecentlyAddedAlbums(std::vector<CAlbum>& albums, unsigned int limit = 0);

  /*! \brief Get songs from recently added albums
   \param strBaseDir the base directory for building paths
   \param items [out] the resulting song items
   \param limit maximum number of albums (0 = use settings default)
   \return true on success, false on failure
   */
  bool GetRecentlyAddedAlbumSongs(const std::string& strBaseDir,
                                  CFileItemList& items,
                                  unsigned int limit = 0);

  /////////////////////////////////////////////////
  // Recently played
  /////////////////////////////////////////////////

  /*! \brief Get recently played albums
   \param albums [out] the resulting albums
   \return true on success, false on failure
   */
  bool GetRecentlyPlayedAlbums(std::vector<CAlbum>& albums);

  /*! \brief Get songs from recently played albums
   \param strBaseDir the base directory for building paths
   \param items [out] the resulting song items
   \return true on success, false on failure
   */
  bool GetRecentlyPlayedAlbumSongs(const std::string& strBaseDir, CFileItemList& items);

  /////////////////////////////////////////////////
  // Play count
  /////////////////////////////////////////////////

  /*! \brief Increment the playcount of an item
   Increments the playcount and updates the last played date
   \param item CFileItem to increment the playcount for
   */
  void IncrementPlayCount(const CFileItem& item);

private:
  CMusicDatabase& m_db;
};
