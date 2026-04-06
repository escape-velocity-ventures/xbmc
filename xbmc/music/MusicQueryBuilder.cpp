/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicQueryBuilder.h"

#include "Album.h"
#include "MusicDatabase.h"
#include "media/MediaType.h"
#include "MusicDbUrl.h"
#include "ServiceBroker.h"
#include "dbwrappers/dataset.h"
#include "imagefiles/ImageFileURL.h"
#include "playlists/SmartPlayList.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/DatabaseUtils.h"
#include "LangInfo.h"
#include "utils/Random.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <array>
#include <chrono>

using namespace KODI;

using Filter = CDatabase::Filter;
using DatasetLayout = CDatabase::DatasetLayout;
using ExistsSubQuery = CDatabase::ExistsSubQuery;

namespace
{
// clang-format off
struct TranslateJSONField
{
  std::string fieldJSON;  // Field name in JSON schema
  std::string formatJSON; // Format in JSON schema
  bool bSimple;           // Fetch field directly to JSON output
  std::string fieldDB;    // Name of field in db query
  std::string SQL;        // SQL for scalar subqueries or field alias
};

// clang-format off
const std::array<TranslateJSONField, 35> JSONtoDBArtist = {{
  // Table and single value join fields
  { "artist",                    "string", true,  "strArtist",              "" }, // Label field at top
  { "sortname",                  "string", true,  "strSortname",            "" },
  { "instrument",                 "array", true,  "strInstruments",         "" },
  { "description",               "string", true,  "strBiography",           "" },
  { "genre",                      "array", true,  "strGenres",              "" },
  { "mood",                       "array", true,  "strMoods",               "" },
  { "style",                      "array", true,  "strStyles",              "" },
  { "yearsactive",                "array", true,  "strYearsActive",         "" },
  { "born",                      "string", true,  "strBorn",                "" },
  { "formed",                    "string", true,  "strFormed",              "" },
  { "died",                      "string", true,  "strDied",                "" },
  { "disbanded",                 "string", true,  "strDisbanded",           "" },
  { "type",                      "string", true,  "strType",                "" },
  { "gender",                    "string", true,  "strGender",              "" },
  { "disambiguation",            "string", true,  "strDisambiguation",      "" },
  { "musicbrainzartistid",        "array", true,  "strMusicBrainzArtistId", "" }, // Array in schema, but only ever one element
  { "dateadded",                 "string", true,  "dateAdded",              "" },
  { "datenew",                   "string", true,  "dateNew",                "" },
  { "datemodified",              "string", true,  "dateModified",           "" },

  // JOIN fields (multivalue), same order as _JoinToArtistFields
  { "",                                "", false, "isSong",                 "" },
  { "sourceid",                  "string", false, "idSourceAlbum",          "album_source.idSource AS idSourceAlbum" },
  { "",                          "string", false, "idSourceSong",           "album_source.idSource AS idSourceSong" },
  { "songgenres",                 "array", false, "idSongGenreAlbum",       "song_genre.idGenre AS idSongGenreAlbum" },
  { "",                           "array", false, "idSongGenreSong",        "song_genre.idGenre AS idSongGenreSong" },
  { "",                                "", false, "strSongGenreAlbum",      "genre.strGenre AS strSongGenreAlbum" },
  { "",                                "", false, "strSongGenreSong",       "genre.strGenre AS strSongGenreSong" },
  { "art",                             "", false, "idArt",                  "art.art_id AS idArt" },
  { "",                                "", false, "artType",                "art.type AS artType" },
  { "",                                "", false, "artURL",                 "art.url AS artURL" },
  { "",                                "", false, "idRole",                 "song_artist.idRole" },
  { "roles",                           "", false, "strRole",                "role.strRole" },
  { "",                                "", false, "iOrderRole",             "song_artist.iOrder AS iOrderRole" },
  // Derived from joined tables
  { "isalbumartist",               "bool", false, "",                       "" },
  { "thumbnail",                 "string", false, "",                       "" },
  { "fanart",                    "string", false, "",                       "" }
  /*
   Sources and genre are related via album, and so the dataset only contains source and genre
   pairs that exist, rather than all the genres being repeated for every source. We can not only
   look at genres for the first source, and genre can be out of order.
   */
}};
// clang-format on
} // unnamed namespace

