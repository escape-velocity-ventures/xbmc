/*
 *  Copyright (C) 2005-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CFileItemList;
class CMusicDatabase;

/*!
 \ingroup music
 \brief Service class encapsulating music search operations.

 CMusicSearchService provides search functionality across artists,
 albums, and songs in the music database. It delegates database
 access to its owning CMusicDatabase instance via friend access.

 \sa CMusicDatabase
 */
class CMusicSearchService
{
public:
  explicit CMusicSearchService(CMusicDatabase& db);
  ~CMusicSearchService() = default;

  /*! \brief Search for artists, albums, and songs matching the given term
   \param search the search term
   \param items [out] the results
   \return true on success, false on failure
   */
  bool Search(const std::string& search, CFileItemList& items);

  /*! \brief Search for songs matching the given term
   \param search the search term
   \param items [out] the matching songs
   \return true on success, false on failure
   */
  bool SearchSongs(const std::string& search, CFileItemList& items);

  /*! \brief Search for artists matching the given term
   \param search the search term
   \param artists [out] the matching artists
   \return true on success, false on failure
   */
  bool SearchArtists(const std::string& search, CFileItemList& artists);

  /*! \brief Search for albums matching the given term
   \param search the search term
   \param albums [out] the matching albums
   \return true on success, false on failure
   */
  bool SearchAlbums(const std::string& search, CFileItemList& albums);

  /*! \brief Search for albums by a specific artist name
   \param strArtist the artist name to match
   \param items [out] the matching albums
   \return true on success, false on failure
   */
  bool SearchAlbumsByArtistName(const std::string& strArtist, CFileItemList& items);

private:
  CMusicDatabase& m_db;
};
