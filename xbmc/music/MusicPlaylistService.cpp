/*
 *  Copyright (C) 2005-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicPlaylistService.h"

#include "Album.h"
#include "Artist.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "MusicDatabase.h"
#include "ServiceBroker.h"
#include "XBDateTime.h"
#include "dbwrappers/dataset.h"
#include "music/MusicDbUrl.h"
#include "music/tags/MusicInfoTag.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <chrono>
#include <string>
#include <vector>

constexpr unsigned int RECENTLY_PLAYED_LIMIT_PLAYLIST = 25;

CMusicPlaylistService::CMusicPlaylistService(CMusicDatabase& db) : m_db(db)
{
}

bool CMusicPlaylistService::GetTop100(const std::string& strBaseDir, CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    CMusicDbUrl baseUrl;
    if (!strBaseDir.empty() && !baseUrl.FromString(strBaseDir))
      return false;

    std::string strSQL = "SELECT * FROM songview "
                         "WHERE iTimesPlayed>0 "
                         "ORDER BY iTimesPlayed DESC "
                         "LIMIT 100";

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }
    items.Reserve(iRowsFound);
    while (!m_db.m_pDS->eof())
    {
      auto item{std::make_shared<CFileItem>()};
      m_db.GetFileItemFromDataset(item.get(), baseUrl);
      items.Add(std::move(item));
      m_db.m_pDS->next();
    }

    m_db.m_pDS->close(); // cleanup recordset data
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicPlaylistService::GetTop100Albums(std::vector<CAlbum>& albums)
{
  try
  {
    albums.erase(albums.begin(), albums.end());
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    // Get data from album and album_artist tables to fully populate albums
    std::string strSQL = "SELECT albumview.*, albumartistview.* FROM albumview "
                         "JOIN albumartistview ON albumview.idAlbum = albumartistview.idAlbum "
                         "WHERE albumartistview.idAlbum IN "
                         "(SELECT albumview.idAlbum FROM albumview "
                         "WHERE albumview.strAlbum != '' AND albumview.iTimesPlayed>0 "
                         "ORDER BY albumview.iTimesPlayed DESC LIMIT 100) "
                         "ORDER BY albumview.iTimesPlayed DESC, albumartistview.iOrder";

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    int albumArtistOffset = album_enumCount;
    int albumId = -1;
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      if (albumId != record->at(album_idAlbum).get_asInt())
      { // New album
        albumId = record->at(album_idAlbum).get_asInt();
        albums.push_back(m_db.GetAlbumFromDataset(record));
      }
      // Get album artists
      albums.back().artistCredits.push_back(
          m_db.GetArtistCreditFromDataset(record, albumArtistOffset));

      m_db.m_pDS->next();
    }

    m_db.m_pDS->close(); // cleanup recordset data
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicPlaylistService::GetTop100AlbumSongs(const std::string& strBaseDir,
                                                CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    CMusicDbUrl baseUrl;
    if (!strBaseDir.empty() && baseUrl.FromString(strBaseDir))
      return false;

    std::string strSQL = StringUtils::Format(
        "SELECT songview.*, albumview.* FROM songview"
        "JOIN albumview ON (songview.idAlbum = albumview.idAlbum) "
        "JOIN (SELECT song.idAlbum, SUM(song.iTimesPlayed) AS iTimesPlayedSum FROM song "
        "WHERE song.iTimesPlayed > 0 "
        "GROUP BY idAlbum "
        "ORDER BY iTimesPlayedSum DESC LIMIT 100) AS _albumlimit "
        "ON (songview.idAlbum = _albumlimit.idAlbum) "
        "ORDER BY _albumlimit.iTimesPlayedSum DESC");
    CLog::Log(LOGDEBUG, "GetTop100AlbumSongs() query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    items.Reserve(iRowsFound);
    while (!m_db.m_pDS->eof())
    {
      auto item{std::make_shared<CFileItem>()};
      m_db.GetFileItemFromDataset(item.get(), baseUrl);
      items.Add(std::move(item));
      m_db.m_pDS->next();
    }

    // cleanup
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicPlaylistService::GetRecentlyPlayedAlbums(std::vector<CAlbum>& albums)
{
  try
  {
    albums.erase(albums.begin(), albums.end());
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    auto start = std::chrono::steady_clock::now();

    // Get data from album and album_artist tables to fully populate albums
    std::string strSQL = m_db.PrepareSQL(
        "SELECT albumview.*, albumartistview.* "
        "FROM (SELECT idAlbum FROM albumview WHERE albumview.lastplayed IS NOT NULL "
        "AND albumview.strReleaseType = '%s' "
        "ORDER BY albumview.lastplayed DESC LIMIT %u) as playedalbums "
        "JOIN albumview ON albumview.idAlbum = playedalbums.idAlbum "
        "JOIN albumartistview ON albumview.idAlbum = albumartistview.idAlbum "
        "ORDER BY albumview.lastplayed DESC, albumartistview.iorder ",
        CAlbum::ReleaseTypeToString(ReleaseType::Album).c_str(), RECENTLY_PLAYED_LIMIT_PLAYLIST);

    auto queryStart = std::chrono::steady_clock::now();
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;

    auto queryEnd = std::chrono::steady_clock::now();
    auto queryDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - queryStart);

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    int albumArtistOffset = album_enumCount;
    int albumId = -1;
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      if (albumId != record->at(album_idAlbum).get_asInt())
      { // New album
        albumId = record->at(album_idAlbum).get_asInt();
        albums.push_back(m_db.GetAlbumFromDataset(record));
      }
      // Get album artists
      albums.back().artistCredits.push_back(
          m_db.GetArtistCreditFromDataset(record, albumArtistOffset));

      m_db.m_pDS->next();
    }
    m_db.m_pDS->close(); // cleanup recordset data

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "Time to fill list with albums {}ms query took {}ms", duration.count(),
               queryDuration.count());

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicPlaylistService::GetRecentlyPlayedAlbumSongs(const std::string& strBaseDir,
                                                        CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    CMusicDbUrl baseUrl;
    if (!strBaseDir.empty() && !baseUrl.FromString(strBaseDir))
      return false;

    std::string strSQL = m_db.PrepareSQL(
        "SELECT songview.*, songartistview.* "
        "FROM  (SELECT idAlbum, lastPlayed FROM albumview "
        "WHERE albumview.lastplayed IS NOT NULL "
        "ORDER BY albumview.lastplayed DESC LIMIT %u) as playedalbums "
        "JOIN songview ON songview.idAlbum = playedalbums.idAlbum "
        "JOIN songartistview ON songview.idSong = songartistview.idSong "
        "ORDER BY playedalbums.lastplayed DESC, "
        "songartistview.idsong, songartistview.idRole, songartistview.iOrder",
        CServiceBroker::GetSettingsComponent()
            ->GetAdvancedSettings()
            ->m_iMusicLibraryRecentlyAddedItems);
    CLog::Log(LOGDEBUG, "GetRecentlyPlayedAlbumSongs() query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    // Needs a separate query to determine number of songs to set items size.
    // Get songs from returned rows. Join means there is a row for every song artist
    // Gather artist credits, rather than append to item as go along, so can return array of artistIDs too
    int songArtistOffset = song_enumCount;
    int songId = -1;
    std::vector<CArtistCredit> artistCredits;
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      int idSongArtistRole =
          record->at(songArtistOffset + artistCredit_idRole).get_asInt();
      if (songId != record->at(song_idSong).get_asInt())
      { //New song
        if (songId > 0 && !artistCredits.empty())
        {
          //Store artist credits for previous song
          m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
          artistCredits.clear();
        }
        songId = record->at(song_idSong).get_asInt();
        auto item{std::make_shared<CFileItem>()};
        m_db.GetFileItemFromDataset(record, item.get(), baseUrl);
        items.Add(std::move(item));
      }
      // Get song artist credits and contributors
      if (idSongArtistRole == ROLE_ARTIST)
        artistCredits.push_back(
            m_db.GetArtistCreditFromDataset(record, songArtistOffset));
      else
        items[items.Size() - 1]->GetMusicInfoTag()->AppendArtistRole(
            m_db.GetArtistRoleFromDataset(record, songArtistOffset));

      m_db.m_pDS->next();
    }
    if (!artistCredits.empty())
    {
      //Store artist credits for final song
      m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
      artistCredits.clear();
    }

    // cleanup
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicPlaylistService::GetRecentlyAddedAlbums(std::vector<CAlbum>& albums,
                                                   unsigned int limit)
{
  try
  {
    albums.erase(albums.begin(), albums.end());
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    // Get data from album and album_artist tables to fully populate albums
    // Determine the recently added albums from dateAdded (usually derived from music file
    // timestamps, nothing to do with when albums added to library)
    std::string strSQL = m_db.PrepareSQL(
        "SELECT albumview.*, albumartistview.* "
        "FROM (SELECT idAlbum FROM album WHERE strAlbum != '' "
        "ORDER BY dateAdded DESC LIMIT %u) AS recentalbums "
        "JOIN albumview ON albumview.idAlbum = recentalbums.idAlbum "
        "JOIN albumartistview ON albumview.idAlbum = albumartistview.idAlbum "
        "ORDER BY dateAdded DESC, albumview.idAlbum desc, albumartistview.iOrder ",
        limit ? limit
              : CServiceBroker::GetSettingsComponent()
                    ->GetAdvancedSettings()
                    ->m_iMusicLibraryRecentlyAddedItems);

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    int albumArtistOffset = album_enumCount;
    int albumId = -1;
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      if (albumId != record->at(album_idAlbum).get_asInt())
      { // New album
        albumId = record->at(album_idAlbum).get_asInt();
        albums.push_back(m_db.GetAlbumFromDataset(record));
      }
      // Get album artists
      albums.back().artistCredits.push_back(
          m_db.GetArtistCreditFromDataset(record, albumArtistOffset));

      m_db.m_pDS->next();
    }
    m_db.m_pDS->close(); // cleanup recordset data
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicPlaylistService::GetRecentlyAddedAlbumSongs(const std::string& strBaseDir,
                                                       CFileItemList& items,
                                                       unsigned int limit)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    CMusicDbUrl baseUrl;
    if (!strBaseDir.empty() && !baseUrl.FromString(strBaseDir))
      return false;

    // Get data from song and song_artist tables to fully populate songs
    // Determine the recently added albums from dateAdded (usually derived from music file
    // timestamps, nothing to do with when albums added to library)
    std::string strSQL;
    strSQL = m_db.PrepareSQL(
        "SELECT songview.*, songartistview.* "
        "FROM (SELECT idAlbum, dateAdded FROM album "
        "ORDER BY dateAdded DESC LIMIT %u) AS recentalbums "
        "JOIN songview ON songview.idAlbum = recentalbums.idAlbum "
        "JOIN songartistview ON songview.idSong = songartistview.idSong "
        "ORDER BY recentalbums.dateAdded DESC, songview.idAlbum DESC, "
        "songview.idSong, songartistview.idRole, songartistview.iOrder ",
        limit ? limit
              : CServiceBroker::GetSettingsComponent()
                    ->GetAdvancedSettings()
                    ->m_iMusicLibraryRecentlyAddedItems);
    CLog::Log(LOGDEBUG, "GetRecentlyAddedAlbumSongs() query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    // Needs a separate query to determine number of songs to set items size.
    // Get songs from returned rows. Join means there is a row for every song artist
    int songArtistOffset = song_enumCount;
    int songId = -1;
    std::vector<CArtistCredit> artistCredits;
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      int idSongArtistRole =
          record->at(songArtistOffset + artistCredit_idRole).get_asInt();
      if (songId != record->at(song_idSong).get_asInt())
      { //New song
        if (songId > 0 && !artistCredits.empty())
        {
          //Store artist credits for previous song
          m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
          artistCredits.clear();
        }
        songId = record->at(song_idSong).get_asInt();
        auto item{std::make_shared<CFileItem>()};
        m_db.GetFileItemFromDataset(record, item.get(), baseUrl);
        items.Add(std::move(item));
      }
      // Get song artist credits and contributors
      if (idSongArtistRole == ROLE_ARTIST)
        artistCredits.push_back(
            m_db.GetArtistCreditFromDataset(record, songArtistOffset));
      else
        items[items.Size() - 1]->GetMusicInfoTag()->AppendArtistRole(
            m_db.GetArtistRoleFromDataset(record, songArtistOffset));

      m_db.m_pDS->next();
    }
    if (!artistCredits.empty())
    {
      //Store artist credits for final song
      m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
      artistCredits.clear();
    }

    // cleanup
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

void CMusicPlaylistService::IncrementPlayCount(const CFileItem& item)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return;
    if (nullptr == m_db.m_pDS)
      return;

    int idSong = m_db.GetSongIDFromPath(item.GetPath());
    std::string strDateNow = CDateTime::GetCurrentDateTime().GetAsDBDateTime();
    std::string sql =
        m_db.PrepareSQL("UPDATE song SET iTimesPlayed = iTimesPlayed+1, lastplayed ='%s' "
                        "WHERE idSong=%i",
                        strDateNow.c_str(), idSong);
    m_db.m_pDS->exec(sql);
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", item.GetPath());
  }
}