bool CMusicQueryBuilder::GetArtistsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                           const std::string& baseDir,
                                           CVariant& result,
                                           int& total,
                                           const SortDescription& sortDescription,
    CMusicDatabase& db)
{
  if (nullptr == db.m_pDB)
    return false;
  if (nullptr == db.m_pDS)
    return false;

  try
  {
    total = -1;

    size_t resultcount = 0;
    Filter extFilter;
    CMusicDbUrl musicUrl;
    SortDescription sorting = sortDescription;
    //! @todo: replace GetFilter to avoid exists as well as JOIn to albm_artist and song_artist tables
    if (!musicUrl.FromString(baseDir) || !CMusicQueryBuilder::GetFilter(musicUrl, extFilter, sorting, db))
      return false;

    // Replace view names in filter with table names
    StringUtils::Replace(extFilter.where, "artistview", "artist");
    StringUtils::Replace(extFilter.where, "albumview", "album");

    std::string strSQLExtra;
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count number of artists that satisfy selection criteria
    //(includes xsp limits from filter, but not sort limits)
    total = db.GetSingleValueInt("SELECT COUNT(1) FROM artist " + strSQLExtra, *db.m_pDS);
    resultcount = static_cast<size_t>(total);

    // Process albumartistsonly option
    const CUrlOptions::UrlOptions& options = musicUrl.GetOptions();
    bool albumArtistsOnly(false);
    auto option = options.find("albumartistsonly");
    if (option != options.end())
      albumArtistsOnly = option->second.asBoolean();
    // Process role options
    int roleidfilter = 1; // Default restrict song_artist to "artists" only, no other roles.
    option = options.find("roleid");
    if (option != options.end())
      roleidfilter = static_cast<int>(option->second.asInteger());
    else
    {
      option = options.find("role");
      if (option != options.end())
      {
        if (option->second.asString() == "all" || option->second.asString() == "%")
          roleidfilter = -1000; //All roles
        else
          roleidfilter = db.GetRoleByName(option->second.asString());
      }
    }

    // Get order by (and any scalar query artist fields)
    int iAddedFields = CMusicQueryBuilder::GetOrderFilter(MediaTypeArtist, sortDescription, extFilter, db);
    // Replace artistview field names in order by artist table field names
    StringUtils::Replace(extFilter.order, "artistview", "artist");
    StringUtils::Replace(extFilter.fields, "artistview", "artist");

    // Grab and adjust artist sort field that may have been added to filter
    // These need to be added to the end of the artist table field list
    std::string artistsortSQL = extFilter.fields;
    extFilter.fields.clear();

    std::string strSQL;

    // Setup fields to query, and album field number mapping
    // Find first join field (isSong) in JSONtoDBArtist for offset
    int index_firstjoin = -1;
    for (unsigned int i = 0; i < std::size(JSONtoDBArtist); i++)
    {
      if (JSONtoDBArtist[i].fieldDB == "isSong")
      {
        index_firstjoin = i;
        break;
      }
    }
    Filter joinFilter;
    Filter albumArtistFilter;
    Filter songArtistFilter;
    DatasetLayout joinLayout(static_cast<size_t>(joinToArtist_enumCount));
    extFilter.AppendField("artist.idArtist"); // ID "artistid" in JSON
    std::vector<int> dbfieldindex;
    // JSON "label" field is strArtist which is also output as "artist", query field once output twice
    extFilter.AppendField(JSONtoDBArtist[0].fieldDB);
    dbfieldindex.emplace_back(0); // Output "artist"

    // Check each optional artist db field that could be retrieved (not "artist")
    for (unsigned int i = 1; i < std::size(JSONtoDBArtist); i++)
    {
      bool foundJSON = fields.contains(JSONtoDBArtist[i].fieldJSON);
      if (JSONtoDBArtist[i].bSimple)
      {
        // Check for non-join fields in order too.
        // Query these in inline view (but not output) so can ref in outer order
        bool foundOrderby(false);
        if (!foundJSON)
          foundOrderby = extFilter.order.find(JSONtoDBArtist[i].fieldDB) != std::string::npos;
        if (foundOrderby || foundJSON)
        {
          // Store indexes of requested artist table and scalar subquery fields
          // to be output, and -1 when not output to JSON
          if (!foundJSON)
            dbfieldindex.emplace_back(-1);
          else
            dbfieldindex.emplace_back(i);
          // Field from scaler subquery
          if (!JSONtoDBArtist[i].SQL.empty())
            extFilter.AppendField(db.PrepareSQL(JSONtoDBArtist[i].SQL));
          else
            // Field from artist table
            extFilter.AppendField(JSONtoDBArtist[i].fieldDB);
        }
      }
      else if (foundJSON)
        // Field from join or derived from joined fields
        joinLayout.SetField(i - index_firstjoin, JSONtoDBArtist[i].fieldDB, true);
    }

    // Append calculated artistsort field that may have been added to filter
    // Field used only for ORDER BY, not output to JSON
    extFilter.AppendField(artistsortSQL);
    for (int i = 0; i < iAddedFields; i++)
      dbfieldindex.emplace_back(-2); // columns in dataset

    // Build JOIN, WHERE, ORDER BY and LIMIT for inline view
    strSQLExtra = "";
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Add any LIMIT clause to strSQLExtra
    if (extFilter.limit.empty() && (sortDescription.limitStart > 0 || sortDescription.limitEnd > 0))
    {
      strSQLExtra +=
          DatabaseUtils::BuildLimitClause(sortDescription.limitEnd, sortDescription.limitStart);
      resultcount = std::min(
          DatabaseUtils::GetLimitCount(sortDescription.limitEnd, sortDescription.limitStart),
          resultcount);
    }

    // Setup multivalue JOINs, GROUP BY and ORDER BY
    bool bJoinAlbumArtist(false);
    bool bJoinSongArtist(false);
    if (sortDescription.sortBy != SortBy::RANDOM)
    {
      // Repeat inline view order (that always includes idArtist) on join query
      std::string order = extFilter.order;
      StringUtils::Replace(order, "artist.", "a1.");
      joinFilter.AppendOrder(order);
    }
    else
      joinFilter.AppendOrder("a1.idArtist");
    joinFilter.AppendGroup("a1.idArtist");
    // Album artists and song artists
    if ((joinLayout.GetFetch(joinToArtist_isalbumartist) && !albumArtistsOnly) ||
        joinLayout.GetFetch(joinToArtist_idSourceAlbum) ||
        joinLayout.GetFetch(joinToArtist_idSongGenreAlbum) ||
        joinLayout.GetFetch(joinToArtist_strRole))
    {
      bJoinAlbumArtist = true;
      albumArtistFilter.AppendField("album_artist.idArtist AS id");
      if (!albumArtistsOnly || joinLayout.GetFetch(joinToArtist_strRole))
      {
        bJoinSongArtist = true;
        songArtistFilter.AppendField("song_artist.idArtist AS id");
        songArtistFilter.AppendField("1 AS isSong");
        albumArtistFilter.AppendField("0 AS isSong");
        joinLayout.SetField(joinToArtist_isSong,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_isSong].fieldDB);
        joinFilter.AppendGroup(JSONtoDBArtist[index_firstjoin + joinToArtist_isSong].fieldDB);
        joinFilter.AppendOrder(JSONtoDBArtist[index_firstjoin + joinToArtist_isSong].fieldDB);
      }
    }
    else if (joinLayout.GetFetch(joinToArtist_isalbumartist))
    {
      // Filtering album artists only and isalbumartist requested but not source, songgenres or roles,
      // so no need for join to album_artist table. Set fetching fetch false so that
      // joinLayout.HasFilterFields() is false
      joinLayout.SetFetch(joinToArtist_isalbumartist, false);
    }

    // Sources
    if (joinLayout.GetFetch(joinToArtist_idSourceAlbum))
    { // Left join as source may have been removed but leaving lib entries
      albumArtistFilter.AppendJoin(
          "LEFT JOIN album_source ON album_source.idAlbum = album_artist.idAlbum");
      albumArtistFilter.AppendField(
          JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceAlbum].SQL);
      joinFilter.AppendGroup(JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceAlbum].fieldDB);
      joinFilter.AppendOrder(JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceAlbum].fieldDB);
      if (bJoinSongArtist)
      {
        songArtistFilter.AppendJoin("JOIN song ON song.idSong = song_artist.idSong");
        songArtistFilter.AppendJoin(
            "LEFT JOIN album_source ON album_source.idAlbum = song.idAlbum");
        songArtistFilter.AppendField(
            "-1 AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceAlbum].fieldDB);
        songArtistFilter.AppendField(
            JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceSong].SQL);
        albumArtistFilter.AppendField(
            "-1 AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceSong].fieldDB);
        joinLayout.SetField(joinToArtist_idSourceSong,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceSong].fieldDB);
        joinFilter.AppendGroup(JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceSong].fieldDB);
        joinFilter.AppendOrder(JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceSong].fieldDB);
      }
      else
      {
        joinLayout.SetField(joinToArtist_idSourceAlbum,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_idSourceAlbum].SQL, true);
      }
    }

    // Songgenres - id and genres always both
    if (joinLayout.GetFetch(joinToArtist_idSongGenreAlbum))
    { // All albums have songs, but left join genre as songs may not have genre
      albumArtistFilter.AppendJoin("JOIN song ON song.idAlbum = album_artist.idAlbum");
      albumArtistFilter.AppendJoin("LEFT JOIN song_genre ON song_genre.idSong = song.idSong");
      albumArtistFilter.AppendJoin("LEFT JOIN genre ON genre.idGenre = song_genre.idGenre");
      albumArtistFilter.AppendField(
          JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreAlbum].SQL);
      albumArtistFilter.AppendField(
          JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreAlbum].SQL);
      joinLayout.SetField(joinToArtist_strSongGenreAlbum,
                          JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreAlbum].fieldDB);
      joinFilter.AppendGroup(
          JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreAlbum].fieldDB);
      joinFilter.AppendOrder(
          JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreAlbum].fieldDB);
      if (bJoinSongArtist)
      { // Left join genre as songs may not have genre
        songArtistFilter.AppendJoin(
            "LEFT JOIN song_genre ON song_genre.idSong = song_artist.idSong");
        songArtistFilter.AppendJoin("LEFT JOIN genre ON genre.idGenre = song_genre.idGenre");
        songArtistFilter.AppendField(
            "-1 AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreAlbum].fieldDB);
        songArtistFilter.AppendField(
            "'' AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreAlbum].fieldDB);
        songArtistFilter.AppendField(
            JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreSong].SQL);
        songArtistFilter.AppendField(
            JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreSong].SQL);
        albumArtistFilter.AppendField(
            "-1 AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreSong].fieldDB);
        albumArtistFilter.AppendField(
            "'' AS " + JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreSong].fieldDB);
        joinLayout.SetField(joinToArtist_idSongGenreSong,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreSong].fieldDB);
        joinLayout.SetField(
            joinToArtist_strSongGenreSong,
            JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreSong].fieldDB);
        joinFilter.AppendGroup(
            JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreSong].fieldDB);
        joinFilter.AppendOrder(
            JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreSong].fieldDB);
      }
      else
      { // Define field alias names in join layout
        joinLayout.SetField(joinToArtist_idSongGenreAlbum,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_idSongGenreAlbum].SQL,
                            true);
        joinLayout.SetField(joinToArtist_strSongGenreAlbum,
                            JSONtoDBArtist[index_firstjoin + joinToArtist_strSongGenreAlbum].SQL);
      }
    }

    // Roles
    if (roleidfilter == 1 && !joinLayout.GetFetch(joinToArtist_strRole))
      // Only looking at album and song artists not other roles (default),
      // so filter dataset rows likewise.
      songArtistFilter.AppendWhere("song_artist.idRole = 1");
    else if (joinLayout.GetFetch(joinToArtist_strRole) || // "roles" field
             (bJoinSongArtist && (joinLayout.GetFetch(joinToArtist_idSourceAlbum) ||
                                  joinLayout.GetFetch(joinToArtist_idSongGenreAlbum))))
    { // Rows from many roles so fetch roleid for "roles", source and genre processing
      songArtistFilter.AppendField(JSONtoDBArtist[index_firstjoin + joinToArtist_idRole].SQL);
      // Add fake column to album_artist query
      albumArtistFilter.AppendField("-1 AS " +
                                    JSONtoDBArtist[index_firstjoin + joinToArtist_idRole].fieldDB);
      joinLayout.SetField(joinToArtist_idRole,
                          JSONtoDBArtist[index_firstjoin + joinToArtist_idRole].fieldDB);
      joinFilter.AppendGroup(JSONtoDBArtist[index_firstjoin + joinToArtist_idRole].fieldDB);
      joinFilter.AppendOrder(JSONtoDBArtist[index_firstjoin + joinToArtist_idRole].fieldDB);
    }
    if (joinLayout.GetFetch(joinToArtist_strRole))
    { // Fetch role desc
      songArtistFilter.AppendJoin("JOIN role ON role.idRole = song_artist.idRole");
      songArtistFilter.AppendField(JSONtoDBArtist[index_firstjoin + joinToArtist_strRole].SQL);
      // Add fake column to album_artist query
      albumArtistFilter.AppendField("'albumartist' AS " +
                                    JSONtoDBArtist[index_firstjoin + joinToArtist_strRole].fieldDB);
    }

    // Build source, genre and roles part of query
    if (bJoinAlbumArtist)
    {
      if (bJoinSongArtist)
      {
        // Combine song and album artist filter as UNION and add to join filter as an inline view
        std::string strAlbumSQL;
        if (!db.BuildSQL(strAlbumSQL, albumArtistFilter, strAlbumSQL))
          return false;
        strAlbumSQL = "SELECT " + albumArtistFilter.fields + " FROM album_artist " + strAlbumSQL;
        std::string strSongSQL;
        if (!db.BuildSQL(strSongSQL, songArtistFilter, strSongSQL))
          return false;
        strSongSQL = "SELECT " + songArtistFilter.fields + " FROM song_artist " + strSongSQL;

        joinFilter.AppendJoin("JOIN (" + strAlbumSQL + " UNION " + strSongSQL +
                              ") AS albumSong ON id = a1.idArtist");
      }
      else
      { //Only join album_artist, so move filter elements to join filter
        joinFilter.AppendJoin("JOIN album_artist ON album_artist.idArtist = a1.idArtist");
        joinFilter.AppendJoin(albumArtistFilter.join);
      }
    }

    //Art
    bool bJoinArt(false);
    bJoinArt = joinLayout.GetOutput(joinToArtist_idArt) ||
               joinLayout.GetOutput(joinToArtist_thumbnail) ||
               joinLayout.GetOutput(joinToArtist_fanart);
    if (bJoinArt)
    { // Left join as artist may not have any art
      joinFilter.AppendJoin(
          "LEFT JOIN art ON art.media_id = a1.idArtist AND art.media_type = 'artist'");
      joinLayout.SetField(joinToArtist_idArt,
                          JSONtoDBArtist[index_firstjoin + joinToArtist_idArt].SQL,
                          joinLayout.GetOutput(joinToArtist_idArt));
      joinLayout.SetField(joinToArtist_artType,
                          JSONtoDBArtist[index_firstjoin + joinToArtist_artType].SQL);
      joinLayout.SetField(joinToArtist_artURL,
                          JSONtoDBArtist[index_firstjoin + joinToArtist_artURL].SQL);
      joinFilter.AppendGroup("art.art_id");
      joinFilter.AppendOrder("arttype");
      if (!joinLayout.GetOutput(joinToArtist_idArt))
      {
        if (!joinLayout.GetOutput(joinToArtist_thumbnail))
          // Fanart only
          joinFilter.AppendJoin("AND art.type = 'fanart'");
        else if (!joinLayout.GetOutput(joinToArtist_fanart))
          // Thumb only
          joinFilter.AppendJoin("AND art.type = 'thumb'");
      }
    }
    else if (bJoinSongArtist)
      joinFilter.group.clear(); // UNION only so no GROUP BY needed

    // Build JOIN part of query (if we have one)
    std::string strSQLJoin;
    if (joinLayout.HasFilterFields() && !db.BuildSQL(strSQLJoin, joinFilter, strSQLJoin))
      return false;

    // Adjust where in the results record the join fields are allowing for the
    // inline view fields (Quicker than finding field by name every time)
    // idArtist + other artist fields
    joinLayout.AdjustRecordNumbers(static_cast<int>(1 + dbfieldindex.size()));

    // Build full query
    // When have multiple value joins e.g. song genres, use inline view
    // SELECT a1.*, <join fields> FROM
    //   (SELECT <artist fields> FROM artist <where> + <order by> +  <limits> ) AS a1
    //   <joins> <group by> <order by> + <joins order by>
    // Don't use prepareSQL - confuses  arttype = 'thumb' filter

    strSQL = "SELECT " + extFilter.fields + " FROM artist " + strSQLExtra;
    if (joinLayout.HasFilterFields())
    {
      strSQL = "(" + strSQL + ") AS a1 ";
      strSQL = "SELECT a1.*, " + joinLayout.GetFields() + " FROM " + strSQL + strSQLJoin;
    }

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    // run query
    auto start = std::chrono::steady_clock::now();

    if (!db.m_pDS->query(strSQL))
      return false;

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "query took {} ms", duration.count());

    int iRowsFound = db.m_pDS->num_rows();
    if (iRowsFound <= 0)
    {
      db.m_pDS->close();
      return true;
    }

    // Get artists from returned rows. Joins means there can be many rows per artist
    int artistId = -1;
    int sourceId = -1;
    int genreId = -1;
    int roleId = -1;
    int artId = -1;
    std::vector<int> genreidlist;
    std::vector<int> sourceidlist;
    std::vector<int> roleidlist;
    bool bArtDone(false);
    bool bHaveArtist(false);
    bool bIsAlbumArtist(true);
    bool bGenreFoundViaAlbum(false);
    CVariant artistObj;
    result["artists"].reserve(resultcount);
    while (!db.m_pDS->eof() || bHaveArtist)
    {
      const dbiplus::sql_record* const record = db.m_pDS->get_sql_record();

      if (db.m_pDS->eof() || artistId != record->at(0).get_asInt())
      {
        // Store previous or last artist
        if (bHaveArtist)
        {
          // Convert any empty MBid array into an array with one empty element [""]
          // to match the number of artist ID (way other mbid arrays handled)
          if (artistObj.isMember("musicbrainzartistid") && artistObj["musicbrainzartistid"].empty())
            artistObj["musicbrainzartistid"].append("");

          result["artists"].append(artistObj);
          bHaveArtist = false;
          artistObj.clear();
        }
        if (artistObj.empty())
        {
          // Initialise fields, ensure those with possible null values are set to correct empty variant type
          if (joinLayout.GetOutput(joinToArtist_idSourceAlbum))
            artistObj["sourceid"] = CVariant(CVariant::VariantTypeArray);
          if (joinLayout.GetOutput(joinToArtist_idSongGenreAlbum))
            artistObj["songgenres"] = CVariant(CVariant::VariantTypeArray);
          if (joinLayout.GetOutput(joinToArtist_idArt))
            artistObj["art"] = CVariant(CVariant::VariantTypeObject);
          if (joinLayout.GetOutput(joinToArtist_thumbnail))
            artistObj["thumbnail"] = "";
          if (joinLayout.GetOutput(joinToArtist_fanart))
            artistObj["fanart"] = "";

          sourceId = -1;
          roleId = -1;
          genreId = -1;
          artId = -1;
          genreidlist.clear();
          bGenreFoundViaAlbum = false;
          sourceidlist.clear();
          roleidlist.clear();
          bArtDone = false;
        }
        if (db.m_pDS->eof())
          continue; // Having saved the last artist stop

        // New artist
        artistId = record->at(0).get_asInt();
        bHaveArtist = true;
        artistObj["artistid"] = artistId;
        artistObj["label"] = record->at(1).get_asString();
        artistObj["artist"] = record->at(1).get_asString(); // Always have "artist"
        bIsAlbumArtist = true; //Album artist by default
        if (joinLayout.GetOutput(joinToArtist_isalbumartist))
        {
          // Not album artist when fetching song artists too and first row for artist isSong=true
          if (bJoinSongArtist)
            bIsAlbumArtist = !record->at(joinLayout.GetRecNo(joinToArtist_isSong)).get_asBool();
          artistObj["isalbumartist"] = bIsAlbumArtist;
        }
        for (size_t i = 0; i < dbfieldindex.size(); i++)
          if (dbfieldindex[i] > -1)
          {
            if (JSONtoDBArtist[dbfieldindex[i]].formatJSON == "integer")
              artistObj[JSONtoDBArtist[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asInt();
            else if (JSONtoDBArtist[dbfieldindex[i]].formatJSON == "float")
              artistObj[JSONtoDBArtist[dbfieldindex[i]].fieldJSON] =
                  record->at(1 + i).get_asFloat();
            else if (JSONtoDBArtist[dbfieldindex[i]].formatJSON == "array")
              artistObj[JSONtoDBArtist[dbfieldindex[i]].fieldJSON] = StringUtils::Split(
                  record->at(1 + i).get_asString(), CServiceBroker::GetSettingsComponent()
                                                        ->GetAdvancedSettings()
                                                        ->m_musicItemSeparator);
            else if (JSONtoDBArtist[dbfieldindex[i]].formatJSON == "boolean")
              artistObj[JSONtoDBArtist[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asBool();
            else
              artistObj[JSONtoDBArtist[dbfieldindex[i]].fieldJSON] =
                  record->at(1 + i).get_asString();
          }
      }
      if (bJoinAlbumArtist)
      {
        bool bAlbumArtistRow(true);
        int idRoleRow = -1;
        if (bJoinSongArtist)
        {
          bAlbumArtistRow = !record->at(joinLayout.GetRecNo(joinToArtist_isSong)).get_asBool();
          if (joinLayout.GetRecNo(joinToArtist_idRole) > -1 &&
              !record->at(joinLayout.GetRecNo(joinToArtist_idRole)).get_isNull())
          {
            idRoleRow = record->at(joinLayout.GetRecNo(joinToArtist_idRole)).get_asInt();
          }
        }

        // Sources - gathered via both album_artist and song_artist (with role = 1)
        if (joinLayout.GetFetch(joinToArtist_idSourceAlbum))
        {
          if ((bAlbumArtistRow && joinLayout.GetRecNo(joinToArtist_idSourceAlbum) > -1 &&
               !record->at(joinLayout.GetRecNo(joinToArtist_idSourceAlbum)).get_isNull() &&
               sourceId !=
                   record->at(joinLayout.GetRecNo(joinToArtist_idSourceAlbum)).get_asInt()) ||
              (!bAlbumArtistRow && joinLayout.GetRecNo(joinToArtist_idSourceSong) > -1 &&
               !record->at(joinLayout.GetRecNo(joinToArtist_idSourceSong)).get_isNull() &&
               sourceId != record->at(joinLayout.GetRecNo(joinToArtist_idSourceSong)).get_asInt()))
          {
            bArtDone = bArtDone || (sourceId > 0); // Not first source, skip art repeats
            bool found(false);
            sourceId = record->at(joinLayout.GetRecNo(joinToArtist_idSourceAlbum)).get_asInt();
            if (!bAlbumArtistRow)
            {
              // Skip other roles (when fetching them)
              if (idRoleRow > 1)
              {
                found = true;
              }
              else
              {
                sourceId = record->at(joinLayout.GetRecNo(joinToArtist_idSourceSong)).get_asInt();
                // Song artist row may repeat sources found via album artist
                // Already have that source?
                for (const auto& i : sourceidlist)
                  if (i == sourceId)
                  {
                    found = true;
                    break;
                  }
              }
            }
            if (!found)
            {
              sourceidlist.emplace_back(sourceId);
              artistObj["sourceid"].append(sourceId);
            }
          }
        }
        // Songgenres - via album artist takes precedence
        /*
        Sources and genre are related via album, and so the dataset only contains source
        and genre pairs that exist, rather than all the genres being repeated for every
        source. We can not only look at genres for the first source, and genre can be
        found out of order.
        Also song artist row may repeat genres found via album artist
        */
        if (joinLayout.GetFetch(joinToArtist_idSongGenreAlbum))
        {
          std::string strGenre;
          bool newgenre(false);
          if (bAlbumArtistRow && joinLayout.GetRecNo(joinToArtist_idSongGenreAlbum) > -1 &&
              !record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreAlbum)).get_isNull() &&
              genreId != record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreAlbum)).get_asInt())
          {
            bArtDone = bArtDone || (genreId > 0); // Not first genre, skip art repeats
            newgenre = true;
            genreId = record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreAlbum)).get_asInt();
            strGenre =
                record->at(joinLayout.GetRecNo(joinToArtist_strSongGenreAlbum)).get_asString();
          }
          else if (!bAlbumArtistRow && !bGenreFoundViaAlbum &&
                   joinLayout.GetRecNo(joinToArtist_idSongGenreSong) > -1 &&
                   !record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreSong)).get_isNull() &&
                   genreId !=
                       record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreSong)).get_asInt())
          {
            bArtDone = bArtDone || (genreId > 0); // Not first genre, skip art repeats
            newgenre = idRoleRow <= 1; // Skip other roles (when fetching them)
            genreId = record->at(joinLayout.GetRecNo(joinToArtist_idSongGenreSong)).get_asInt();
            strGenre =
                record->at(joinLayout.GetRecNo(joinToArtist_strSongGenreSong)).get_asString();
          }
          if (newgenre)
          {
            // Already have that genre?
            bool found(false);
            for (const auto& i : genreidlist)
              if (i == genreId)
              {
                found = true;
                break;
              }
            if (!found)
            {
              bGenreFoundViaAlbum = bGenreFoundViaAlbum || bAlbumArtistRow;
              genreidlist.emplace_back(genreId);
              CVariant genreObj;
              genreObj["genreid"] = genreId;
              genreObj["title"] = strGenre;
              artistObj["songgenres"].append(genreObj);
            }
          }
        }
        // Roles - gathered via song_artist roleid rows
        if (joinLayout.GetFetch(joinToArtist_idRole))
        {
          if (!bAlbumArtistRow && roleId != idRoleRow)
          {
            bArtDone = bArtDone || (roleId > 0); // Not first role, skip art repeats
            roleId = idRoleRow;
            if (joinLayout.GetOutput(joinToArtist_strRole))
            {
              // Already have that role?
              bool found(false);
              for (const auto& i : roleidlist)
                if (i == roleId)
                {
                  found = true;
                  break;
                }
              if (!found)
              {
                roleidlist.emplace_back(roleId);
                CVariant roleObj;
                roleObj["roleid"] = roleId;
                roleObj["role"] =
                    record->at(joinLayout.GetRecNo(joinToArtist_strRole)).get_asString();
                artistObj["roles"].append(roleObj);
              }
            }
          }
        }
      }
      // Art
      if (bJoinArt && !bArtDone &&
          !record->at(joinLayout.GetRecNo(joinToArtist_idArt)).get_isNull() &&
          record->at(joinLayout.GetRecNo(joinToArtist_idArt)).get_asInt() > 0 &&
          artId != record->at(joinLayout.GetRecNo(joinToArtist_idArt)).get_asInt())
      {
        artId = record->at(joinLayout.GetRecNo(joinToArtist_idArt)).get_asInt();
        if (joinLayout.GetOutput(joinToArtist_idArt))
        {
          artistObj["art"][record->at(joinLayout.GetRecNo(joinToArtist_artType)).get_asString()] =
              IMAGE_FILES::URLFromFile(
                  record->at(joinLayout.GetRecNo(joinToArtist_artURL)).get_asString());
        }
        if (joinLayout.GetOutput(joinToArtist_thumbnail) &&
            record->at(joinLayout.GetRecNo(joinToArtist_artType)).get_asString() == "thumb")
        {
          artistObj["thumbnail"] = IMAGE_FILES::URLFromFile(
              record->at(joinLayout.GetRecNo(joinToArtist_artURL)).get_asString());
        }
        if (joinLayout.GetOutput(joinToArtist_fanart) &&
            record->at(joinLayout.GetRecNo(joinToArtist_artType)).get_asString() == "fanart")
        {
          artistObj["fanart"] = IMAGE_FILES::URLFromFile(
              record->at(joinLayout.GetRecNo(joinToArtist_artURL)).get_asString());
        }
      }

      db.m_pDS->next();
    }
    db.m_pDS->close(); // cleanup recordset data

    // Ensure random order of output when results set is sorted to process multi-value joins
    if (sortDescription.sortBy == SortBy::RANDOM && joinLayout.HasFilterFields())
      KODI::UTILS::RandomShuffle(result["artists"].begin_array(), result["artists"].end_array());

    return true;
  }
  catch (...)
  {
    db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}


