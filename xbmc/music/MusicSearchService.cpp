/*
 *  Copyright (C) 2005-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicSearchService.h"

#include "Album.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "MusicDatabase.h"
#include "ServiceBroker.h"
#include "dbwrappers/dataset.h"
#include "music/MusicDbUrl.h"
#include "music/tags/MusicInfoTag.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <chrono>
#include <string>

constexpr size_t MIN_FULL_SEARCH_LENGTH_SEARCH = 3;

CMusicSearchService::CMusicSearchService(CMusicDatabase& db) : m_db(db)
{
}

bool CMusicSearchService::Search(const std::string& search, CFileItemList& items)
{
  auto start = std::chrono::steady_clock::now();
  // first grab all the artists that match
  SearchArtists(search, items);
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Artist search in {} ms", duration.count());

  start = std::chrono::steady_clock::now();
  // then albums that match
  SearchAlbums(search, items);
  end = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Album search in {} ms", duration.count());

  start = std::chrono::steady_clock::now();
  // and finally songs
  SearchSongs(search, items);
  end = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Songs search in {} ms", duration.count());

  return true;
}

bool CMusicSearchService::SearchArtists(const std::string& search, CFileItemList& artists)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strVariousArtists =
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(340).c_str();
    std::string strSQL;
    if (search.size() >= MIN_FULL_SEARCH_LENGTH_SEARCH)
      strSQL = m_db.PrepareSQL("SELECT * FROM artist "
                               "WHERE (strArtist LIKE '%s%%' OR strArtist LIKE '%% %s%%') "
                               "AND strArtist <> '%s' ",
                               search.c_str(), search.c_str(), strVariousArtists.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT * FROM artist "
                               "WHERE strArtist LIKE '%s%%' AND strArtist <> '%s' ",
                               search.c_str(), strVariousArtists.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    const std::string& artistLabel(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(557)); // Artist
    while (!m_db.m_pDS->eof())
    {
      std::string path =
          StringUtils::Format("musicdb://artists/{}/", m_db.m_pDS->fv(0).get_asInt());
      auto pItem{std::make_shared<CFileItem>(path, true)};
      std::string label =
          StringUtils::Format("[{}] {}", artistLabel, m_db.m_pDS->fv(1).get_asString());
      pItem->SetLabel(label);
      // sort label is stored in the title tag
      label = StringUtils::Format("A {}", m_db.m_pDS->fv(1).get_asString());
      pItem->GetMusicInfoTag()->SetTitle(label);
      pItem->GetMusicInfoTag()->SetDatabaseId(m_db.m_pDS->fv(0).get_asInt(), MediaTypeArtist);
      artists.Add(std::move(pItem));
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

bool CMusicSearchService::SearchSongs(const std::string& search, CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    CMusicDbUrl baseUrl;
    if (!baseUrl.FromString("musicdb://songs/"))
      return false;

    std::string strSQL;
    if (search.size() >= MIN_FULL_SEARCH_LENGTH_SEARCH)
      strSQL = m_db.PrepareSQL("SELECT * FROM songview "
                               "WHERE strTitle LIKE '%s%%' or strTitle LIKE '%% %s%%' LIMIT 1000",
                               search.c_str(), search.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT * FROM songview "
                               "WHERE strTitle LIKE '%s%%' LIMIT 1000",
                               search.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
      return false;

    while (!m_db.m_pDS->eof())
    {
      auto item{std::make_shared<CFileItem>()};
      m_db.GetFileItemFromDataset(item.get(), baseUrl);
      items.Add(std::move(item));
      m_db.m_pDS->next();
    }

    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicSearchService::SearchAlbums(const std::string& search, CFileItemList& albums)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL;
    if (search.size() >= MIN_FULL_SEARCH_LENGTH_SEARCH)
      strSQL = m_db.PrepareSQL("SELECT * FROM albumview "
                               "WHERE strAlbum LIKE '%s%%' OR strAlbum LIKE '%% %s%%'",
                               search.c_str(), search.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT * FROM albumview "
                               "WHERE strAlbum LIKE '%s%%'",
                               search.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;

    const std::string& albumLabel(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(558)); // Album
    while (!m_db.m_pDS->eof())
    {
      CAlbum album = m_db.GetAlbumFromDataset(m_db.m_pDS.get());
      std::string path = StringUtils::Format("musicdb://albums/{}/", album.idAlbum);
      auto pItem{std::make_shared<CFileItem>(path, album)};
      std::string label = StringUtils::Format("[{}] {}", albumLabel, album.strAlbum);
      pItem->SetLabel(label);
      // sort label is stored in the title tag
      label = StringUtils::Format("B {}", album.strAlbum);
      pItem->GetMusicInfoTag()->SetTitle(label);
      albums.Add(std::move(pItem));
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

bool CMusicSearchService::SearchAlbumsByArtistName(const std::string& strArtist,
                                                   CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT albumview.* FROM albumview "
                             "JOIN album_artist ON album_artist.idAlbum = albumview.idAlbum "
                             "WHERE  album_artist.strArtist LIKE '%s'",
                             strArtist.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;

    while (!m_db.m_pDS->eof())
    {
      CAlbum album = m_db.GetAlbumFromDataset(m_db.m_pDS.get());
      std::string path = StringUtils::Format("musicdb://albums/{}/", album.idAlbum);
      auto pItem{std::make_shared<CFileItem>(path, album)};
      std::string label =
          StringUtils::Format("{} ({})", album.strAlbum, pItem->GetMusicInfoTag()->GetYear());
      pItem->SetLabel(label);
      items.Add(std::move(pItem));
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
