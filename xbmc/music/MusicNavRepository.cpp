/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicNavRepository.h"

#include "MusicDatabase.h"

#include "Album.h"
#include "Artist.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "dbwrappers/dataset.h"
#include "music/MusicDbUrl.h"
#include "music/tags/MusicInfoTag.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/DatabaseUtils.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

CMusicNavRepository::CMusicNavRepository(CMusicDatabase& db) : m_db(db)
{
}

bool CMusicNavRepository::GetGenresNav(const std::string& strBaseDir,
                                       CFileItemList& items,
                                       const Filter& filter /* = Filter() */,
                                       bool countOnly /* = false */)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    // get primary genres for songs - could be simplified to just SELECT * FROM genre?
    std::string strSQL = "SELECT %s FROM genre ";

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting;
    if (!musicUrl.FromString(strBaseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    // if there are extra WHERE conditions we might need access
    // to songview or albumview for these conditions
    if (!extFilter.where.empty())
    {
      if (extFilter.where.find("artistview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN song_genre ON song_genre.idGenre = genre.idGenre");
        extFilter.AppendJoin("JOIN songview ON songview.idSong = song_genre.idSong");
        extFilter.AppendJoin("JOIN song_artist ON song_artist.idSong = songview.idSong");
        extFilter.AppendJoin("JOIN artistview ON artistview.idArtist = song_artist.idArtist");
      }
      else if (extFilter.where.find("songview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN song_genre ON song_genre.idGenre = genre.idGenre");
        extFilter.AppendJoin("JOIN songview ON songview.idSong = song_genre.idSong");
      }
      else if (extFilter.where.find("albumview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN song_genre ON song_genre.idGenre = genre.idGenre");
        extFilter.AppendJoin("JOIN song ON song.idSong = song_genre.idSong");
        extFilter.AppendJoin("JOIN albumview ON albumview.idAlbum = song.idAlbum");
      }
      extFilter.AppendGroup("genre.idGenre");
    }
    extFilter.AppendWhere("genre.strGenre != ''");

    if (countOnly)
    {
      extFilter.fields = "COUNT(DISTINCT genre.idGenre)";
      extFilter.group.clear();
      extFilter.order.clear();
    }

    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    strSQL = m_db.PrepareSQL(strSQL,
                             !extFilter.fields.empty() && extFilter.fields.compare("*") != 0
                                 ? extFilter.fields.c_str()
                                 : "genre.*") +
             strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    if (countOnly)
    {
      auto pItem{std::make_shared<CFileItem>()};
      pItem->SetProperty("total",
                         iRowsFound == 1 ? m_db.m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(std::move(pItem));

      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    while (!m_db.m_pDS->eof())
    {
      auto pItem{
          std::make_shared<CFileItem>(m_db.m_pDS->fv("genre.strGenre").get_asString())};
      pItem->GetMusicInfoTag()->SetGenre(m_db.m_pDS->fv("genre.strGenre").get_asString());
      pItem->GetMusicInfoTag()->SetDatabaseId(m_db.m_pDS->fv("genre.idGenre").get_asInt(),
                                              "genre");

      CMusicDbUrl itemUrl = musicUrl;
      std::string strDir =
          StringUtils::Format("{}/", m_db.m_pDS->fv("genre.idGenre").get_asInt());
      itemUrl.AppendPath(strDir);
      pItem->SetPath(itemUrl.ToString());

      pItem->SetFolder(true);
      items.Add(std::move(pItem));

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

bool CMusicNavRepository::GetSourcesNav(const std::string& strBaseDir,
                                        CFileItemList& items,
                                        const Filter& filter /*= Filter()*/,
                                        bool countOnly /*= false*/)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    // Get sources for selection list when add/edit filter or smartplaylist rule
    std::string strSQL = "SELECT %s FROM source ";

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting;
    if (!musicUrl.FromString(strBaseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    // if there are extra WHERE conditions we might need access
    // to songview or albumview for these conditions
    if (!extFilter.where.empty())
    {
      if (extFilter.where.find("artistview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN album_source ON album_source.idSource = source.idSource");
        extFilter.AppendJoin(
            "JOIN album_artist ON album_artist.idAlbum = album_source.idAlbum");
        extFilter.AppendJoin(
            "JOIN artistview ON artistview.idArtist = album_artist.idArtist");
      }
      else if (extFilter.where.find("songview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN album_source ON album_source.idSource = source.idSource");
        extFilter.AppendJoin("JOIN songview ON songview.idAlbum = album_source .idAlbum");
      }
      else if (extFilter.where.find("albumview") != std::string::npos)
      {
        extFilter.AppendJoin("JOIN album_source ON album_source.idSource = source.idSource");
        extFilter.AppendJoin("JOIN albumview ON albumview.idAlbum = album_source .idAlbum");
      }
      extFilter.AppendGroup("source.idSource");
    }
    else
    { // Get only sources that have been scanned into music library
      extFilter.AppendJoin("JOIN album_source ON album_source.idSource = source.idSource");
      extFilter.AppendGroup("source.idSource");
    }

    if (countOnly)
    {
      extFilter.fields = "COUNT(DISTINCT source.idSource)";
      extFilter.group.clear();
      extFilter.order.clear();
    }

    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    strSQL = m_db.PrepareSQL(strSQL,
                             !extFilter.fields.empty() && extFilter.fields.compare("*") != 0
                                 ? extFilter.fields.c_str()
                                 : "source.*") +
             strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    if (countOnly)
    {
      auto pItem{std::make_shared<CFileItem>()};
      pItem->SetProperty("total",
                         iRowsFound == 1 ? m_db.m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(std::move(pItem));

      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    while (!m_db.m_pDS->eof())
    {
      auto pItem{
          std::make_shared<CFileItem>(m_db.m_pDS->fv("source.strName").get_asString())};
      pItem->GetMusicInfoTag()->SetTitle(m_db.m_pDS->fv("source.strName").get_asString());
      pItem->GetMusicInfoTag()->SetDatabaseId(
          m_db.m_pDS->fv("source.idSource").get_asInt(), "source");

      CMusicDbUrl itemUrl = musicUrl;
      std::string strDir =
          StringUtils::Format("{}/", m_db.m_pDS->fv("source.idSource").get_asInt());
      itemUrl.AppendPath(strDir);
      itemUrl.AddOption("sourceid", m_db.m_pDS->fv("source.idSource").get_asInt());
      pItem->SetPath(itemUrl.ToString());

      pItem->SetFolder(true);
      items.Add(std::move(pItem));

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

bool CMusicNavRepository::GetYearsNav(const std::string& strBaseDir,
                                      CFileItemList& items,
                                      const Filter& filter /* = Filter() */)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting;
    std::string strSQL;
    if (!musicUrl.FromString(strBaseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    bool useOriginalYears = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
        CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE);

    useOriginalYears =
        useOriginalYears || StringUtils::StartsWith(strBaseDir, "musicdb://originalyears/");

    if (!useOriginalYears)
    { // Get years from year part of release date
      strSQL = "SELECT DISTINCT CAST(strReleaseDate AS INTEGER) AS year FROM albumview ";
      extFilter.AppendWhere("(TRIM(strReleaseDate) <> '' AND strReleaseDate IS NOT NULL)");
    }
    else
    { // Get years from year part of original date
      strSQL = "SELECT DISTINCT CAST(strOrigReleaseDate AS INTEGER) AS year FROM albumview ";
      extFilter.AppendWhere(
          "(TRIM(strOrigReleaseDate) <> '' AND strOrigReleaseDate IS NOT NULL)");
    }
    if (!m_db.BuildSQL(strSQL, extFilter, strSQL))
      return false;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    while (!m_db.m_pDS->eof())
    {
      auto pItem{std::make_shared<CFileItem>(m_db.m_pDS->fv(0).get_asString())};
      pItem->GetMusicInfoTag()->SetYear(m_db.m_pDS->fv(0).get_asInt());
      if (useOriginalYears)
        pItem->GetMusicInfoTag()->SetDatabaseId(-1, "originalyear");
      else
        pItem->GetMusicInfoTag()->SetDatabaseId(-1, "year");

      CMusicDbUrl itemUrl = musicUrl;
      std::string strDir = StringUtils::Format("{}/", m_db.m_pDS->fv(0).get_asInt());
      itemUrl.AppendPath(strDir);
      if (useOriginalYears)
        itemUrl.AddOption("useoriginalyear", true);
      pItem->SetPath(itemUrl.ToString());

      pItem->SetFolder(true);
      items.Add(std::move(pItem));

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

bool CMusicNavRepository::GetRolesNav(const std::string& strBaseDir,
                                      CFileItemList& items,
                                      const Filter& filter /* = Filter() */)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting;
    if (!musicUrl.FromString(strBaseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    // get roles with artists having that role
    std::string strSQL = "SELECT DISTINCT role.idRole, role.strRole FROM role "
                         "JOIN song_artist ON song_artist.idRole = role.idRole ";

    if (!m_db.BuildSQL(strSQL, extFilter, strSQL))
      return false;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    while (!m_db.m_pDS->eof())
    {
      std::string labelValue = m_db.m_pDS->fv("role.strRole").get_asString();
      auto pItem{std::make_shared<CFileItem>(labelValue)};
      pItem->GetMusicInfoTag()->SetTitle(labelValue);
      pItem->GetMusicInfoTag()->SetDatabaseId(m_db.m_pDS->fv("role.idRole").get_asInt(),
                                              "role");
      CMusicDbUrl itemUrl = musicUrl;
      std::string strDir =
          StringUtils::Format("{}/", m_db.m_pDS->fv("role.idRole").get_asInt());
      itemUrl.AppendPath(strDir);
      itemUrl.AddOption("roleid", m_db.m_pDS->fv("role.idRole").get_asInt());
      pItem->SetPath(itemUrl.ToString());

      pItem->SetFolder(true);
      items.Add(std::move(pItem));

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

bool CMusicNavRepository::GetAlbumsByYear(const std::string& strBaseDir,
                                          CFileItemList& items,
                                          int year)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(strBaseDir))
    return false;

  musicUrl.AddOption("year", year);
  musicUrl.AddOption("show_singles", true); // allow singles to be listed

  Filter filter;
  return GetAlbumsByWhere(musicUrl.ToString(), items, SortDescription(), filter, false);
}

bool CMusicNavRepository::GetCommonNav(const std::string& strBaseDir,
                                       const std::string& table,
                                       const std::string& labelField,
                                       CFileItemList& items,
                                       const Filter& filter /* = Filter() */,
                                       bool countOnly /* = false */)
{
  if (nullptr == m_db.m_pDB)
    return false;
  if (nullptr == m_db.m_pDS)
    return false;

  if (table.empty() || labelField.empty())
    return false;

  try
  {
    Filter extFilter = filter;
    std::string strSQL = "SELECT %s FROM " + table + " ";
    extFilter.AppendGroup(labelField);
    extFilter.AppendWhere(labelField + " != ''");

    if (countOnly)
    {
      extFilter.fields = "COUNT(DISTINCT " + labelField + ")";
      extFilter.group.clear();
      extFilter.order.clear();
    }

    // Do prepare before add where as it could contain a LIKE statement with wild card that upsets format
    // e.g. LIKE '%symphony%' would be taken as a %s format argument
    strSQL = m_db.PrepareSQL(
        strSQL, !extFilter.fields.empty() ? extFilter.fields.c_str() : labelField.c_str());

    CMusicDbUrl musicUrl;
    if (!m_db.BuildSQL(strBaseDir, strSQL, extFilter, strSQL, musicUrl))
      return false;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound <= 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    if (countOnly)
    {
      auto pItem{std::make_shared<CFileItem>()};
      pItem->SetProperty("total",
                         iRowsFound == 1 ? m_db.m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(std::move(pItem));

      m_db.m_pDS->close();
      return true;
    }

    // get data from returned rows
    while (!m_db.m_pDS->eof())
    {
      std::string labelValue = m_db.m_pDS->fv(labelField.c_str()).get_asString();
      auto pItem{std::make_shared<CFileItem>(labelValue)};

      CMusicDbUrl itemUrl = musicUrl;
      std::string strDir = StringUtils::Format("{}/", labelValue);
      itemUrl.AppendPath(strDir);
      pItem->SetPath(itemUrl.ToString());

      pItem->SetFolder(true);
      items.Add(std::move(pItem));

      m_db.m_pDS->next();
    }

    // cleanup
    m_db.m_pDS->close();

    return true;
  }
  catch (...)
  {
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }

  return false;
}

bool CMusicNavRepository::GetAlbumTypesNav(const std::string& strBaseDir,
                                           CFileItemList& items,
                                           const Filter& filter /* = Filter() */,
                                           bool countOnly /* = false */)
{
  return GetCommonNav(strBaseDir, "albumview", "albumview.strType", items, filter, countOnly);
}

bool CMusicNavRepository::GetMusicLabelsNav(const std::string& strBaseDir,
                                            CFileItemList& items,
                                            const Filter& filter /* = Filter() */,
                                            bool countOnly /* = false */)
{
  return GetCommonNav(strBaseDir, "albumview", "albumview.strLabel", items, filter, countOnly);
}

bool CMusicNavRepository::GetArtistsNav(const std::string& strBaseDir,
                                        CFileItemList& items,
                                        const SortDescription& sortDescription,
                                        bool albumArtistsOnly /* = false */,
                                        int idGenre /* = -1 */,
                                        int idAlbum /* = -1 */,
                                        int idSong /* = -1 */,
                                        const Filter& filter /* = Filter() */,
                                        bool countOnly /* = false */)
{
  if (nullptr == m_db.m_pDB)
    return false;
  if (nullptr == m_db.m_pDS)
    return false;
  try
  {
    CMusicDbUrl musicUrl;
    if (!musicUrl.FromString(strBaseDir))
      return false;

    if (idGenre > 0)
      musicUrl.AddOption("genreid", idGenre);
    else if (idAlbum > 0)
      musicUrl.AddOption("albumid", idAlbum);
    else if (idSong > 0)
      musicUrl.AddOption("songid", idSong);

    // Override albumArtistsOnly parameter (usually externally set to SETTING_MUSICLIBRARY_SHOWCOMPILATIONARTISTS)
    // when local option already present in music URL thus allowing it to be an option in custom nodes
    if (!musicUrl.HasOption("albumartistsonly"))
      musicUrl.AddOption("albumartistsonly", albumArtistsOnly);

    bool result =
        GetArtistsByWhere(musicUrl.ToString(), items, sortDescription, filter, countOnly);

    return result;
  }
  catch (...)
  {
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicNavRepository::GetArtistsByWhere(const std::string& strBaseDir,
                                            CFileItemList& items,
                                            const SortDescription& sortDescription,
                                            const Filter& filter,
                                            bool countOnly /* = false */)
{
  if (nullptr == m_db.m_pDB)
    return false;
  if (nullptr == m_db.m_pDS)
    return false;

  try
  {
    auto start = std::chrono::steady_clock::now();
    int total = -1;

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting = sortDescription;
    if (!musicUrl.FromString(strBaseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    bool extended = false;
    bool limitedInSQL =
        extFilter.limit.empty() && (sorting.limitStart > 0 || sorting.limitEnd > 0);

    // if there are extra WHERE conditions (from media filter dialog) we might
    // need access to songview or albumview for these conditions
    if (!extFilter.where.empty())
    {
      if (extFilter.where.find("songview") != std::string::npos)
      {
        extended = true;
        extFilter.AppendJoin("JOIN song_artist ON song_artist.idArtist = artistview.idArtist "
                             "JOIN songview ON songview.idSong = song_artist.idSong");
      }
      else if (extFilter.where.find("albumview") != std::string::npos)
      {
        extended = true;
        extFilter.AppendJoin(
            "JOIN album_artist ON album_artist.idArtist = artistview.idArtist "
            "JOIN albumview ON albumview.idAlbum = album_artist.idAlbum");
      }
      if (extended)
        extFilter.AppendGroup(
            "artistview.idArtist"); // Only one row per artist despite joins
    }

    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count number of artists that satisfy selection criteria (no limit built)
    // Count done in full query fetch when unlimited
    if (countOnly || limitedInSQL)
    {
      if (extended)
      {
        // Count distinct without group by
        Filter countFilter = extFilter;
        countFilter.group.clear();
        std::string strSQLWhere;
        if (!m_db.BuildSQL(strSQLWhere, countFilter, strSQLWhere))
          return false;
        total = m_db.GetSingleValueInt(
            "SELECT COUNT(DISTINCT artistview.idArtist) FROM artistview " + strSQLWhere,
            *m_db.m_pDS);
      }
      else
        total = m_db.GetSingleValueInt("SELECT COUNT(1) FROM artistview " + strSQLExtra,
                                       *m_db.m_pDS);
    }
    if (countOnly)
    {
      auto pItem{std::make_shared<CFileItem>()};
      pItem->SetProperty("total", total);
      items.Add(std::move(pItem));

      m_db.m_pDS->close();
      return true;
    }

    // Apply any limiting directly in SQL and so sort as well
    if (limitedInSQL)
    {
      extFilter.limit =
          DatabaseUtils::BuildLimitClauseOnly(sorting.limitEnd, sorting.limitStart);
    }

    // Apply sort in SQL
    const std::shared_ptr<CSettings> settings =
        CServiceBroker::GetSettingsComponent()->GetSettings();
    if (settings->GetBool(CSettings::SETTING_MUSICLIBRARY_USEARTISTSORTNAME))
      sorting.sortAttributes = static_cast<SortAttribute>(sorting.sortAttributes |
                                                          SortAttributeUseArtistSortName);
    // Set Orderby and add any extra fields needed for sort e.g. "artistname" scalar query
    m_db.GetOrderFilter(MediaTypeArtist, sorting, extFilter);

    strSQLExtra.clear();
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    std::string strSQL;
    std::string strFields = "artistview.*";
    if (!extFilter.fields.empty() && extFilter.fields.compare("*") != 0)
      strFields = "artistview.*, " + extFilter.fields;
    strSQL = "SELECT " + strFields + " FROM artistview " + strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    auto queryStart = std::chrono::steady_clock::now();
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    auto queryEnd = std::chrono::steady_clock::now();
    auto queryDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - queryStart);

    // Store the total number of artists as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);

    DatabaseResults results;
    results.reserve(iRowsFound);
    // Populate results field vector from dataset
    FieldList fields;
    if (!DatabaseUtils::GetDatabaseResults(MediaTypeArtist, fields, *m_db.m_pDS, results))
      return false;
    // Store item list sort order
    items.SetSortMethod(sortDescription.sortBy);
    items.SetSortOrder(sortDescription.sortOrder);

    // Get Artists from returned rows
    items.Reserve(results.size());
    const dbiplus::query_data& data = m_db.m_pDS->get_result_set().records;
    for (const auto& i : results)
    {
      const auto targetRow = static_cast<unsigned int>(i.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(targetRow);

      try
      {
        CArtist artist = m_db.GetArtistFromDataset(record, false);
        auto pItem{std::make_shared<CFileItem>(artist)};

        CMusicDbUrl itemUrl = musicUrl;
        std::string path = StringUtils::Format("{}/", artist.idArtist);
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->GetMusicInfoTag()->SetDatabaseId(artist.idArtist, MediaTypeArtist);
        // Set icon now to avoid slow per item processing in FillInDefaultIcon later
        pItem->SetProperty("icon_never_overlay", true);
        pItem->SetArt("icon", "DefaultArtist.png");

        CMusicDatabase::SetPropertiesFromArtist(*pItem, artist);
        items.Add(std::move(pItem));
      }
      catch (...)
      {
        m_db.m_pDS->close();
        CLog::LogF(LOGERROR, "out of memory getting listing (got {})", items.Size());
      }
    }
    // cleanup
    m_db.m_pDS->close();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "Time to fill list with artists {} ms query took {} ms",
               duration.count(), queryDuration.count());

    return true;
  }
  catch (...)
  {
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicNavRepository::GetAlbumsNav(const std::string& strBaseDir,
                                       CFileItemList& items,
                                       const SortDescription& sortDescription,
                                       int idGenre /* = -1 */,
                                       int idArtist /* = -1 */,
                                       const Filter& filter /* = Filter() */,
                                       bool countOnly /* = false */)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(strBaseDir))
    return false;

  // where clause
  if (idGenre > 0)
    musicUrl.AddOption("genreid", idGenre);

  if (idArtist > 0)
    musicUrl.AddOption("artistid", idArtist);

  return GetAlbumsByWhere(musicUrl.ToString(), items, sortDescription, filter, countOnly);
}

bool CMusicNavRepository::GetAlbumsByWhere(const std::string& baseDir,
                                           CFileItemList& items,
                                           const SortDescription& sortDescription,
                                           const Filter& filter,
                                           bool countOnly /* = false */)
{
  if (m_db.m_pDB == nullptr || m_db.m_pDS == nullptr)
    return false;

  try
  {
    auto start = std::chrono::steady_clock::now();
    int total = -1;

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting = sortDescription;
    if (!musicUrl.FromString(baseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    bool extended = false;
    bool limitedInSQL =
        extFilter.limit.empty() && (sorting.limitStart > 0 || sorting.limitEnd > 0);

    // If there are extra WHERE conditions (from media filter dialog) we might
    // need access to songview for these conditions
    if (extFilter.where.find("songview") != std::string::npos)
    {
      extended = true;
      extFilter.AppendJoin("JOIN songview ON songview.idAlbum = albumview.idAlbum");
      extFilter.AppendGroup("albumview.idAlbum");
    }

    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count number of albums that satisfy selection criteria (no limit built)
    // Count done in full query fetch when unlimited
    if (countOnly || limitedInSQL)
    {
      if (extended)
      {
        // Count distinct without group by
        Filter countFilter = extFilter;
        countFilter.group.clear();
        std::string strSQLWhere;
        if (!m_db.BuildSQL(strSQLWhere, countFilter, strSQLWhere))
          return false;
        total = m_db.GetSingleValueInt(
            "SELECT COUNT(DISTINCT albumview.idAlbum) FROM albumview " + strSQLWhere,
            *m_db.m_pDS);
      }
      else
        total = m_db.GetSingleValueInt("SELECT COUNT(1) FROM albumview " + strSQLExtra,
                                       *m_db.m_pDS);
    }
    if (countOnly)
    {
      auto pItem{std::make_shared<CFileItem>()};
      pItem->SetProperty("total", total);
      items.Add(std::move(pItem));

      m_db.m_pDS->close();
      return true;
    }

    // Apply any limiting directly in SQL
    if (limitedInSQL)
    {
      extFilter.limit =
          DatabaseUtils::BuildLimitClauseOnly(sorting.limitEnd, sorting.limitStart);
    }

    // Apply sort in SQL
    const std::shared_ptr<CSettings> settings =
        CServiceBroker::GetSettingsComponent()->GetSettings();
    if (settings->GetBool(CSettings::SETTING_MUSICLIBRARY_USEARTISTSORTNAME))
      sorting.sortAttributes = static_cast<SortAttribute>(sorting.sortAttributes |
                                                          SortAttributeUseArtistSortName);
    // Set Orderby and add any extra fields needed for sort e.g. "artistname" scalar query
    m_db.GetOrderFilter(MediaTypeAlbum, sorting, extFilter);
    // Modify order to use correct calculated year field
    if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
            CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE))
      StringUtils::Replace(extFilter.order, "iYear", "CAST(strReleaseDate AS INTEGER)");
    else
      StringUtils::Replace(extFilter.order, "iYear", "CAST(strOrigReleaseDate AS INTEGER)");

    strSQLExtra.clear();
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    std::string strSQL;
    std::string strFields = "albumview.*";
    if (!extFilter.fields.empty() && extFilter.fields.compare("*") != 0)
      strFields = "albumview.*, " + extFilter.fields;
    strSQL = "SELECT " + strFields + " FROM albumview " + strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    auto querytime = std::chrono::steady_clock::now();
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    auto queryEnd = std::chrono::steady_clock::now();
    auto queryDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - querytime);

    // Store the total number of albums as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);

    DatabaseResults results;
    results.reserve(iRowsFound);
    // Populate results field vector from dataset
    FieldList fields;
    if (!DatabaseUtils::GetDatabaseResults(MediaTypeAlbum, fields, *m_db.m_pDS, results))
      return false;
    // Store item list sort order
    items.SetSortMethod(sorting.sortBy);
    items.SetSortOrder(sorting.sortOrder);

    // Get albums from returned rows
    items.Reserve(results.size());
    const dbiplus::query_data& data = m_db.m_pDS->get_result_set().records;
    for (const auto& i : results)
    {
      const auto targetRow = static_cast<unsigned int>(i.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(targetRow);

      try
      {
        CMusicDbUrl itemUrl = musicUrl;
        std::string path = StringUtils::Format(
            "{}/", record->at(CMusicDatabase::album_idAlbum).get_asInt());
        itemUrl.AppendPath(path);

        auto pItem{std::make_shared<CFileItem>(itemUrl.ToString(),
                                               m_db.GetAlbumFromDataset(record))};
        // Set icon now to avoid slow per item processing in FillInDefaultIcon later
        pItem->SetProperty("icon_never_overlay", true);
        pItem->SetArt("icon", "DefaultAlbumCover.png");
        items.Add(std::move(pItem));
      }
      catch (...)
      {
        m_db.m_pDS->close();
        CLog::LogF(LOGERROR, "out of memory getting listing (got {})", items.Size());
      }
    }
    // cleanup
    m_db.m_pDS->close();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "Time to fill list with albums {}ms query took {}ms",
               duration.count(), queryDuration.count());

    return true;
  }
  catch (...)
  {
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "({}) failed", filter.where);
  }
  return false;
}

bool CMusicNavRepository::GetDiscsNav(const std::string& strBaseDir,
                                      CFileItemList& items,
                                      const SortDescription& sortDescription,
                                      int idAlbum,
                                      const Filter& filter,
                                      bool countOnly)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(strBaseDir))
    return false;

  if (idAlbum > 0)
    musicUrl.AddOption("albumid", idAlbum);

  return GetDiscsByWhere(musicUrl, items, sortDescription, filter, countOnly);
}

bool CMusicNavRepository::GetDiscsByWhere(const std::string& baseDir,
                                          CFileItemList& items,
                                          const SortDescription& sortDescription,
                                          const Filter& filter,
                                          bool countOnly)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(baseDir))
    return false;
  return GetDiscsByWhere(musicUrl, items, sortDescription, filter, countOnly);
}

bool CMusicNavRepository::GetDiscsByWhere(CMusicDbUrl& musicUrl,
                                          CFileItemList& items,
                                          const SortDescription& sortDescription,
                                          const Filter& filter,
                                          bool countOnly)
{
  if (m_db.m_pDB == nullptr || m_db.m_pDS == nullptr)
    return false;

  try
  {
    auto start = std::chrono::steady_clock::now();
    int total = -1;
    std::string strSQL;

    Filter extFilter = filter;
    SortDescription sorting = sortDescription;

    if (!m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    extFilter.AppendGroup("albumview.idAlbum, iDisc");

    // If there are extra songview WHERE conditions adjust to song or albumview
    // fields, and join Path table for strPath
    // ! @todo: convert songview fields into to song or albumview fields
    // But not sure we ever get songview fields in filter - REMOVE??
    if (extFilter.where.find("songview.strPath") != std::string::npos)
    {
      extFilter.AppendJoin("JOIN path ON song.idPath = path.idPath");
    }

    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Apply any limiting directly in SQL if there is either no special sorting or random sort
    // When limited, random sort is also applied in SQL
    bool limitedInSQL = extFilter.limit.empty() &&
                        (sorting.sortBy == SortBy::NONE || sorting.sortBy == SortBy::RANDOM) &&
                        (sorting.limitStart > 0 || sorting.limitEnd > 0);

    if (countOnly || limitedInSQL)
    {
      // Count number of discs that satisfy selection criteria
      // (when fetching all records get total from row count of results dataset)
      // Count not allow for same non-null title discs to be grouped together
      strSQL = "SELECT iTrack >> 16 AS iDisc FROM albumview JOIN song on song.idAlbum = "
               "albumview.idAlbum " +
               strSQLExtra;
      strSQL = "SELECT COUNT(1) FROM (" + strSQL + ") AS albumdisc ";
      total = m_db.GetSingleValueInt(strSQL, *m_db.m_pDS);
    }
    if (countOnly)
    {
      items.SetProperty("total", total);
      return true;
    }
    // Apply limits and random sort order directly in SQL
    if (limitedInSQL)
    {
      if (sorting.sortBy == SortBy::RANDOM)
        strSQLExtra += m_db.PrepareSQL(" ORDER BY RANDOM()");
      strSQLExtra +=
          DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }
    else
      strSQLExtra += m_db.PrepareSQL(" ORDER BY albumview.idAlbum, iDisc");

    strSQL = "SELECT iTrack >> 16 AS iDisc, strDiscSubtitle, albumview.* "
             "FROM albumview JOIN song on song.idAlbum = albumview.idAlbum " +
             strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    auto queryStart = std::chrono::steady_clock::now();
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    auto queryEnd = std::chrono::steady_clock::now();
    auto queryDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - queryStart);

    // store the total value of items as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);

    DatabaseResults results;
    results.reserve(iRowsFound);

    // Avoid sorting with limits, just fetch results from dataset
    // Limit when SortBy::NONE already applied in SQL,
    // Need guaranteed ordering for dataset processing to group by disc title
    // so apply sort later to fileitems list rather than dataset
    sorting.sortBy = SortBy::NONE;
    if (!SortUtils::SortFromDataset(sorting, MediaTypeAlbum, *m_db.m_pDS, results))
      return false;

    // Get data from returned rows, note possibly multiple albums although usually only one
    items.Reserve(total);
    int albumOffset = 2;
    CAlbum album;
    bool useTitle =
        true; // Assume we want to match by disc title later unless we have no titles
    std::string oldDiscTitle;
    const dbiplus::query_data& data = m_db.m_pDS->get_result_set().records;
    for (const auto& i : results)
    {
      const auto targetRow = static_cast<unsigned int>(i.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(targetRow);
      try
      {
        if (album.idAlbum !=
            record->at(albumOffset + CMusicDatabase::album_idAlbum).get_asInt())
        { // New album
          useTitle = true;
          album = m_db.GetAlbumFromDataset(record, albumOffset);
        }

        int discnum = record->at(0).get_asInt();
        std::string strDiscSubtitle = record->at(1).get_asString();
        if (strDiscSubtitle.empty())
        { // Make (fake) disc title from disc number, group by disc number as no real title to match
          strDiscSubtitle = StringUtils::Format(
              "{} {}",
              CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(427),
              discnum);
          useTitle = false;
        }
        else if (oldDiscTitle == strDiscSubtitle)
        { // disc title already added to list, fetch the next disc
          continue;
        }
        oldDiscTitle = strDiscSubtitle;

        CMusicDbUrl itemUrl = musicUrl;
        std::string path = StringUtils::Format("{}/", discnum);
        itemUrl.AppendPath(path);

        // When disc titles are provided group discs together by title not number.
        // For monster sets like https://musicbrainz.org/release/cc967f36-7e4e-4a5b-ae0d-f1a1ab2c9c5a
        if (useTitle)
          itemUrl.AddOption("disctitle", strDiscSubtitle.c_str());
        else
          itemUrl.AddOption("discid", discnum);
        auto pItem{std::make_shared<CFileItem>(itemUrl.ToString(), album)};
        pItem->SetLabel2(
            record->at(0).get_asString()); // GUI show label2 for disc sort order??
        pItem->GetMusicInfoTag()->SetDiscNumber(discnum);
        pItem->GetMusicInfoTag()->SetTitle(strDiscSubtitle);
        pItem->SetLabel(strDiscSubtitle);
        // Set icon now to avoid slow per item processing in FillInDefaultIcon later
        pItem->SetProperty("icon_never_overlay", true);
        pItem->SetArt("icon", "DefaultAlbumCover.png");
        items.Add(std::move(pItem));
      }
      catch (...)
      {
        m_db.m_pDS->close();
        CLog::LogF(LOGERROR, "out of memory getting listing (got {})", items.Size());
      }
    }

    // cleanup
    m_db.m_pDS->close();

    // Finally do any sorting in items list we have not been able to do before in SQL or dataset,
    // that is when have join with songartistview and sorting other than random with limit
    if (sorting.sortBy != SortBy::NONE && !(limitedInSQL && sorting.sortBy == SortBy::RANDOM))
      items.Sort(sorting);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "Time to fill list with discs {}ms query took {}ms",
               duration.count(), queryDuration.count());

    return true;
  }
  catch (...)
  {
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "({}) failed", filter.where);
  }

  return false;
}

int CMusicNavRepository::GetDiscsCount(const std::string& baseDir,
                                       const Filter& filter /* = Filter() */)
{
  int iDiscTotal = -1;
  CFileItemList itemscount;
  if (GetDiscsByWhere(baseDir, itemscount, SortDescription(), filter, true))
    iDiscTotal = itemscount.GetProperty("total").asInteger32();
  return iDiscTotal;
}

bool CMusicNavRepository::GetSongsFullByWhere(const std::string& baseDir,
                                              CFileItemList& items,
                                              const SortDescription& sortDescription,
                                              const Filter& filter,
                                              bool artistData /* = false*/)
{
  if (m_db.m_pDB == nullptr || m_db.m_pDS == nullptr)
    return false;

  try
  {
    auto start = std::chrono::steady_clock::now();
    int total = -1;

    Filter extFilter = filter;
    CMusicDbUrl musicUrl;
    SortDescription sorting = sortDescription;
    if (!musicUrl.FromString(baseDir) || !m_db.GetFilter(musicUrl, extFilter, sorting))
      return false;

    bool extended = false;
    bool limitedInSQL = extFilter.limit.empty() &&
                        (sortDescription.limitStart > 0 || sortDescription.limitEnd > 0);

    // If there are extra WHERE conditions (from media filter dialog) we might
    // need access to albumview for these conditions
    if (extFilter.where.find("albumview") != std::string::npos)
    {
      extended = true;
      extFilter.AppendJoin("JOIN albumview ON albumview.idAlbum = songview.idAlbum");
    }

    // Build songview <where> for count
    std::string strSQLExtra;
    if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count (without group by) number of songs that satisfy selection criteria
    // Much quicker to use song table, not songview, when filtering only on song fields
    if (extended ||
        (!extFilter.where.empty() &&
         (extFilter.where.find("strAlbum") != std::string::npos ||
          extFilter.where.find("strPath") != std::string::npos ||
          extFilter.where.find("bCompilation") != std::string::npos ||
          extFilter.where.find("bBoxedset") != std::string::npos)))
      total = m_db.GetSingleValueInt("SELECT COUNT(1) FROM songview " + strSQLExtra,
                                     *m_db.m_pDS);
    else
    {
      std::string strSQLsong = strSQLExtra;
      StringUtils::Replace(strSQLsong, "songview", "song");
      total =
          m_db.GetSingleValueInt("SELECT COUNT(1) FROM song " + strSQLsong, *m_db.m_pDS);
    }

    if (extended)
      extFilter.AppendGroup("songview.idSong");

    // Apply any limiting directly in SQL
    if (limitedInSQL)
    {
      extFilter.limit =
          DatabaseUtils::BuildLimitClauseOnly(sorting.limitEnd, sorting.limitStart);
    }

    // Apply sort in SQL
    const std::shared_ptr<CSettings> settings =
        CServiceBroker::GetSettingsComponent()->GetSettings();
    if (settings->GetBool(CSettings::SETTING_MUSICLIBRARY_USEARTISTSORTNAME))
      sorting.sortAttributes = static_cast<SortAttribute>(sorting.sortAttributes |
                                                          SortAttributeUseArtistSortName);
    // Set Orderby and add any extra fields needed for sort e.g. "artistname" scalar query
    m_db.GetOrderFilter(MediaTypeSong, sorting, extFilter);
    // Modify order to use correct calculated year field
    if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
            CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE))
      StringUtils::Replace(extFilter.order, "iYear", "CAST(strReleaseDate AS INTEGER)");
    else
      StringUtils::Replace(extFilter.order, "iYear", "CAST(strOrigReleaseDate AS INTEGER)");

    std::string strFields = "songview.*";
    if (!artistData || limitedInSQL)
    {
      // Build songview <where> + <order by> + <limits>
      strSQLExtra.clear();
      if (!m_db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
        return false;
    }
    else
      strFields = "songview.*, songartistview.*";
    if (!extFilter.fields.empty() && extFilter.fields.compare("*") != 0)
      strFields = strFields + ", " + extFilter.fields;

    std::string strSQL;
    if (artistData)
    { // Get data from song and song_artist tables to fully populate songs with artists
      // All songs now have at least one artist so inner join sufficient
      // Build songartistview JOIN part of query
      Filter joinFilter;
      std::string strSQLJoin;
      joinFilter.AppendJoin("JOIN songartistview ON songartistview.idSong = songview.idSong");
      if (sortDescription.sortBy == SortBy::RANDOM)
        joinFilter.AppendOrder("songartistview.idSong");
      else
        joinFilter.order = extFilter.order;
      if (limitedInSQL)
      {
        StringUtils::Replace(joinFilter.join, "songview.idSong", "sv.idSong");
        StringUtils::Replace(joinFilter.order, "songview.", "sv.");
      }
      else
        joinFilter.where = extFilter.where;
      joinFilter.AppendOrder("songartistview.idRole");
      joinFilter.AppendOrder("songartistview.iOrder");
      if (!m_db.BuildSQL(strSQLJoin, joinFilter, strSQLJoin))
        return false;

      if (limitedInSQL)
      {
        // When have artist data (all roles) and LIMIT on songs use inline view
        // SELECT sv.*, songartistview.* FROM
        //   (SELECT songview.* FROM songview <where> + <order by> + <limits> ) AS sv
        //   <order by sv fields>, songartistview.idRole, songartistview.iOrder
        // Apply where clause, limits and order to songview, then join to songartistview this gives
        // multiple records per song in result set
        strSQL = "SELECT " + strFields + " FROM songview " + strSQLExtra;
        strSQL = "(" + strSQL + ") AS sv ";
        strSQL = "SELECT sv.*, songartistview.* FROM " + strSQL + strSQLJoin;
      }
      else
        strSQL = "SELECT " + strFields + " FROM songview " + strSQLJoin;
    }
    else
      strSQL = "SELECT " + strFields + " FROM songview " + strSQLExtra;

    CLog::LogF(LOGDEBUG, "query = {}", strSQL);
    auto queryStart = std::chrono::steady_clock::now();
    // run query
    if (!m_db.m_pDS->query(strSQL))
      return false;

    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    auto queryEnd = std::chrono::steady_clock::now();
    auto queryDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(queryEnd - queryStart);

    // Store the total number of songs as a property
    items.SetProperty("total", total);

    DatabaseResults results;
    results.reserve(iRowsFound);
    // Populate results field vector from dataset
    FieldList fields;
    if (!DatabaseUtils::GetDatabaseResults(MediaTypeSong, fields, *m_db.m_pDS, results))
      return false;
    // Store item list sort order
    items.SetSortMethod(sorting.sortBy);
    items.SetSortOrder(sorting.sortOrder);

    // Get songs from returned rows. If join songartistview then there is a row for every artist
    items.Reserve(total);
    int songArtistOffset = CMusicDatabase::song_enumCount;
    int songId = -1;
    std::vector<CArtistCredit> artistCredits;
    const dbiplus::query_data& data = m_db.m_pDS->get_result_set().records;
    int count = 0;
    for (const auto& i : results)
    {
      const auto targetRow = static_cast<unsigned int>(i.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(targetRow);

      try
      {
        if (songId != record->at(CMusicDatabase::song_idSong).get_asInt())
        { //New song
          if (songId > 0 && !artistCredits.empty())
          {
            //Store artist credits for previous song
            m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
            artistCredits.clear();
          }
          songId = record->at(CMusicDatabase::song_idSong).get_asInt();
          auto item{std::make_shared<CFileItem>()};
          m_db.GetFileItemFromDataset(record, item.get(), musicUrl);
          //! @todo remove hack to use program count for sorting by database returned order
          count++;
          item->SetProgramCount(count);
          // Set icon now to avoid slow per item processing in FillInDefaultIcon later
          item->SetProperty("icon_never_overlay", true);
          item->SetArt("icon", "DefaultAudio.png");
          items.Add(std::move(item));
        }
        // Get song artist credits and contributors
        if (artistData)
        {
          int idSongArtistRole =
              record->at(songArtistOffset + CMusicDatabase::artistCredit_idRole).get_asInt();
          if (idSongArtistRole == ROLE_ARTIST)
            artistCredits.push_back(
                m_db.GetArtistCreditFromDataset(record, songArtistOffset));
          else
            items[items.Size() - 1]->GetMusicInfoTag()->AppendArtistRole(
                m_db.GetArtistRoleFromDataset(record, songArtistOffset));
        }
      }
      catch (...)
      {
        m_db.m_pDS->close();
        CLog::LogF(LOGERROR, "out of memory loading query: {}", filter.where);
        return (items.Size() > 0);
      }
    }
    if (!artistCredits.empty())
    {
      //Store artist credits for final song
      m_db.GetFileItemFromArtistCredits(artistCredits, items[items.Size() - 1].get());
      artistCredits.clear();
    }
    // cleanup
    m_db.m_pDS->close();

    // Ensure random order of item list when results set sorted by idSong for artist processing
    // Note while smartplaylists and xml nodes provide sort order, sort is not passed in from node
    // navigation. Order is read later from view state and list sorting is then triggered by
    // CGUIMediaWindow::Update in both cases.
    // So sorting here is currently redundant, but the consistent place to do it.
    // !@ todo: do sorting once, preferably in SQL
    if (sorting.sortBy == SortBy::RANDOM && artistData)
      items.Sort(sorting);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "Time to fill list with songs {}ms query took {}ms",
               duration.count(), queryDuration.count());

    return true;
  }
  catch (...)
  {
    // cleanup
    m_db.m_pDS->close();
    CLog::LogF(LOGERROR, "({}) failed", filter.where);
  }
  return false;
}

bool CMusicNavRepository::GetSongsByYear(const std::string& baseDir,
                                         CFileItemList& items,
                                         int year)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(baseDir))
    return false;

  musicUrl.AddOption("year", year);

  Filter filter;
  return GetSongsFullByWhere(baseDir, items, SortDescription(), filter, true);
}

bool CMusicNavRepository::GetSongsNav(const std::string& strBaseDir,
                                      CFileItemList& items,
                                      const SortDescription& sortDescription,
                                      int idGenre,
                                      int idArtist,
                                      int idAlbum)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(strBaseDir))
    return false;

  if (idAlbum > 0)
    musicUrl.AddOption("albumid", idAlbum);

  if (idGenre > 0)
    musicUrl.AddOption("genreid", idGenre);

  if (idArtist > 0)
    musicUrl.AddOption("artistid", idArtist);

  Filter filter;
  return GetSongsFullByWhere(musicUrl.ToString(), items, sortDescription, filter, true);
}

int CMusicNavRepository::GetSongsCount(const Filter& filter)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return 0;
    if (nullptr == m_db.m_pDS)
      return 0;

    std::string strSQL = "select count(idSong) as NumSongs from songview ";
    if (!m_db.BuildSQL(strSQL, filter, strSQL))
      return false;

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return 0;
    }

    int iNumSongs = m_db.m_pDS->fv("NumSongs").get_asInt();
    // cleanup
    m_db.m_pDS->close();
    return iNumSongs;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return 0;
}