namespace
{
// clang-format off
const std::array<TranslateJSONField, 35> JSONtoDBAlbum = {{
  // albumview (inc scalar subquery fields use in filter rules)
  { "title",                     "string", true,  "strAlbum",               "" },  // Label field at top
  { "description",               "string", true,  "strReview",              "" },
  { "genre",                      "array", true,  "strGenres",              "" },
  { "theme",                      "array", true,  "strThemes",              "" },
  { "mood",                       "array", true,  "strMoods",               "" },
  { "style",                      "array", true,  "strStyles",              "" },
  { "type",                      "string", true,  "strType",                "" },
  { "albumlabel",                "string", true,  "strLabel",               "" },
  { "rating",                     "float", true,  "fRating",                "" },
  { "votes",                    "integer", true,  "iVotes",                 "" },
  { "userrating",              "unsigned", true,  "iUserrating",            "" },
  { "isboxset",                 "boolean", true,  "bBoxedSet",              "" },
  { "musicbrainzalbumid",        "string", true,  "strMusicBrainzAlbumID",  "" },
  { "displayartist",             "string", true,  "strArtists",             "" }, //strArtistDisp in album table
  { "compilation",              "boolean", true,  "bCompilation",           "" },
  { "releasetype",               "string", true,  "strReleaseType",         "" },
  { "totaldiscs",               "integer", true,  "iDiscTotal",             "" },
  { "sortartist",                "string", true,  "strArtistSort",          "" },
  { "musicbrainzreleasegroupid", "string", true,  "strReleaseGroupMBID",    "" },
  { "playcount",                "integer", true,  "iTimesPlayed",           "" },  // Scalar subquery in view
  { "dateadded",                 "string", true,  "dateAdded",              "" },
  { "datenew",                   "string", true,  "dateNew",                "" },
  { "datemodified",              "string", true,  "dateModified",           "" },
  { "lastplayed",                "string", true,  "lastPlayed",             "" },  // Scalar subquery in view
  { "originaldate",              "string", true,  "strOrigReleaseDate",     "" },
  { "releasedate",               "string", true,  "strReleaseDate",         "" },
  { "albumstatus",               "string", true,  "strReleaseStatus",       "" },
  { "albumduration",             "integer", true,  "iAlbumDuration",        "" },
  // Scalar subquery fields
  { "year",                     "integer", true,  "iYear",                  "CAST(<datefield> AS INTEGER) AS iYear" }, //From strReleaseDate or strOrigReleaseDate
  { "sourceid",                  "string", true,  "sourceid",               "(SELECT GROUP_CONCAT(album_source.idSource SEPARATOR '; ') FROM album_source WHERE album_source.idAlbum = albumview.idAlbum) AS sources" },
  { "songgenres",                 "array", true,  "songgenres",             "(SELECT GROUP_CONCAT(DISTINCT CONCAT(genre.idGenre, ',', REPLACE(genre.strGenre, ',', '-'))) FROM song "
    "JOIN song_genre ON song.idSong = song_genre.idSong JOIN genre ON song_genre.idGenre = genre.idGenre WHERE song.idAlbum = albumview.idAlbum) AS songgenres" } ,
  // Single value JOIN fields
  { "thumbnail",                  "image", true,  "thumbnail",              "art.url AS thumbnail" }, // or (SELECT art.url FROM art WHERE art.media_id = album.idAlbum AND art.media_type = "album" AND art.type = "thumb") as url
                                                                                                      // JOIN fields (multivalue), same order as _JoinToAlbumFields
  { "artistid",                   "array", false, "idArtist",               "album_artist.idArtist AS idArtist" },
  { "artist",                     "array", false, "strArtist",              "artist.strArtist AS strArtist" },
  { "musicbrainzalbumartistid",   "array", false, "strArtistMBID",          "artist.strMusicBrainzArtistID AS strArtistMBID" },
  /*
   Album "fanart" and "art" fields of JSON schema are fetched using thumbloader
   and separate queries to allow for fallback strategy.

   Using albmview, rather than album table, as view has scalar subqueries for
   playcount and lastplayed already defined. Needed as MySQL does
   not support use of scalar subquery field alias names in where clauses (they
   have to be repeated) and these fields can be used by filter rules.
   Using this view is no slower than the album table as these scalar fields are
   only calculated (slowing query) when field is in field list.
   */
}};
// clang-format on
} //unnamed namespace


bool CMusicQueryBuilder::GetAlbumsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                          const std::string& baseDir,
                                          CVariant& result,
                                          int& total,
                                          const SortDescription& sortDescription,
    CMusicDatabase& db)
{

  if (nullptr == db.m_pDB)
    return false;
  if (nullptr == db.m_pDS)
    return false;

  try
  {
    total = -1;

    size_t resultcount = 0;
    Filter extFilter;
    CMusicDbUrl musicUrl;
    // sorting passed into CMusicQueryBuilder::GetFilter() but not used as we only want to use the Const sortDescription
    // passed in at the start of the function
    SortDescription sorting = sortDescription;
    if (!musicUrl.FromString(baseDir) || !CMusicQueryBuilder::GetFilter(musicUrl, extFilter, sorting, db))
      return false;

    // Replace view names in filter with table names
    StringUtils::Replace(extFilter.where, "artistview", "artist");

    std::string strSQLExtra;
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count number of albums that satisfy selection criteria
    // (includes xsp limits from filter, but not sort limits)
    // Use albumview as filter rules in where clause may use scalar query fields
    total = db.GetSingleValueInt("SELECT COUNT(1) FROM albumview " + strSQLExtra, *db.m_pDS);
    resultcount = static_cast<size_t>(total);

    // Get order by (and any scalar query artist fields
    int iAddedFields = CMusicQueryBuilder::GetOrderFilter(MediaTypeAlbum, sortDescription, extFilter, db);

    // Grab calculated artist/title sort fields that may have been added to filter
    // These need to be added to the end of the album table field list
    std::string calcsortfieldsSQL = extFilter.fields;
    extFilter.fields.clear();

    std::string strSQL;

    // Setup fields to query, and album field number mapping
    // Find idArtist in JSONtoDBAlbum, offset of first join field
    int index_idArtist = -1;
    for (unsigned int i = 0; i < std::size(JSONtoDBAlbum); i++)
    {
      if (JSONtoDBAlbum[i].fieldDB == "idArtist")
      {
        index_idArtist = i;
        break;
      }
    }
    Filter joinFilter;
    DatasetLayout joinLayout(static_cast<size_t>(joinToAlbum_enumCount));
    extFilter.AppendField("albumview.idAlbum"); // ID "albumid" in JSON
    std::vector<int> dbfieldindex;
    // JSON "label" field is strAlbum which may also be requested as "title", query field once output twice
    extFilter.AppendField(JSONtoDBAlbum[0].fieldDB);
    if (fields.contains(JSONtoDBAlbum[0].fieldJSON))
      dbfieldindex.emplace_back(0); // Output "title"
    else
      dbfieldindex.emplace_back(-1); // fetch but not output

    // Check each optional album db field that could be retrieved (not label)
    for (unsigned int i = 1; i < std::size(JSONtoDBAlbum); i++)
    {
      bool foundJSON = fields.contains(JSONtoDBAlbum[i].fieldJSON);
      if (JSONtoDBAlbum[i].bSimple)
      {
        // Check for non-join fields in order too.
        // Query these in inline view (but not output) so can ref in outer order
        bool foundOrderby(false);
        if (!foundJSON)
          foundOrderby = extFilter.order.find(JSONtoDBAlbum[i].fieldDB) != std::string::npos;
        if (foundOrderby || foundJSON)
        {
          // Store indexes of requested album table and scalar subquery fields
          // to be output, and -1 when not output to JSON
          if (!foundJSON)
            dbfieldindex.emplace_back(-1);
          else
            dbfieldindex.emplace_back(i);
          if (!JSONtoDBAlbum[i].SQL.empty())
            // Field from scaler subquery
            extFilter.AppendField(db.PrepareSQL(JSONtoDBAlbum[i].SQL));
          else
            // Field from album table
            extFilter.AppendField(JSONtoDBAlbum[i].fieldDB);
        }
      }
      else if (foundJSON)
        // Field from join found in JSON request
        joinLayout.SetField(i - index_idArtist, JSONtoDBAlbum[i].SQL, true);
    }

    // Append calculated artist/title sort fields that may have been added to filter
    // Field used only for ORDER BY, not output to JSON
    extFilter.AppendField(calcsortfieldsSQL);
    for (int i = 0; i < iAddedFields; i++)
      dbfieldindex.emplace_back(-1); // columns in dataset

    // JOIN art tables if needed (fields output and/or in sort)
    if (extFilter.fields.find("art.") != std::string::npos)
    { // Left join as not all albums have art, but only have one thumb at most
      extFilter.AppendJoin("LEFT JOIN art ON art.media_id = idAlbum "
                           "AND art.media_type = 'album' AND art.type = 'thumb'");
    }

    // Build JOIN, WHERE, ORDER BY and LIMIT for inline view
    strSQLExtra = "";
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Add any LIMIT clause to strSQLExtra
    if (extFilter.limit.empty() && (sortDescription.limitStart > 0 || sortDescription.limitEnd > 0))
    {
      strSQLExtra +=
          DatabaseUtils::BuildLimitClause(sortDescription.limitEnd, sortDescription.limitStart);
      resultcount = std::min(
          DatabaseUtils::GetLimitCount(sortDescription.limitEnd, sortDescription.limitStart),
          resultcount);
    }

    // Setup multivalue JOINs, GROUP BY and ORDER BY
    bool bJoinAlbumArtist(false);
    if (sortDescription.sortBy != SortBy::RANDOM)
    {
      // Repeat inline view order (that always includes idAlbum) on join query
      std::string order = extFilter.order;
      StringUtils::Replace(order, "albumview.", "a1.");
      joinFilter.AppendOrder(order);
    }
    else
      joinFilter.AppendOrder("a1.idAlbum");
    joinFilter.AppendGroup("a1.idAlbum");
    // Album artists
    if (joinLayout.GetFetch(joinToAlbum_idArtist) || joinLayout.GetFetch(joinToAlbum_strArtist) ||
        joinLayout.GetFetch(joinToAlbum_strArtistMBID))
    { // All albums have at least one artist so inner join sufficient
      bJoinAlbumArtist = true;
      joinFilter.AppendJoin("JOIN album_artist ON album_artist.idAlbum = a1.idAlbum");
      joinFilter.AppendGroup("album_artist.idArtist");
      joinFilter.AppendOrder("album_artist.iOrder");
      // Ensure idArtist is queried
      if (!joinLayout.GetFetch(joinToAlbum_idArtist))
        joinLayout.SetField(joinToAlbum_idArtist,
                            JSONtoDBAlbum[index_idArtist + joinToAlbum_idArtist].SQL);
    }
    // artist table needed for strArtist or MBID
    // (album_artist.strArtist can be an alias or spelling variation)
    if (joinLayout.GetFetch(joinToAlbum_strArtist) ||
        joinLayout.GetFetch(joinToAlbum_strArtistMBID))
      joinFilter.AppendJoin("JOIN artist ON artist.idArtist = album_artist.idArtist");

    // Build JOIN part of query (if we have one)
    std::string strSQLJoin;
    if (joinLayout.HasFilterFields() && !db.BuildSQL(strSQLJoin, joinFilter, strSQLJoin))
      return false;

    // Adjust where in the results record the join fields are allowing for the
    // inline view fields (Quicker than finding field by name every time)
    // idAlbum + other album fields
    joinLayout.AdjustRecordNumbers(static_cast<int>(1 + dbfieldindex.size()));

    // Build full query
    // When have multiple value joins (artists or song genres) use inline view
    // SELECT a1.*, <join fields> FROM
    //   (SELECT <album fields> FROM albumview <where> + <order by> +  <limits> ) AS a1
    //   <joins> <group by> <order by> <joins order by>
    // Don't use prepareSQL - confuses  releasetype = 'album' filter and group_concat separator

    strSQL = "SELECT " + extFilter.fields + " FROM albumview " + strSQLExtra;
    if (joinLayout.HasFilterFields())
    {
      strSQL = "(" + strSQL + ") AS a1 ";
      strSQL = "SELECT a1.*, " + joinLayout.GetFields() + " FROM " + strSQL + strSQLJoin;
    }

    // Modify query to use correct year field
    if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
            CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE))
      StringUtils::Replace(strSQL, "<datefield>", "strReleaseDate");
    else
      StringUtils::Replace(strSQL, "<datefield>", "strOrigReleaseDate");

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    // run query
    auto start = std::chrono::steady_clock::now();

    if (!db.m_pDS->query(strSQL))
      return false;

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "query took {} ms", duration.count());

    int iRowsFound = db.m_pDS->num_rows();
    if (iRowsFound <= 0)
    {
      db.m_pDS->close();
      return true;
    }

    // Get albums from returned rows. Joins means there can be many rows per album
    int albumId = -1;
    int artistId = -1;
    CVariant albumObj;
    result["albums"].reserve(resultcount);
    while (!db.m_pDS->eof() || !albumObj.empty())
    {
      const dbiplus::sql_record* const record = db.m_pDS->get_sql_record();

      if (db.m_pDS->eof() || albumId != record->at(0).get_asInt())
      {
        // Store previous or last album
        if (!albumObj.empty())
        {
          // Split sources string into int array
          if (albumObj.isMember("sourceid"))
          {
            std::vector<std::string> sources =
                StringUtils::Split(albumObj["sourceid"].asString(), ";");
            albumObj["sourceid"] = CVariant(CVariant::VariantTypeArray);
            for (const auto& source : sources)
              albumObj["sourceid"].append(std::atoi(source.c_str()));
          }
          result["albums"].append(albumObj);
          albumObj.clear();
          artistId = -1;
        }
        if (db.m_pDS->eof())
          continue; // Having saved last album stop

        // New album
        albumId = record->at(0).get_asInt();
        albumObj["albumid"] = albumId;
        albumObj["label"] = record->at(1).get_asString();
        for (size_t i = 0; i < dbfieldindex.size(); i++)
        {
          if (dbfieldindex[i] > -1)
          {
            if (JSONtoDBAlbum[dbfieldindex[i]].fieldDB == "songgenres")
            {
              // Convert "20,Jazz,54,New Age,65,Rock" into array of objects
              std::vector<std::string> values =
                  StringUtils::Split(record->at(1 + i).get_asString(), ",");
              if (values.size() % 2 == 0) // Must contain an even number of entries
              {
                for (size_t j = 0; j + 1 < values.size(); j += 2)
                {
                  int idGenre = atoi(values[j].c_str());
                  if (idGenre > 0)
                  {
                    CVariant genreObj;
                    genreObj["genreid"] = idGenre;
                    genreObj["title"] = values[j + 1];
                    albumObj["songgenres"].append(genreObj);
                  }
                }
              }
              // Ensure albums with null songgenres get empty array
              if (!albumObj.isMember("songgenres"))
                albumObj["songgenres"] = CVariant(CVariant::VariantTypeArray);
            }
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "integer")
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asInt();
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "unsigned")
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] =
                  std::max(record->at(1 + i).get_asInt(), 0);
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "float")
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] =
                  std::max(record->at(1 + i).get_asFloat(), 0.f);
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "array")
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] = StringUtils::Split(
                  record->at(1 + i).get_asString(), CServiceBroker::GetSettingsComponent()
                                                        ->GetAdvancedSettings()
                                                        ->m_musicItemSeparator);
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "boolean")
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asBool();
            else if (JSONtoDBAlbum[dbfieldindex[i]].formatJSON == "image")
            {
              std::string url = record->at(1 + i).get_asString();
              if (!url.empty())
                url = IMAGE_FILES::URLFromFile(url);
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] = url;
            }
            else
              albumObj[JSONtoDBAlbum[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asString();
          }
        }
      }
      if (bJoinAlbumArtist && joinLayout.GetRecNo(joinToAlbum_idArtist) > -1 &&
          artistId != record->at(joinLayout.GetRecNo(joinToAlbum_idArtist)).get_asInt())
      {
        artistId = record->at(joinLayout.GetRecNo(joinToAlbum_idArtist)).get_asInt();
        if (joinLayout.GetOutput(joinToAlbum_idArtist))
          albumObj["artistid"].append(artistId);
        if (artistId == BLANKARTIST_ID)
        {
          if (joinLayout.GetOutput(joinToAlbum_strArtist))
            albumObj["artist"].append(StringUtils::Empty);
          if (joinLayout.GetOutput(joinToAlbum_strArtistMBID))
            albumObj["musicbrainzalbumartistid"].append(StringUtils::Empty);
        }
        else
        {
          if (joinLayout.GetOutput(joinToAlbum_strArtist) &&
              joinLayout.GetRecNo(joinToAlbum_strArtist) > -1)
            albumObj["artist"].append(
                record->at(joinLayout.GetRecNo(joinToAlbum_strArtist)).get_asString());
          if (joinLayout.GetOutput(joinToAlbum_strArtistMBID) &&
              joinLayout.GetRecNo(joinToAlbum_strArtistMBID) > -1)
            albumObj["musicbrainzalbumartistid"].append(
                record->at(joinLayout.GetRecNo(joinToAlbum_strArtistMBID)).get_asString());
        }
      }
      db.m_pDS->next();
    }
    db.m_pDS->close(); // cleanup recordset data

    // Ensure random order of output when results set is sorted to process multi-value joins
    if (sortDescription.sortBy == SortBy::RANDOM && joinLayout.HasFilterFields())
      KODI::UTILS::RandomShuffle(result["albums"].begin_array(), result["albums"].end_array());

    return true;
  }
  catch (...)
  {
    db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}


namespace
{
// clang-format off
const std::array<TranslateJSONField, 54> JSONtoDBSong = {{
  // table and single value join fields
  { "title",                     "string", true,  "strTitle",               "" }, // Label field at top
  { "albumid",                  "integer", true,  "song.idAlbum",           "" },
  { "",                                "", true,  "song.iTrack",            "" },
  { "displayartist",             "string", true,  "song.strArtistDisp",     "" },
  { "sortartist",                "string", true,  "song.strArtistSort",     "" },
  { "genre",                      "array", true,  "song.strGenres",         "" },
  { "duration",                 "integer", true,  "iDuration",              "" },
  { "comment",                   "string", true,  "comment",                "" },
  { "",                          "string", true,  "strFileName",            "" },
  { "musicbrainztrackid",        "string", true,  "strMusicBrainzTrackID",  "" },
  { "playcount",                "integer", true,  "iTimesPlayed",           "" },
  { "lastplayed",                "string", true,  "lastPlayed",             "" },
  { "rating",                     "float", true,  "rating",                 "" },
  { "votes",                    "integer", true,  "votes",                  "" },
  { "userrating",              "unsigned", true,  "song.userrating",        "" },
  { "mood",                       "array", true,  "mood",                   "" },
  { "dateadded",                 "string", true,  "song.dateAdded",         "" },
  { "datenew",                   "string", true,  "song.dateNew",           "" },
  { "datemodified",              "string", true,  "song.dateModified",      "" },
  { "file",                      "string", true,  "strPathFile",            "CONCAT(path.strPath, strFilename) AS strPathFile" },
  { "",                          "string", true,  "strPath",                "path.strPath AS strPath" },
  { "album",                     "string", true,  "strAlbum",               "album.strAlbum AS strAlbum" },
  { "albumreleasetype",          "string", true,  "strAlbumReleaseType",    "album.strReleaseType AS strAlbumReleaseType" },
  { "musicbrainzalbumid",        "string", true,  "strMusicBrainzAlbumID",  "album.strMusicBrainzAlbumID AS strMusicBrainzAlbumID" },
  { "disctitle",                 "string", true,  "song.strDiscSubtitle",   "" },
  { "bpm",                      "integer", true,  "iBPM",                   "" },
  { "originaldate",             "string" , true,  "song.strOrigReleaseDate","" },
  { "releasedate",              "string" , true,  "song.strReleaseDate",    "" },
  { "bitrate",                  "integer", true,  "iBitRate",               "" },
  { "samplerate",               "integer", true,  "iSampleRate",            "" },
  { "channels",                 "integer", true,  "iChannels",              "" },
  { "songvideourl",              "string", true,  "strVideoURL",            "" },

  // JOIN fields (multivalue), same order as _JoinToSongFields
  { "albumartistid",              "array", false, "idAlbumArtist",          "album_artist.idArtist AS idAlbumArtist" },
  { "albumartist",                "array", false, "strAlbumArtist",         "albumartist.strArtist AS strAlbumArtist" },
  { "musicbrainzalbumartistid",   "array", false, "strAlbumArtistMBID",     "albumartist.strMusicBrainzArtistID AS strAlbumArtistMBID" },
  { "",                                "", false, "iOrderAlbumArtist",      "album_artist.iOrder AS iOrderAlbumArtist" },
  { "artistid",                   "array", false, "idArtist",               "song_artist.idArtist AS idArtist" },
  { "artist",                     "array", false, "strArtist",              "songartist.strArtist AS strArtist" },
  { "musicbrainzartistid",        "array", false, "strArtistMBID",          "songartist.strMusicBrainzArtistID AS strArtistMBID" },
  { "",                                "", false, "iOrderArtist",           "song_artist.iOrder AS iOrderArtist" },
  { "",                                "", false, "idRole",                 "song_artist.idRole" },
  { "",                                "", false, "strRole",                "role.strRole" },
  { "",                                "", false, "iOrderRole",             "song_artist.iOrder AS iOrderRole" },
  { "genreid",                    "array", false, "idGenre",                "song_genre.idGenre AS idGenre" }, // Not GROUP_CONCAT as can't control order
  { "",                                "", false, "iOrderGenre",            "song_genre.idOrder AS iOrderGenre" },

  { "contributors",               "array", false, "Role_All",               "song_artist.idRole AS Role_All" },
  { "displaycomposer",           "string", false, "Role_Composer",          "song_artist.idRole AS Role_Composer" },
  { "displayconductor",          "string", false, "Role_Conductor",         "song_artist.idRole AS Role_Conductor" },
  { "displayorchestra",          "string", false, "Role_Orchestra",         "song_artist.idRole AS Role_Orchestra" },
  { "displaylyricist",           "string", false, "Role_Lyricist",          "song_artist.idRole AS Role_Lyricist" },

  // Scalar subquery fields
  { "year",                     "integer", true,  "iYear",                  "CAST(<datefield> AS INTEGER) AS iYear" }, //From strReleaseDate or strOrigReleaseDate
  { "track",                    "integer", true,  "track",                  "(iTrack & 0xffff) AS track" },
  { "disc",                     "integer", true,  "disc",                   "(iTrack >> 16) AS disc" },
  { "sourceid",                  "string", true,  "sourceid",               "(SELECT GROUP_CONCAT(album_source.idSource SEPARATOR '; ') FROM album_source WHERE album_source.idAlbum = song.idAlbum) AS sources" },
  /*
   Song "thumbnail", "fanart" and "art" fields of JSON schema are fetched using
   thumbloader and separate queries to allow for fallback strategy
   "lyrics"?? Can be set for an item (by addons) but not held in db so
   AudioLibrary.GetSongs() never fills this field despite being in schema

   FROM ( SELECT * FROM song
   JOIN album ON album.idAlbum = song.idAlbum
   JOIN path ON path.idPath = song.idPath) AS sv
   JOIN album_artist ON album_artist.idAlbum = song.idAlbum
   JOIN artist AS albumartist ON albumartist.idArtist = album_artist.idArtist
   JOIN song_artist ON song_artist.idSong = song.idSong
   JOIN artist AS artistsong ON artistsong.idArtist  = song_artist.idArtist
   JOIN role ON song_artist.idRole = role.idRole
   LEFT JOIN song_genre ON song.idSong = song_genre.idSong

   */
}};
// clang-format on
} // unnamed namespace


bool CMusicQueryBuilder::GetSongsByWhereJSON(
    const std::set<std::string, std::less<>>& fields,
    const std::string& baseDir,
    CVariant& result,
    int& total,
    const SortDescription& sortDescription,
    CMusicDatabase& db)
{

  if (nullptr == db.m_pDB)
    return false;
  if (nullptr == db.m_pDS)
    return false;

  try
  {
    total = -1;

    size_t resultcount = 0;
    Filter extFilter;
    CMusicDbUrl musicUrl;
    // sorting passed into CMusicQueryBuilder::GetFilter() but not used as we only want to use the Const sortDescription
    // passed into the function
    SortDescription sorting = sortDescription;
    if (!musicUrl.FromString(baseDir) || !CMusicQueryBuilder::GetFilter(musicUrl, extFilter, sorting, db))
      return false;

    // Replace view names in filter with table names
    StringUtils::Replace(extFilter.where, "artistview", "artist");
    StringUtils::Replace(extFilter.where, "albumview", "album");
    StringUtils::Replace(extFilter.where, "songview.strPath", "strPath");
    StringUtils::Replace(extFilter.where, "songview.strAlbum", "strAlbum");
    StringUtils::Replace(extFilter.where, "songview", "song");
    StringUtils::Replace(extFilter.where, "songartistview", "song_artist");

    // JOIN album and path tables needed by filter rules in where clause
    if (extFilter.where.find("album.") != std::string::npos ||
        extFilter.where.find("strAlbum") != std::string::npos)
    { // All songs have one album so inner join sufficient
      extFilter.AppendJoin("JOIN album ON album.idAlbum = song.idAlbum");
    }
    if (extFilter.where.find("strPath") != std::string::npos)
    { // All songs have one path so inner join sufficient
      extFilter.AppendJoin("JOIN path ON path.idPath = song.idPath");
    }

    // Build JOINs and WHERE needed by filter for counting songs
    std::string strSQLExtra;
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Count number of songs that satisfy selection criteria
    // (includes xsp limits from filter, but not sort limits)
    total = db.GetSingleValueInt("SELECT COUNT(1) FROM song " + strSQLExtra, *db.m_pDS);
    resultcount = static_cast<size_t>(total);

    int iAddedFields = CMusicQueryBuilder::GetOrderFilter(MediaTypeSong, sortDescription, extFilter, db);
    // Replace songview field names in order by with song, album path table field names
    // Field names in album same as song:
    //   idAlbum, strArtistDisp, strArtistSort, strGenres, iYear, bCompilation
    StringUtils::Replace(extFilter.order, "songview.strPath", "strPath");
    StringUtils::Replace(extFilter.order, "songview.strAlbum", "strAlbum");
    StringUtils::Replace(extFilter.order, "songview.bCompilation", "album.bCompilation");
    StringUtils::Replace(extFilter.order, "songview.strArtists", "song.strArtistDisp");
    StringUtils::Replace(extFilter.order, "songview.strAlbumArtists", "album.strArtistDisp");
    StringUtils::Replace(extFilter.order, "songview.strAlbumArtistSort", "album.strArtistSort");
    StringUtils::Replace(extFilter.order, "songview.strAlbumReleaseType", "strReleaseType");
    StringUtils::Replace(extFilter.order, "songview", "song");
    StringUtils::Replace(extFilter.fields, " strArtistSort", " song.strArtistSort");
    StringUtils::Replace(extFilter.fields, "songview.strArtists", "song.strArtistDisp");
    StringUtils::Replace(extFilter.fields, "songview.strAlbum", "strAlbum");
    StringUtils::Replace(extFilter.fields, "songview.strTitle", "strTitle");

    // Grab calculated artist/title sort fields that may have been added to filter
    // These need to be added to the end of the song table field list
    std::string calcsortfieldsSQL = extFilter.fields;
    extFilter.fields.clear();

    std::string strSQL;

    // Setup fields to query, and song field number mapping
    // Find idAlbumArtist in JSONtoDBSong, offset of first join field
    int index_idAlbumArtist = -1;
    for (unsigned int i = 0; i < std::size(JSONtoDBSong); i++)
    {
      if (JSONtoDBSong[i].fieldDB == "idAlbumArtist")
      {
        index_idAlbumArtist = i;
        break;
      }
    }
    Filter joinFilter;
    DatasetLayout joinLayout(static_cast<size_t>(joinToSongs_enumCount));
    extFilter.AppendField("song.idSong"); // ID "songid" in JSON
    std::vector<int> dbfieldindex;
    // JSON "label" field is strTitle which may also be requested as "title", query field once output twice
    extFilter.AppendField(JSONtoDBSong[0].fieldDB);
    if (fields.contains(JSONtoDBSong[0].fieldJSON))
      dbfieldindex.emplace_back(0); // Output "title"
    else
      dbfieldindex.emplace_back(-1); // Fetch but not output
    std::vector<std::string> rolefieldlist;
    std::vector<int> roleidlist;
    // Check each optional db field that could be retrieved (not label)
    for (unsigned int i = 1; i < std::size(JSONtoDBSong); i++)
    {
      bool foundJSON = fields.contains(JSONtoDBSong[i].fieldJSON);
      if (JSONtoDBSong[i].bSimple)
      {
        // Check for non-join fields in order too.
        // Query these in inline view (but not output) so can ref in outer order
        bool foundOrderby(false);
        if (!foundJSON)
          foundOrderby = extFilter.order.find(JSONtoDBSong[i].fieldDB) != std::string::npos;
        if (foundOrderby || foundJSON)
        {
          // Store indexes of requested album table and scalar subquery fields
          // to be output, and -1 when not output to JSON
          if (!foundJSON)
            dbfieldindex.emplace_back(-1);
          else
            dbfieldindex.emplace_back(i);
          if (!JSONtoDBSong[i].SQL.empty())
            // Field from scaler subquery
            extFilter.AppendField(db.PrepareSQL(JSONtoDBSong[i].SQL));
          else
            // Field from song table
            extFilter.AppendField(JSONtoDBSong[i].fieldDB);
        }
      }
      else if (foundJSON)
      { // Field from join found in JSON request
        if (!StringUtils::StartsWith(JSONtoDBSong[i].fieldDB, "Role_"))
        {
          joinLayout.SetField(i - index_idAlbumArtist, JSONtoDBSong[i].SQL, true);
        }
        else
        { // "contributors", "displaycomposer" etc.
          rolefieldlist.emplace_back(JSONtoDBSong[i].fieldJSON);
        }
      }
    }
    // Append calculated artist/title sort fields that may have been added to filter
    // Field used only for ORDER BY, not output to JSON
    extFilter.AppendField(calcsortfieldsSQL);
    for (int i = 0; i < iAddedFields; i++)
      dbfieldindex.emplace_back(-1); // columns in dataset

    // Build matching list of role id for "displaycomposer", "displayconductor",
    // "displayorchestra", "displaylyricist"
    if (!rolefieldlist.empty())
    {
      for (const auto& name : rolefieldlist)
      {
        int idRole = -1;
        if (StringUtils::StartsWith(name, "display"))
          idRole = db.GetRoleByName(name.substr(7));
        roleidlist.emplace_back(idRole);
      }
    }

    // JOIN album and path tables needed for field output and/or in sort
    // if not already there for filter
    if ((extFilter.fields.find("album.") != std::string::npos ||
         extFilter.fields.find("strAlbum") != std::string::npos) &&
        extFilter.join.find("JOIN album") == std::string::npos)
    { // All songs have one album so inner join sufficient
      extFilter.AppendJoin("JOIN album ON album.idAlbum = song.idAlbum");
    }
    if (extFilter.fields.find("path.") != std::string::npos &&
        extFilter.join.find("JOIN path") == std::string::npos)
    { // All songs have one path so inner join sufficient
      extFilter.AppendJoin("JOIN path ON path.idPath = song.idPath");
    }

    // Build JOIN, WHERE, ORDER BY and LIMIT for inline view
    strSQLExtra = "";
    if (!db.BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    // Add any LIMIT clause to strSQLExtra
    if (extFilter.limit.empty() && (sortDescription.limitStart > 0 || sortDescription.limitEnd > 0))
    {
      strSQLExtra +=
          DatabaseUtils::BuildLimitClause(sortDescription.limitEnd, sortDescription.limitStart);
      resultcount = std::min(
          DatabaseUtils::GetLimitCount(sortDescription.limitEnd, sortDescription.limitStart),
          resultcount);
    }

    // Setup multivalue JOINs, GROUP BY and ORDER BY
    bool bJoinSongArtist(false);
    bool bJoinAlbumArtist(false);
    bool bJoinRole(false);
    if (sortDescription.sortBy != SortBy::RANDOM)
    {
      // Repeat inline view order (that always includes idSong) on join query
      std::string order = extFilter.order;
      order = extFilter.order;
      StringUtils::Replace(order, "album.", "sv.");
      StringUtils::Replace(order, "song.", "sv.");
      joinFilter.AppendOrder(order);
    }
    else
      joinFilter.AppendOrder("sv.idSong");
    joinFilter.AppendGroup("sv.idSong");

    // Album artists
    if (joinLayout.GetFetch(joinToSongs_idAlbumArtist) ||
        joinLayout.GetFetch(joinToSongs_strAlbumArtist) ||
        joinLayout.GetFetch(joinToSongs_strAlbumArtistMBID))
    { // All songs have at least one album artist so inner join sufficient
      bJoinAlbumArtist = true;
      joinFilter.AppendJoin("JOIN album_artist ON album_artist.idAlbum = sv.idAlbum");
      joinFilter.AppendGroup("album_artist.idArtist");
      joinFilter.AppendOrder("album_artist.iOrder");
      // Ensure idAlbumArtist is queried for processing repeats
      if (!joinLayout.GetFetch(joinToSongs_idAlbumArtist))
      {
        joinLayout.SetField(joinToSongs_idAlbumArtist,
                            JSONtoDBSong[index_idAlbumArtist + joinToSongs_idAlbumArtist].SQL);
      }
      // Ensure song.IdAlbum is field of the inline view for join
      if (!fields.contains("albumid"))
      {
        extFilter.AppendField("song.idAlbum"); //Prefer lookup JSONtoDBSong[XXX].dbField);
        dbfieldindex.emplace_back(-1);
      }
      // artist table needed for strArtist or MBID
      // (album_artist.strArtist can be an alias or spelling variation)
      if (joinLayout.GetFetch(joinToSongs_strAlbumArtistMBID) ||
          joinLayout.GetFetch(joinToSongs_strAlbumArtist))
        joinFilter.AppendJoin(
            "JOIN artist AS albumartist ON albumartist.idArtist = album_artist.idArtist");
    }

    /*
     Song artists
     JSON schema "artist", "artistid", "musicbrainzartistid", "contributors",
     "displaycomposer", "displayconductor", "displayorchestra", "displaylyricist",
    */
    if (joinLayout.GetFetch(joinToSongs_idArtist) || joinLayout.GetFetch(joinToSongs_strArtist) ||
        joinLayout.GetFetch(joinToSongs_strArtistMBID) || !rolefieldlist.empty())
    { // All songs have at least one artist (idRole = 1) so inner join sufficient
      bJoinSongArtist = true;
      if (rolefieldlist.empty())
      { // song artists only, no other roles needed
        joinFilter.AppendJoin(
            "JOIN song_artist ON song_artist.idSong = sv.idSong AND song_artist.idRole = 1");
        joinFilter.AppendGroup("song_artist.idArtist");
        joinFilter.AppendOrder("song_artist.iOrder");
      }
      else
      {
        // Ensure idRole is queried
        if (!joinLayout.GetFetch(joinToSongs_idRole))
        {
          joinLayout.SetField(joinToSongs_idRole,
                              JSONtoDBSong[index_idAlbumArtist + joinToSongs_idRole].SQL);
        }
        // Ensure strArtist is queried
        if (!joinLayout.GetFetch(joinToSongs_strArtist))
        {
          joinLayout.SetField(joinToSongs_strArtist,
                              JSONtoDBSong[index_idAlbumArtist + joinToSongs_strArtist].SQL);
        }
        if (fields.contains("contributors"))
        { // all roles
          bJoinRole = true;
          // Ensure strRole is queried from role table
          joinLayout.SetField(joinToSongs_strRole, "role.strRole");
          joinFilter.AppendJoin("JOIN song_artist ON song_artist.idSong = sv.idSong");
          joinFilter.AppendJoin("JOIN role ON song_artist.idRole = role.idRole");
          joinFilter.AppendGroup("song_artist.idArtist, song_artist.idRole");
          joinFilter.AppendOrder("song_artist.idRole, song_artist.iOrder, song_artist.idArtist");
        }
        else
        { // Get just roles for  "displaycomposer", "displayconductor" etc.
          std::string where;
          for (int idRole : roleidlist)
          {
            if (idRole <= 1)
              continue;
            if (where.empty())
              // Always get song artists too (role = 1) so can do inner join
              where = db.PrepareSQL("song_artist.idRole = 1 OR song_artist.idRole = %i", idRole);
            else
              where += db.PrepareSQL(" OR song_artist.idRole = %i", idRole);
          }
          where = " (" + where + ")";
          joinFilter.AppendJoin("JOIN song_artist ON song_artist.idSong = sv.idSong AND " + where);
          joinFilter.AppendGroup("song_artist.idArtist, song_artist.idRole");
          joinFilter.AppendOrder("song_artist.idRole, song_artist.iOrder, song_artist.idArtist");
        }
      }
      // Ensure idArtist is queried for processing repeats
      if (!joinLayout.GetFetch(joinToSongs_idArtist))
      {
        joinLayout.SetField(joinToSongs_idArtist,
                            JSONtoDBSong[index_idAlbumArtist + joinToSongs_idArtist].SQL);
      }
      // artist table needed for strArtist or MBID
      // (song_artist.strArtist can be an alias or spelling variation)
      if (joinLayout.GetFetch(joinToSongs_strArtistMBID) ||
          joinLayout.GetFetch(joinToSongs_strArtist))
        joinFilter.AppendJoin(
            "JOIN artist AS songartist ON songartist.idArtist = song_artist.idArtist");
    }

    // Genre ids
    if (joinLayout.GetFetch(joinToSongs_idGenre))
    { // song genre ids (strGenre demormalised in song table)
      // Left join as songs may not have genre
      joinFilter.AppendJoin("LEFT JOIN song_genre ON song_genre.idSong = sv.idSong");
      joinFilter.AppendGroup("song_genre.idGenre");
      joinFilter.AppendOrder("song_genre.iOrder");
    }

    // Build JOIN part of query (if we have one)
    std::string strSQLJoin;
    if (joinLayout.HasFilterFields() && !db.BuildSQL(strSQLJoin, joinFilter, strSQLJoin))
      return false;

    // Adjust where in the results record the join fields are allowing for the
    // inline view fields (Quicker than finding field by name every time)
    // idSong + other song fields
    joinLayout.AdjustRecordNumbers(static_cast<int>(1 + dbfieldindex.size()));

    // Build full query
    // When have multiple value joins use inline view
    // SELECT sv.*, <join fields> FROM
    //   (SELECT <song fields> FROM song <JOIN album> <where> + <order by> +  <limits> ) AS sv
    //   <joins> <group by>
    //   <order by> + <joins order by>
    // Don't use prepareSQL - confuses  releasetype = 'album' filter and group_concat separator
    strSQL = "SELECT " + extFilter.fields + " FROM song " + strSQLExtra;
    if (joinLayout.HasFilterFields())
    {
      strSQL = "(" + strSQL + ") AS sv ";
      strSQL = "SELECT sv.*, " + joinLayout.GetFields() + " FROM " + strSQL + strSQLJoin;
    }

    // Modify query to use correct year field
    if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
            CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE))
      StringUtils::Replace(strSQL, "<datefield>", "song.strReleaseDate");
    else
      StringUtils::Replace(strSQL, "<datefield>", "song.strOrigReleaseDate");

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);

    // Run query
    auto start = std::chrono::steady_clock::now();

    if (!db.m_pDS->query(strSQL))
      return false;

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "query took {} ms", duration.count());

    int iRowsFound = db.m_pDS->num_rows();
    if (iRowsFound <= 0)
    {
      db.m_pDS->close();
      return true;
    }

    // Get song from returned rows. Joins mean there can be many rows per song
    int songId = -1;
    int albumartistId = -1;
    int artistId = -1;
    int roleId = -1;
    bool bSongGenreDone(false);
    bool bSongArtistDone(false);
    bool bHaveSong(false);
    CVariant songObj;
    result["songs"].reserve(resultcount);
    while (!db.m_pDS->eof() || bHaveSong)
    {
      const dbiplus::sql_record* const record = db.m_pDS->get_sql_record();

      if (db.m_pDS->eof() || songId != record->at(0).get_asInt())
      {
        // Store previous or last song
        if (bHaveSong)
        {
          // Check empty role fields get returned, and format
          if (!rolefieldlist.empty())
          {
            for (const auto& displayXXX : rolefieldlist)
            {
              if (!StringUtils::StartsWith(displayXXX, "display"))
              {
                // "contributors"
                if (!songObj.isMember(displayXXX))
                  songObj[displayXXX] = CVariant(CVariant::VariantTypeArray);
              }
              else if (songObj.isMember(displayXXX) && songObj[displayXXX].isArray())
              {
                // Convert "displaycomposer", "displayconductor", "displayorchestra",
                // and "displaylyricist" arrays into strings
                std::vector<std::string> names;
                for (CVariant::const_iterator_array field = songObj[displayXXX].begin_array();
                     field != songObj[displayXXX].end_array(); ++field)
                  names.emplace_back(field->asString());

                std::string role = StringUtils::Join(names, CServiceBroker::GetSettingsComponent()
                                                                ->GetAdvancedSettings()
                                                                ->m_musicItemSeparator);
                songObj[displayXXX] = role;
              }
              else
                songObj[displayXXX] = "";
            }
          }
          result["songs"].append(songObj);
          bHaveSong = false;
          songObj.clear();
        }
        if (songObj.empty())
        {
          // Initialise fields, ensure those with possible null values are set to correct empty variant type
          if (joinLayout.GetOutput(joinToSongs_idGenre))
            songObj["genreid"] =
                CVariant(CVariant::VariantTypeArray); //"genre" set [] by split of array

          albumartistId = -1;
          artistId = -1;
          roleId = -1;
          bSongGenreDone = false;
          bSongArtistDone = false;
        }
        if (db.m_pDS->eof())
          continue; // Having saved the last song stop

        // New song
        songId = record->at(0).get_asInt();
        bHaveSong = true;
        songObj["songid"] = songId;
        songObj["label"] = record->at(1).get_asString();
        for (size_t i = 0; i < dbfieldindex.size(); i++)
          if (dbfieldindex[i] > -1)
          {
            if (JSONtoDBSong[dbfieldindex[i]].formatJSON == "integer")
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asInt();
            else if (JSONtoDBSong[dbfieldindex[i]].formatJSON == "unsigned")
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] =
                  std::max(record->at(1 + i).get_asInt(), 0);
            else if (JSONtoDBSong[dbfieldindex[i]].formatJSON == "float")
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] =
                  std::max(record->at(1 + i).get_asFloat(), 0.f);
            else if (JSONtoDBSong[dbfieldindex[i]].formatJSON == "array")
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] = StringUtils::Split(
                  record->at(1 + i).get_asString(), CServiceBroker::GetSettingsComponent()
                                                        ->GetAdvancedSettings()
                                                        ->m_musicItemSeparator);
            else if (JSONtoDBSong[dbfieldindex[i]].formatJSON == "boolean")
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asBool();
            else
              songObj[JSONtoDBSong[dbfieldindex[i]].fieldJSON] = record->at(1 + i).get_asString();
          }

        // Split sources string into int array
        if (songObj.isMember("sourceid"))
        {
          std::vector<std::string> sources =
              StringUtils::Split(songObj["sourceid"].asString(), ";");
          songObj["sourceid"] = CVariant(CVariant::VariantTypeArray);
          for (const auto& source : sources)
            songObj["sourceid"].append(std::atoi(source.c_str()));
        }
      }

      if (bJoinAlbumArtist)
      {
        if (albumartistId != record->at(joinLayout.GetRecNo(joinToSongs_idAlbumArtist)).get_asInt())
        {
          bSongGenreDone =
              bSongGenreDone || (albumartistId > 0); // Not first album artist, skip genre
          bSongArtistDone =
              bSongArtistDone || (albumartistId > 0); // Not first album artist, skip song artists
          albumartistId = record->at(joinLayout.GetRecNo(joinToSongs_idAlbumArtist)).get_asInt();
          if (joinLayout.GetOutput(joinToSongs_idAlbumArtist))
            songObj["albumartistid"].append(albumartistId);
          if (albumartistId == BLANKARTIST_ID)
          {
            if (joinLayout.GetOutput(joinToSongs_strAlbumArtist))
              songObj["albumartist"].append(StringUtils::Empty);
            if (joinLayout.GetOutput(joinToSongs_strAlbumArtistMBID))
              songObj["musicbrainzalbumartistid"].append(StringUtils::Empty);
          }
          else
          {
            if (joinLayout.GetOutput(joinToSongs_idAlbumArtist))
              songObj["albumartistid"].append(albumartistId);
            if (joinLayout.GetOutput(joinToSongs_strAlbumArtist))
              songObj["albumartist"].append(
                  record->at(joinLayout.GetRecNo(joinToSongs_strAlbumArtist)).get_asString());
            if (joinLayout.GetOutput(joinToSongs_strAlbumArtistMBID))
              songObj["musicbrainzalbumartistid"].append(
                  record->at(joinLayout.GetRecNo(joinToSongs_strAlbumArtistMBID)).get_asString());
          }
        }
      }
      if (bJoinSongArtist && !bSongArtistDone)
      {
        if (artistId != record->at(joinLayout.GetRecNo(joinToSongs_idArtist)).get_asInt())
        {
          bSongGenreDone = bSongGenreDone || (artistId > 0); // Not first artist, skip genre
          roleId = -1; // Allow for many artists same role
          artistId = record->at(joinLayout.GetRecNo(joinToSongs_idArtist)).get_asInt();
          if (joinLayout.GetRecNo(joinToSongs_idRole) < 0 ||
              record->at(joinLayout.GetRecNo(joinToSongs_idRole)).get_asInt() == 1)
          {
            if (joinLayout.GetOutput(joinToSongs_idArtist))
              songObj["artistid"].append(artistId);
            if (artistId == BLANKARTIST_ID)
            {
              if (joinLayout.GetOutput(joinToSongs_strArtist))
                songObj["artist"].append(StringUtils::Empty);
              if (joinLayout.GetOutput(joinToSongs_strArtistMBID))
                songObj["musicbrainzartistid"].append(StringUtils::Empty);
            }
            else
            {
              if (joinLayout.GetOutput(joinToSongs_strArtist))
                songObj["artist"].append(
                    record->at(joinLayout.GetRecNo(joinToSongs_strArtist)).get_asString());
              if (joinLayout.GetOutput(joinToSongs_strArtistMBID))
                songObj["musicbrainzartistid"].append(
                    record->at(joinLayout.GetRecNo(joinToSongs_strArtistMBID)).get_asString());
            }
          }
        }
        if (joinLayout.GetRecNo(joinToSongs_idRole) > 0 &&
            roleId != record->at(joinLayout.GetRecNo(joinToSongs_idRole)).get_asInt())
        {
          bSongGenreDone = bSongGenreDone || (roleId > 0); // Not first role, skip genre
          roleId = record->at(joinLayout.GetRecNo(joinToSongs_idRole)).get_asInt();
          if (roleId > 1)
          {
            if (bJoinRole)
            { //Contributors
              CVariant contributor;
              contributor["name"] =
                  record->at(joinLayout.GetRecNo(joinToSongs_strArtist)).get_asString();
              contributor["role"] =
                  record->at(joinLayout.GetRecNo(joinToSongs_strRole)).get_asString();
              contributor["roleid"] = roleId;
              contributor["artistid"] =
                  record->at(joinLayout.GetRecNo(joinToSongs_idArtist)).get_asInt();
              songObj["contributors"].append(contributor);
            }
            // "displaycomposer", "displayconductor" etc.
            for (size_t i = 0; i < roleidlist.size(); i++)
            {
              if (roleidlist[i] == roleId)
              {
                songObj[rolefieldlist[i]].append(
                    record->at(joinLayout.GetRecNo(joinToSongs_strArtist)).get_asString());
                continue;
              }
            }
          }
        }
      }
      if (!bSongGenreDone && joinLayout.GetRecNo(joinToSongs_idGenre) > -1 &&
          !record->at(joinLayout.GetRecNo(joinToSongs_idGenre)).get_isNull())
      {
        songObj["genreid"].append(record->at(joinLayout.GetRecNo(joinToSongs_idGenre)).get_asInt());
      }
      db.m_pDS->next();
    }
    db.m_pDS->close(); // cleanup recordset data

    // Ensure random order of output when results set is sorted to process multi-value joins
    if (sortDescription.sortBy == SortBy::RANDOM && joinLayout.HasFilterFields())
      KODI::UTILS::RandomShuffle(result["songs"].begin_array(), result["songs"].end_array());

    return true;
  }
  catch (...)
  {
    db.m_pDS->close();
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}


std::string CMusicQueryBuilder::GetIgnoreArticleSQL(const std::string& strField,
                                         const CMusicDatabase& db)
{
  /*
  Make SQL clause from ignore article list.
  Group tokens the same length together, for example :
    WHEN strArtist LIKE 'the ' OR strArtist LIKE 'the.' strArtist LIKE 'the_' ESCAPE '_'
    THEN SUBSTR(strArtist, 5)
    WHEN strArtist LIKE 'an ' OR strArtist LIKE 'an.' strArtist LIKE 'an_' ESCAPE '_'
    THEN SUBSTR(strArtist, 4)
  */
  const CLangInfo::Tokens sortTokens = g_langInfo.GetSortTokens();
  std::string sortclause;
  size_t tokenlength = 0;
  std::string strWhen;
  for (const auto& token : sortTokens)
  {
    if (token.length() != tokenlength)
    {
      if (!strWhen.empty())
      {
        if (!sortclause.empty())
          sortclause += " ";
        std::string strThen = db.PrepareSQL(" THEN SUBSTR(%s, %i)", strField.c_str(), tokenlength + 1);
        sortclause += "WHEN " + strWhen + strThen;
        strWhen.clear();
      }
      tokenlength = token.length();
    }
    std::string tokenclause = token;
    //Escape any ' or % in the token
    StringUtils::Replace(tokenclause, "'", "''");
    StringUtils::Replace(tokenclause, "%", "%%");
    // Single %, _ and ' so avoid using PrepareSQL
    tokenclause = strField + " LIKE '" + tokenclause + "%'";
    if (token.find('_') != std::string::npos)
      tokenclause += " ESCAPE '_'";
    if (!strWhen.empty())
      strWhen += " OR ";
    strWhen += tokenclause;
  }
  if (!strWhen.empty())
  {
    if (!sortclause.empty())
      sortclause += " ";
    std::string strThen = db.PrepareSQL(" THEN SUBSTR(%s, %i)", strField.c_str(), tokenlength + 1);
    sortclause += "WHEN " + strWhen + strThen;
  }
  return sortclause;
}


std::string CMusicQueryBuilder::SortnameBuildSQL(const std::string& strAlias,
                                             const SortAttribute& sortAttributes,
                                             const std::string& strField,
                                             const std::string& strSortField,
                                         const CMusicDatabase& db)
{
  /*
  Build SQL for sort name scalar subquery from sort attributes and ignore article list.
  For example :
  CASE WHEN strArtistSort IS NOT NULL THEN strArtistSort
  WHEN strField LIKE 'the ' OR strField LIKE 'the_' ESCAPE '_' THEN SUBSTR(strArtist, 5)
  WHEN strField LIKE 'LIKE 'an.' strField LIKE 'an_' ESCAPE '_' THEN SUBSTR(strArtist, 4)
  ELSE strField
  END AS strAlias
  */

  std::string sortSQL;
  if (!strSortField.empty() && sortAttributes & SortAttributeUseArtistSortName)
    sortSQL =
        db.PrepareSQL("WHEN %s IS NOT NULL THEN %s ", strSortField.c_str(), strSortField.c_str());
  if (sortAttributes & SortAttributeIgnoreArticle)
  {
    if (!sortSQL.empty())
      sortSQL += " ";
    // Make SQL from ignore article list, grouping tokens the same length together
    sortSQL += GetIgnoreArticleSQL(strField, db);
  }
  if (!sortSQL.empty())
  {
    sortSQL = "CASE " + sortSQL; // Not prepare as may contain ' and % etc.
    sortSQL += db.PrepareSQL(" ELSE %s END AS %s", strField.c_str(), strAlias.c_str());
  }

  return sortSQL;
}


std::string CMusicQueryBuilder::AlphanumericSortSQL(const std::string& strField,
                                                const SortOrder& sortOrder,
                                         const CMusicDatabase& db)
{
  /*
  Use custom collation ALPHANUM in SQLite. This handles natural number order, case sensitivity
  and locale UFT-8 order for accents using the same functionality as fileitem list sorting.
  Natural number order is not significant for where clause comparison and use of calculated fields
  means there is no advantage in defining as column default in table create than per query (which
  also makes looking at the db with other tools difficult).

  MySQL does not have callback collation, but all tables are defined with utf8_general_ci an
  "ascii folding" case insensitive collation. Natural sorting is provided via native functions
  stored in the db.
  */
  std::string DESC;
  if (sortOrder == SortOrder::DESCENDING)
    DESC = " DESC";
  std::string strSort;

  if (StringUtils::EqualsNoCase(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type,
          "mysql"))
    strSort = db.PrepareSQL("udfNaturalSortFormat(%s, 8, '.')%s", strField.c_str(), DESC.c_str());
  else
    strSort = db.PrepareSQL("%s COLLATE ALPHANUM%s", strField.c_str(), DESC.c_str());
  return strSort;
}

int CMusicQueryBuilder::GetOrderFilter(const std::string& type,
                                   const SortDescription& sorting,
                                   Filter& filter,
    const CMusicDatabase& db)
{
  // Populate filter with ORDER BY clause and any extra scalar query fields needed for sort
  int iFieldsAdded = 0;
  filter.fields.clear(); // remove "*"
  std::vector<std::string> orderfields;
  std::string DESC;

  if (sorting.sortOrder == SortOrder::DESCENDING)
    DESC = " DESC";

  if (sorting.sortBy == SortBy::RANDOM)
    orderfields.emplace_back(db.PrepareSQL("RANDOM()")); //Adjusts styntax for MySQL
  else
  {
    FieldList fields;
    SortUtils::GetFieldsForSQLSort(type, sorting.sortBy, fields);
    for (const auto& it : fields)
    {
      std::string strField;
      if (it == Field::YEAR)
        strField = "iYear";
      else
        strField = DatabaseUtils::GetField(it, type, DatabaseQueryPart::SELECT);
      if (!strField.empty())
        orderfields.emplace_back(strField);
    }
  }

  // Get the right tableview as if we are using strArtistSort the column name is ambiguous
  std::string table;
  if (StringUtils::StartsWithNoCase(type, "album"))
    table = "albumview.";
  else if (StringUtils::StartsWithNoCase(type, "song"))
    table = "songview.";

  // Convert field names into order by statement elements
  for (auto& name : orderfields)
  {
    //Add field for adjusted name sorting using sort name and ignoring articles
    std::string sortSQL;
    if (StringUtils::EndsWith(name, "strArtists") || StringUtils::EndsWith(name, "strArtist"))
    {
      if (StringUtils::EndsWith(name, "strArtists"))
        sortSQL = CMusicQueryBuilder::SortnameBuildSQL("artistsortname", sorting.sortAttributes, name,
                                   table + "strArtistSort", db);
      else
        sortSQL = CMusicQueryBuilder::SortnameBuildSQL("artistsortname", sorting.sortAttributes, name, "strSortName", db);
      if (!sortSQL.empty())
      {
        name = "artistsortname";
        filter.AppendField(sortSQL); // Add artistsortname as scalar query field
        iFieldsAdded++;
      }
      // Natural number case-insensitive sort
      filter.AppendOrder(CMusicQueryBuilder::AlphanumericSortSQL(name, sorting.sortOrder, db));
    }
    else if (StringUtils::EndsWith(name, "strAlbum") || StringUtils::EndsWith(name, "strTitle"))
    {
      sortSQL = CMusicQueryBuilder::SortnameBuildSQL("titlesortname", sorting.sortAttributes, name, "", db);
      if (!sortSQL.empty())
      {
        name = "titlesortname";
        filter.AppendField(sortSQL); // Add sortname as scalar query field
        iFieldsAdded++;
      }
      // Natural number case-insensitive sort
      filter.AppendOrder(CMusicQueryBuilder::AlphanumericSortSQL(name, sorting.sortOrder, db));
    }
    else if (StringUtils::EndsWith(name, "strGenres"))
      // Natural number case-insensitive sort
      filter.AppendOrder(CMusicQueryBuilder::AlphanumericSortSQL(name, sorting.sortOrder, db));
    else
      filter.AppendOrder(name + DESC);
  }
  return iFieldsAdded;
}


bool CMusicQueryBuilder::GetFilter(CDbUrl& musicUrl, Filter& filter, SortDescription& sorting,
    CMusicDatabase& db)
{
  if (!musicUrl.IsValid())
    return false;

  std::string type = musicUrl.GetType();
  const CUrlOptions::UrlOptions& options = musicUrl.GetOptions();

  // Check for playlist rules first, they may contain role criteria
  bool hasRoleRules = false;

  auto option = options.find("xsp");
  if (option != options.end())
  {
    PLAYLIST::CSmartPlaylist xsp;
    if (!xsp.LoadFromJson(option->second.asString()))
      return false;

    std::set<std::string, std::less<>> playlists;
    std::string xspWhere;
    xspWhere = xsp.GetWhereClause(db, playlists);
    hasRoleRules = xsp.GetType() == "artists" &&
                   xspWhere.find("song_artist.idRole = role.idRole") != std::string::npos;

    // Check if the filter playlist matches the item type
    // Allow for grouping name like "originalyears" and type "years"
    if (xsp.GetType() == type ||
        (xsp.GetGroup().find(type) != std::string::npos && !xsp.IsGroupMixed()))
    {
      filter.AppendWhere(xspWhere);

      if (xsp.GetLimit() > 0)
        sorting.limitEnd = xsp.GetLimit();
      if (xsp.GetOrder() != SortBy::NONE)
        sorting.sortBy = xsp.GetOrder();
      sorting.sortOrder = xsp.GetOrderAscending() ? SortOrder::ASCENDING : SortOrder::DESCENDING;
      if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
              CSettings::SETTING_FILELISTS_IGNORETHEWHENSORTING))
        sorting.sortAttributes = SortAttributeIgnoreArticle;
    }
  }

  //Process role options, common to artist and album type filtering
  int idRole = 1; // Default restrict song_artist to "artists" only, no other roles.
  option = options.find("roleid");
  if (option != options.end())
    idRole = static_cast<int>(option->second.asInteger());
  else
  {
    option = options.find("role");
    if (option != options.end())
    {
      if (option->second.asString() == "all" || option->second.asString() == "%")
        idRole = -1000; //All roles
      else
        idRole = db.GetRoleByName(option->second.asString());
    }
  }
  if (hasRoleRules)
  {
    // Get Role from role rule(s) here.
    // But that requires much change, so for now get all roles as better than none
    idRole = -1000; //All roles
  }

  std::string strRoleSQL; //Role < 0 means all roles, otherwise filter by role
  if (idRole > 0)
    strRoleSQL = db.PrepareSQL(" AND song_artist.idRole = %i ", idRole);

  int idArtist = -1;
  int idGenre = -1;
  int idAlbum = -1;
  int idSong = -1;
  int idDisc = -1;
  int idSource = -1;
  bool albumArtistsOnly = false;
  bool useOriginalYear = false;
  std::string artistname;

  // Process useoriginalyear option, setting overridden by option
  useOriginalYear = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
      CSettings::SETTING_MUSICLIBRARY_USEORIGINALDATE);
  option = options.find("useoriginalyear");
  if (option != options.end())
    useOriginalYear = option->second.asBoolean();

  // Process albumartistsonly option
  option = options.find("albumartistsonly");
  if (option != options.end())
    albumArtistsOnly = option->second.asBoolean();

  // Process genre option
  option = options.find("genreid");
  if (option != options.end())
    idGenre = static_cast<int>(option->second.asInteger());
  else
  {
    option = options.find("genre");
    if (option != options.end())
      idGenre = db.GetGenreByName(option->second.asString());
  }

  // Process source option
  option = options.find("sourceid");
  if (option != options.end())
    idSource = static_cast<int>(option->second.asInteger());
  else
  {
    option = options.find("source");
    if (option != options.end())
      idSource = db.GetSourceByName(option->second.asString());
  }

  // Process album option
  option = options.find("albumid");
  if (option != options.end())
    idAlbum = static_cast<int>(option->second.asInteger());
  else
  {
    option = options.find("album");
    if (option != options.end())
      idAlbum = db.GetAlbumByName(option->second.asString());
  }

  // Process artist option
  option = options.find("artistid");
  if (option != options.end())
    idArtist = static_cast<int>(option->second.asInteger());
  else
  {
    option = options.find("artist");
    if (option != options.end())
    {
      idArtist = db.GetArtistByName(option->second.asString());
      if (idArtist == -1)
      { // not found with that name, or more than one found as artist name is not unique
        artistname = option->second.asString();
      }
    }
  }

  //  Process song option
  option = options.find("songid");
  if (option != options.end())
    idSong = static_cast<int>(option->second.asInteger());

  if (type == "artists")
  {
    if (!hasRoleRules)
    { // Not an "artists" smart playlist with roles rules, so get filter from options
      if (idArtist > 0)
        filter.AppendWhere(db.PrepareSQL("artistview.idArtist = %d", idArtist));
      else if (idAlbum > 0)
        filter.AppendWhere(
            db.PrepareSQL("artistview.idArtist IN (SELECT album_artist.idArtist FROM album_artist "
                       "WHERE album_artist.idAlbum = %i)",
                       idAlbum));
      else if (idSong > 0)
      {
        filter.AppendWhere(
            db.PrepareSQL("artistview.idArtist IN (SELECT song_artist.idArtist FROM song_artist "
                       "WHERE song_artist.idSong = %i %s)",
                       idSong, strRoleSQL.c_str()));
      }
      else
      { /*
        Process idRole, idGenre, idSource and albumArtistsOnly options

        For artists these rules are combined because they apply via album and song
        and so we need to ensure all criteria are met via the same album or song.
        1) Some artists may be only album artists, so for all artists (with linked
           albums or songs) we need to check both album_artist and song_artist tables.
        2) Role is determined from song_artist table, so even if looking for album artists
           only we find those that also have a specific role e.g. which album artist is a
           composer of songs in that album, from entries in the song_artist table.
        a) Role < -1 is used to indicate that all roles are wanted.
        b) When not album artists only and a specific role wanted then only the song_artist
           table is checked.
        c) When album artists only and role = 1 (an "artist") then only the album_artist
           table is checked.
        */
        std::string albumArtistSQL, songArtistSQL;
        ExistsSubQuery albumArtistSub("album_artist",
                                      "album_artist.idArtist = artistview.idArtist");
        // Prepare album artist subquery SQL
        if (idSource > 0)
        {
          if (idRole == 1 && idGenre < 0)
          {
            albumArtistSub.AppendJoin(
                "JOIN album_source ON album_source.idAlbum = album_artist.idAlbum");
            albumArtistSub.AppendWhere(db.PrepareSQL("album_source.idSource = %i", idSource));
          }
          else
          {
            albumArtistSub.AppendWhere(
                db.PrepareSQL("EXISTS(SELECT 1 FROM album_source "
                           "WHERE album_source.idSource = %i "
                           "AND album_source.idAlbum = album_artist.idAlbum)",
                           idSource));
          }
        }
        if (idRole <= 1 && idGenre > 0)
        { // Check genre of songs of album using nested subquery
          std::string strGenre =
              db.PrepareSQL("EXISTS(SELECT 1 FROM song "
                         "JOIN song_genre ON song_genre.idSong = song.idSong "
                         "WHERE song.idAlbum = album_artist.idAlbum AND song_genre.idGenre = %i)",
                         idGenre);
          albumArtistSub.AppendWhere(strGenre);
        }

        // Prepare song artist subquery SQL
        ExistsSubQuery songArtistSub("song_artist", "song_artist.idArtist = artistview.idArtist");
        if (idRole > 0)
          songArtistSub.AppendWhere(db.PrepareSQL("song_artist.idRole = %i", idRole));
        if (idSource > 0 && idGenre > 0 && !albumArtistsOnly && idRole >= 1)
        {
          songArtistSub.AppendWhere(db.PrepareSQL("EXISTS(SELECT 1 FROM song "
                                               "JOIN song_genre ON song_genre.idSong = song.idSong "
                                               "WHERE song.idSong = song_artist.idSong "
                                               "AND song_genre.idGenre = %i "
                                               "AND EXISTS(SELECT 1 FROM album_source "
                                               "WHERE album_source.idSource = %i "
                                               "AND album_source.idAlbum = song.idAlbum))",
                                               idGenre, idSource));
        }
        else
        {
          if (idGenre > 0)
          {
            songArtistSub.AppendJoin("JOIN song_genre ON song_genre.idSong = song_artist.idSong");
            songArtistSub.AppendWhere(db.PrepareSQL("song_genre.idGenre = %i", idGenre));
          }
          if (idSource > 0 && !albumArtistsOnly)
          {
            songArtistSub.AppendJoin("JOIN song ON song.idSong = song_artist.idSong");
            songArtistSub.AppendJoin("JOIN album_source ON album_source.idAlbum = song.idAlbum");
            songArtistSub.AppendWhere(db.PrepareSQL("album_source.idSource = %i", idSource));
          }
          if (idRole > 1 && albumArtistsOnly)
          { // Album artists only with role, check AND in album_artist for album of song
            // using nested subquery correlated with album_artist
            songArtistSub.AppendJoin("JOIN song ON song.idSong = song_artist.idSong");
            songArtistSub.param = "song_artist.idArtist = album_artist.idArtist";
            songArtistSub.AppendWhere("song.idAlbum = album_artist.idAlbum");
          }
        }

        // Build filter clause from subqueries
        if (idRole > 1 && albumArtistsOnly)
        { // Album artists only with role, check AND in album_artist for album of song
          // using nested subquery correlated with album_artist
          songArtistSub.BuildSQL(songArtistSQL);
          albumArtistSub.AppendWhere(songArtistSQL);
          albumArtistSub.BuildSQL(albumArtistSQL);
          filter.AppendWhere(albumArtistSQL);
        }
        else
        {
          songArtistSub.BuildSQL(songArtistSQL);
          albumArtistSub.BuildSQL(albumArtistSQL);
          if (idRole < 0 || (idRole == 1 && !albumArtistsOnly))
          { // Artist contributing to songs, any role, check OR album artist too
            // as artists can be just album artists but not song artists
            filter.AppendWhere(songArtistSQL + " OR " + albumArtistSQL);
          }
          else if (idRole > 1)
          {
            // Artist contributes that role (not albmartistsonly as already handled)
            filter.AppendWhere(songArtistSQL);
          }
          else // idRole = 1 and albumArtistsOnly
          { // Only look at album artists, not albums where artist features on songs
            filter.AppendWhere(albumArtistSQL);
          }
        }
      }
    }
    // remove the null string
    filter.AppendWhere("artistview.strArtist != ''");
  }
  else if (type == "albums")
  {
    option = options.find("year");
    if (option != options.end())
    {
      if (!useOriginalYear)
        filter.AppendWhere(db.PrepareSQL("albumview.strReleaseDate LIKE '%s%%%%'",
                                      option->second.asString().c_str()));
      else
        filter.AppendWhere(db.PrepareSQL("albumview.strOrigReleaseDate LIKE '%s%%%%'",
                                      option->second.asString().c_str()));
    }
    option = options.find("compilation");
    if (option != options.end())
      filter.AppendWhere(
          db.PrepareSQL("albumview.bCompilation = %i", option->second.asBoolean() ? 1 : 0));

    option = options.find("boxset");
    if (option != options.end())
      filter.AppendWhere(
          db.PrepareSQL("albumview.bBoxedSet = %i", option->second.asBoolean() ? 1 : 0));

    if (idSource > 0)
      filter.AppendWhere(db.PrepareSQL(
          "EXISTS(SELECT 1 FROM album_source "
          "WHERE album_source.idAlbum = albumview.idAlbum AND album_source.idSource = %i)",
          idSource));

    // Process artist, role and genre options together as song subquery to filter those
    // albums that have songs with both that artist and genre
    std::string albumArtistSQL, songArtistSQL, genreSQL;
    ExistsSubQuery genreSub("song", "song.idAlbum = album_artist.idAlbum");
    genreSub.AppendJoin("JOIN song_genre ON song_genre.idSong = song.idSong");
    genreSub.AppendWhere(db.PrepareSQL("song_genre.idGenre = %i", idGenre));
    ExistsSubQuery albumArtistSub("album_artist", "album_artist.idAlbum = albumview.idAlbum");
    ExistsSubQuery songArtistSub("song_artist", "song.idAlbum = albumview.idAlbum");
    songArtistSub.AppendJoin("JOIN song ON song.idSong = song_artist.idSong");

    if (idArtist > 0)
    {
      songArtistSub.AppendWhere(db.PrepareSQL("song_artist.idArtist = %i", idArtist));
      albumArtistSub.AppendWhere(db.PrepareSQL("album_artist.idArtist = %i", idArtist));
    }
    else if (!artistname.empty())
    { // Artist name is not unique, so could get albums or songs from more than one.
      songArtistSub.AppendJoin("JOIN artist ON artist.idArtist = song_artist.idArtist");
      songArtistSub.AppendWhere(db.PrepareSQL("artist.strArtist like '%s'", artistname.c_str()));

      albumArtistSub.AppendJoin("JOIN artist ON artist.idArtist = album_artist.idArtist");
      albumArtistSub.AppendWhere(db.PrepareSQL("artist.strArtist like '%s'", artistname.c_str()));
    }
    if (idRole > 0)
      songArtistSub.AppendWhere(db.PrepareSQL("song_artist.idRole = %i", idRole));
    if (idGenre > 0)
    {
      songArtistSub.AppendJoin("JOIN song_genre ON song_genre.idSong = song.idSong");
      songArtistSub.AppendWhere(db.PrepareSQL("song_genre.idGenre = %i", idGenre));
    }

    if (idArtist > 0 || !artistname.empty())
    {
      if (idRole <= 1 && idGenre > 0)
      { // Check genre of songs of album using nested subquery
        genreSub.BuildSQL(genreSQL);
        albumArtistSub.AppendWhere(genreSQL);
      }
      if (idRole > 1 && albumArtistsOnly)
      { // Album artists only with role, check AND in album_artist for same song
        // using nested subquery correlated with album_artist
        songArtistSub.param = "song.idAlbum = album_artist.idAlbum";
        songArtistSub.BuildSQL(songArtistSQL);
        albumArtistSub.AppendWhere(songArtistSQL);
        albumArtistSub.BuildSQL(albumArtistSQL);
        filter.AppendWhere(albumArtistSQL);
      }
      else
      {
        songArtistSub.BuildSQL(songArtistSQL);
        albumArtistSub.BuildSQL(albumArtistSQL);
        if (idRole < 0 || (idRole == 1 && !albumArtistsOnly))
        { // Artist contributing to songs, any role, check OR album artist too
          // as artists can be just album artists but not song artists
          filter.AppendWhere(songArtistSQL + " OR " + albumArtistSQL);
        }
        else if (idRole > 1)
        { // Albums with songs where artist contributes that role (not albmartistsonly as already handled)
          filter.AppendWhere(songArtistSQL);
        }
        else // idRole = 1 and albumArtistsOnly
        { // Only look at album artists, not albums where artist features on songs
          // This may want to be a separate option so you can choose to see all the albums where that artist
          // appears on one or more songs without having to list all song artists in the artists node.
          filter.AppendWhere(albumArtistSQL);
        }
      }
    }
    else
    { // No artist given
      if (idGenre > 0)
      { // Have genre option but not artist
        genreSub.param = "song.idAlbum = albumview.idAlbum";
        genreSub.BuildSQL(genreSQL);
        filter.AppendWhere(genreSQL);
      }
      // Exclude any single albums (aka empty tagged albums)
      // This causes "albums"  media filter artist selection to only offer album artists
      option = options.find("show_singles");
      if (option == options.end() || !option->second.asBoolean())
        filter.AppendWhere(db.PrepareSQL("albumview.strReleaseType = '%s'",
                                      CAlbum::ReleaseTypeToString(ReleaseType::Album).c_str()));
    }
  }
  else if (type == "discs")
  {
    if (idAlbum > 0)
      filter.AppendWhere(db.PrepareSQL("albumview.idAlbum = %i", idAlbum));
    else
    {
      option = options.find("year");
      if (option != options.end())
      {
        if (!useOriginalYear)
          filter.AppendWhere(db.PrepareSQL("albumview.strReleaseDate LIKE '%s%%%%'",
                                        option->second.asString().c_str()));
        else
          filter.AppendWhere(db.PrepareSQL("albumview.strOrigReleaseDate LIKE '%s%%%%'",
                                        option->second.asString().c_str()));
      }

      option = options.find("compilation");
      if (option != options.end())
        filter.AppendWhere(
            db.PrepareSQL("albumview.bCompilation = %i", option->second.asBoolean() ? 1 : 0));

      option = options.find("boxset");
      if (option != options.end())
        filter.AppendWhere(
            db.PrepareSQL("albumview.bBoxedSet = %i", option->second.asBoolean() ? 1 : 0));

      if (idSource > 0)
        filter.AppendWhere(db.PrepareSQL(
            "EXISTS(SELECT 1 FROM album_source "
            "WHERE album_source.idAlbum = albumview.idAlbum AND album_source.idSource = %i)",
            idSource));
    }
    option = options.find("discid");
    if (option != options.end())
      filter.AppendWhere(db.PrepareSQL("iDisc = %i", option->second.asInteger()));

    option = options.find("disctitle");
    if (option != options.end())
      filter.AppendWhere(db.PrepareSQL("strDiscSubtitle = '%s'", option->second.asString().c_str()));

    if (idGenre > 0)
      filter.AppendWhere(db.PrepareSQL("EXISTS(SELECT 1 FROM song_genre WHERE song_genre.idSong = "
                                    "song.idSong AND song_genre.idGenre = %i)",
                                    idGenre));

    std::string songArtistClause;
    std::string albumArtistClause;
    if (idArtist > 0)
    {
      songArtistClause =
          db.PrepareSQL("EXISTS (SELECT 1 FROM song_artist "
                     "WHERE song_artist.idSong = song.idSong AND song_artist.idArtist = %i %s)",
                     idArtist, strRoleSQL.c_str());
      albumArtistClause =
          db.PrepareSQL("EXISTS (SELECT 1 FROM album_artist "
                     "WHERE album_artist.idAlbum = song.idAlbum AND album_artist.idArtist = %i)",
                     idArtist);
    }
    else if (!artistname.empty())
    { // Artist name is not unique, so could get songs from more than one.
      songArtistClause = db.PrepareSQL(
          "EXISTS (SELECT 1 FROM song_artist JOIN artist ON artist.idArtist = song_artist.idArtist "
          "WHERE song_artist.idSong = song.idSong AND artist.strArtist like '%s' %s)",
          artistname.c_str(), strRoleSQL.c_str());
      albumArtistClause =
          db.PrepareSQL("EXISTS (SELECT 1 FROM album_artist JOIN artist ON artist.idArtist = "
                     "album_artist.idArtist "
                     "WHERE album_artist.idAlbum = song.idAlbum AND artist.strArtist like '%s')",
                     artistname.c_str());
    }

    // Process artist name or id option
    if (!songArtistClause.empty())
    {
      if (idRole < 0) // Artist contributes to songs, any roles OR is album artist
        filter.AppendWhere("(" + songArtistClause + " OR " + albumArtistClause + ")");
      else if (idRole > 1)
      {
        if (albumArtistsOnly) //Album artists only with role, check AND in album_artist for same song
          filter.AppendWhere("(" + songArtistClause + " AND " + albumArtistClause + ")");
        else // songs where artist contributes that role.
          filter.AppendWhere(songArtistClause);
      }
      else
      {
        if (albumArtistsOnly) // Only look at album artists, not where artist features on songs
          filter.AppendWhere(albumArtistClause);
        else // Artist is song artist or album artist
          filter.AppendWhere("(" + songArtistClause + " OR " + albumArtistClause + ")");
      }
    }
  }
  else if (type == "songs" || type == "singles")
  {
    option = options.find("singles");
    if (option != options.end())
      filter.AppendWhere(db.PrepareSQL(
          "songview.idAlbum %sIN (SELECT idAlbum FROM album WHERE strReleaseType = '%s')",
          option->second.asBoolean() ? "" : "NOT ",
          CAlbum::ReleaseTypeToString(ReleaseType::Single).c_str()));

    // When have idAlbum skip year, compilation, boxset criteria as already applied via album
    if (idAlbum < 0)
    {
      option = options.find("year");
      if (option != options.end())
      {
        if (!useOriginalYear)
          filter.AppendWhere(db.PrepareSQL("songview.strReleaseDate LIKE '%s%%%%'",
                                        option->second.asString().c_str()));
        else
          filter.AppendWhere(db.PrepareSQL("songview.strOrigReleaseDate LIKE '%s%%%%'",
                                        option->second.asString().c_str()));
      }
      option = options.find("compilation");
      if (option != options.end())
        filter.AppendWhere(
            db.PrepareSQL("songview.bCompilation = %i", option->second.asBoolean() ? 1 : 0));

      option = options.find("boxset");
      if (option != options.end())
        filter.AppendWhere(db.PrepareSQL("EXISTS(SELECT 1 FROM album WHERE album.idAlbum = "
                                      "songview.idAlbum AND bBoxedSet = %i)",
                                      option->second.asBoolean() ? 1 : 0));
    }

    option = options.find("discid");
    if (option != options.end())
      idDisc = static_cast<int>(option->second.asInteger());

    option = options.find("disctitle");
    if (option != options.end())
      filter.AppendWhere(
          db.PrepareSQL("songview.strDiscSubtitle = '%s'", option->second.asString().c_str()));

    if (idSong > 0)
      filter.AppendWhere(db.PrepareSQL("songview.idSong = %i", idSong));

    if (idAlbum > 0)
      filter.AppendWhere(db.PrepareSQL("songview.idAlbum = %i", idAlbum));

    if (idDisc > 0)
      filter.AppendWhere(db.PrepareSQL("songview.iTrack >> 16 = %i", idDisc));

    if (idGenre > 0)
      filter.AppendWhere(db.PrepareSQL("songview.idSong IN (SELECT song_genre.idSong FROM song_genre "
                                    "WHERE song_genre.idGenre = %i)",
                                    idGenre));

    if (idSource > 0)
      filter.AppendWhere(db.PrepareSQL(
          "EXISTS(SELECT 1 FROM album_source "
          "WHERE album_source.idAlbum = songview.idAlbum AND album_source.idSource = %i)",
          idSource));

    std::string songArtistClause;
    std::string albumArtistClause;
    if (idArtist > 0)
    {
      songArtistClause =
          db.PrepareSQL("EXISTS (SELECT 1 FROM song_artist "
                     "WHERE song_artist.idSong = songview.idSong AND song_artist.idArtist = %i %s)",
                     idArtist, strRoleSQL.c_str());
      albumArtistClause = db.PrepareSQL(
          "EXISTS (SELECT 1 FROM album_artist "
          "WHERE album_artist.idAlbum = songview.idAlbum AND album_artist.idArtist = %i)",
          idArtist);
    }
    else if (!artistname.empty())
    { // Artist name is not unique, so could get songs from more than one.
      songArtistClause = db.PrepareSQL(
          "EXISTS (SELECT 1 FROM song_artist "
          "JOIN artist ON artist.idArtist = song_artist.idArtist "
          "WHERE song_artist.idSong = songview.idSong AND artist.strArtist like '%s' %s)",
          artistname.c_str(), strRoleSQL.c_str());
      albumArtistClause = db.PrepareSQL(
          "EXISTS (SELECT 1 FROM album_artist "
          "JOIN artist ON artist.idArtist = album_artist.idArtist "
          "WHERE album_artist.idAlbum = songview.idAlbum AND artist.strArtist like '%s')",
          artistname.c_str());
    }

    // Process artist name or id option
    if (!songArtistClause.empty())
    {
      if (idRole < 0) // Artist contributes to songs, any roles OR is album artist
        filter.AppendWhere("(" + songArtistClause + " OR " + albumArtistClause + ")");
      else if (idRole > 1)
      {
        if (albumArtistsOnly) //Album artists only with role, check AND in album_artist for same song
          filter.AppendWhere("(" + songArtistClause + " AND " + albumArtistClause + ")");
        else // songs where artist contributes that role.
          filter.AppendWhere(songArtistClause);
      }
      else
      {
        if (albumArtistsOnly) // Only look at album artists, not where artist features on songs
          filter.AppendWhere(albumArtistClause);
        else // Artist is song artist or album artist
          filter.AppendWhere("(" + songArtistClause + " OR " + albumArtistClause + ")");
      }
    }
  }

  option = options.find("filter");
  if (option != options.end())
  {
    PLAYLIST::CSmartPlaylist xspFilter;
    if (!xspFilter.LoadFromJson(option->second.asString()))
      return false;

    // check if the filter playlist matches the item type
    if (xspFilter.GetType() == type)
    {
      std::set<std::string, std::less<>> playlists;
      filter.AppendWhere(xspFilter.GetWhereClause(db, playlists));
    }
    // remove the filter if it doesn't match the item type
    else
      musicUrl.RemoveOption("filter");
  }

  return true;
}
