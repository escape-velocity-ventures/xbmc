/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "music/MusicSchemaManager.h"

#include "ServiceBroker.h"
#include "music/Artist.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace KODI::DATABASE;

void CMusicSchemaManager::CreateTables(CDatabase& db)
{
  CLog::Log(LOGINFO, "create artist table");
  db.ExecuteQuery("CREATE TABLE artist ( idArtist integer primary key, "
                  " strArtist varchar(256), strMusicBrainzArtistID text, "
                  " strSortName text, "
                  " strType text, strGender text, strDisambiguation text, "
                  " strBorn text, strFormed text, strGenres text, strMoods text, "
                  " strStyles text, strInstruments text, strBiography text, "
                  " strDied text, strDisbanded text, strYearsActive text, "
                  " strImage text, "
                  " lastScraped varchar(20) default NULL, "
                  " bScrapedMBID INTEGER NOT NULL DEFAULT 0, "
                  " idInfoSetting INTEGER NOT NULL DEFAULT 0, "
                  " dateAdded TEXT, dateNew TEXT, dateModified TEXT)");
  // Create missing artist tag artist [Missing].
  std::string strSQL =
      db.PrepareSQL("INSERT INTO artist (idArtist, strArtist, strSortName, strMusicBrainzArtistID) "
                    "VALUES( %i, '%s', '%s', '%s' )",
                    BLANKARTIST_ID, BLANKARTIST_NAME.data(), BLANKARTIST_NAME.data(),
                    BLANKARTIST_FAKEMUSICBRAINZID.data());
  db.ExecuteQuery(strSQL);

  CLog::Log(LOGINFO, "create album table");
  db.ExecuteQuery("CREATE TABLE album (idAlbum integer primary key, "
                  " strAlbum varchar(256), strMusicBrainzAlbumID text, "
                  " strReleaseGroupMBID text, "
                  " strArtistDisp text, strArtistSort text, strGenres text, "
                  " strReleaseDate TEXT, strOrigReleaseDate TEXT, "
                  " bBoxedSet INTEGER NOT NULL DEFAULT 0, "
                  " bCompilation integer not null default '0', "
                  " strMoods text, strStyles text, strThemes text, "
                  " strReview text, strImage text, strLabel text, "
                  " strType text, "
                  " strReleaseStatus TEXT, "
                  " fRating FLOAT NOT NULL DEFAULT 0, "
                  " iVotes INTEGER NOT NULL DEFAULT 0, "
                  " iUserrating INTEGER NOT NULL DEFAULT 0, "
                  " lastScraped varchar(20) default NULL, "
                  " bScrapedMBID INTEGER NOT NULL DEFAULT 0, "
                  " strReleaseType text, "
                  " iDiscTotal INTEGER NOT NULL DEFAULT 0, "
                  " iAlbumDuration INTEGER NOT NULL DEFAULT 0, "
                  " idInfoSetting INTEGER NOT NULL DEFAULT 0, "
                  " dateAdded TEXT, dateNew TEXT, dateModified TEXT)");

  CLog::Log(LOGINFO, "create audiobook table");
  db.ExecuteQuery("CREATE TABLE audiobook (idBook integer primary key, "
                  " strBook varchar(256), strAuthor text,"
                  " bookmark integer, file text,"
                  " dateAdded varchar (20) default NULL)");

  CLog::Log(LOGINFO, "create album_artist table");
  db.ExecuteQuery("CREATE TABLE album_artist (idArtist integer, idAlbum integer, iOrder integer, "
                  "strArtist text)");

  CLog::Log(LOGINFO, "create album_source table");
  db.ExecuteQuery("CREATE TABLE album_source (idSource INTEGER, idAlbum INTEGER)");

  CLog::Log(LOGINFO, "create genre table");
  db.ExecuteQuery("CREATE TABLE genre (idGenre integer primary key, strGenre varchar(256))");

  CLog::Log(LOGINFO, "create path table");
  db.ExecuteQuery(
      "CREATE TABLE path (idPath integer primary key, strPath varchar(512), strHash text)");

  CLog::Log(LOGINFO, "create source table");
  db.ExecuteQuery(
      "CREATE TABLE source (idSource INTEGER PRIMARY KEY, strName TEXT, strMultipath TEXT)");

  CLog::Log(LOGINFO, "create source_path table");
  db.ExecuteQuery(
      "CREATE TABLE source_path (idSource INTEGER, idPath INTEGER, strPath varchar(512))");

  CLog::Log(LOGINFO, "create song table");
  db.ExecuteQuery("CREATE TABLE song (idSong integer primary key, "
                  " idAlbum integer, idPath integer, "
                  " strArtistDisp text, strArtistSort text, strGenres text, strTitle varchar(512), "
                  " iTrack integer, iDuration integer, "
                  " strReleaseDate TEXT, strOrigReleaseDate TEXT, "
                  " strDiscSubtitle text, strFileName text, strMusicBrainzTrackID text, "
                  " iTimesPlayed integer, iStartOffset integer, iEndOffset integer, "
                  " lastplayed varchar(20) default NULL, "
                  " rating FLOAT NOT NULL DEFAULT 0, votes INTEGER NOT NULL DEFAULT 0, "
                  " userrating INTEGER NOT NULL DEFAULT 0, "
                  " comment text, mood text, iBPM INTEGER NOT NULL DEFAULT 0, "
                  " iBitRate INTEGER NOT NULL DEFAULT 0, "
                  " iSampleRate INTEGER NOT NULL DEFAULT 0, iChannels INTEGER NOT NULL DEFAULT 0, "
                  " strVideoURL TEXT, "
                  " strReplayGain text, "
                  " dateAdded TEXT, dateNew TEXT, dateModified TEXT)");
  CLog::Log(LOGINFO, "create song_artist table");
  db.ExecuteQuery("CREATE TABLE song_artist (idArtist integer, idSong integer, idRole integer, "
                  "iOrder integer, strArtist text)");
  CLog::Log(LOGINFO, "create song_genre table");
  db.ExecuteQuery("CREATE TABLE song_genre (idGenre integer, idSong integer, iOrder integer)");

  CLog::Log(LOGINFO, "create role table");
  db.ExecuteQuery("CREATE TABLE role (idRole integer primary key, strRole text)");
  db.ExecuteQuery("INSERT INTO role(idRole, strRole) VALUES (1, 'Artist')"); //Default role

  CLog::Log(LOGINFO, "create infosetting table");
  db.ExecuteQuery("CREATE TABLE infosetting (idSetting INTEGER PRIMARY KEY, "
                  "strScraperPath TEXT, strSettings TEXT)");

  CLog::Log(LOGINFO, "create discography table");
  db.ExecuteQuery("CREATE TABLE discography (idArtist integer, strAlbum text, strYear text, "
                  "strReleaseGroupMBID TEXT)");

  CLog::Log(LOGINFO, "create art table");
  db.ExecuteQuery("CREATE TABLE art(art_id INTEGER PRIMARY KEY, "
                  "media_id INTEGER, media_type TEXT, type TEXT, url TEXT)");

  CLog::Log(LOGINFO, "create versiontagscan table");
  db.ExecuteQuery("CREATE TABLE versiontagscan "
                  "(idVersion INTEGER, iNeedsScan INTEGER, "
                  "lastscanned VARCHAR(20), "
                  "lastcleaned VARCHAR(20), "
                  "artistlinksupdated VARCHAR(20), "
                  "genresupdated VARCHAR(20))");
  db.ExecuteQuery(db.PrepareSQL("INSERT INTO versiontagscan (idVersion, iNeedsScan) values(%i, 0)",
                                db.GetSchemaVersion()));

  CLog::Log(LOGINFO, "create removed_link table");
  db.ExecuteQuery("CREATE TABLE removed_link (idArtist INTEGER, idMedia INTEGER, idRole INTEGER)");
}

void CMusicSchemaManager::CreateAnalytics(CDatabase& db)
{
  CLog::Log(LOGINFO, "creating indices");
  db.ExecuteQuery("CREATE INDEX idxAlbum ON album(strAlbum(255))");
  db.ExecuteQuery("CREATE INDEX idxAlbum_1 ON album(bCompilation)");
  db.ExecuteQuery("CREATE UNIQUE INDEX idxAlbum_2 ON album(strMusicBrainzAlbumID(36))");
  db.ExecuteQuery("CREATE INDEX idxAlbum_3 ON album(idInfoSetting)");

  db.ExecuteQuery("CREATE UNIQUE INDEX idxAlbumArtist_1 ON album_artist ( idAlbum, idArtist )");
  db.ExecuteQuery("CREATE UNIQUE INDEX idxAlbumArtist_2 ON album_artist ( idArtist, idAlbum )");

  db.ExecuteQuery("CREATE INDEX idxGenre ON genre(strGenre(255))");

  db.ExecuteQuery("CREATE INDEX idxArtist ON artist(strArtist(255))");
  db.ExecuteQuery("CREATE UNIQUE INDEX idxArtist1 ON artist(strMusicBrainzArtistID(36))");
  db.ExecuteQuery("CREATE INDEX idxArtist_2 ON artist(idInfoSetting)");

  db.ExecuteQuery("CREATE INDEX idxPath ON path(strPath(255))");

  db.ExecuteQuery("CREATE INDEX idxSource_1 ON source(strName(255))");
  db.ExecuteQuery("CREATE INDEX idxSource_2 ON source(strMultipath(255))");

  db.ExecuteQuery("CREATE UNIQUE INDEX idxSourcePath_1 ON source_path ( idSource, idPath)");

  db.ExecuteQuery("CREATE UNIQUE INDEX idxAlbumSource_1 ON album_source ( idSource, idAlbum )");
  db.ExecuteQuery("CREATE UNIQUE INDEX idxAlbumSource_2 ON album_source ( idAlbum, idSource )");

  db.ExecuteQuery("CREATE INDEX idxSong ON song(strTitle(255))");
  db.ExecuteQuery("CREATE INDEX idxSong1 ON song(iTimesPlayed)");
  db.ExecuteQuery("CREATE INDEX idxSong2 ON song(lastplayed)");
  db.ExecuteQuery("CREATE INDEX idxSong3 ON song(idAlbum)");
  db.ExecuteQuery("CREATE INDEX idxSong6 ON song( idPath, strFileName(255) )");
  //Musicbrainz Track ID is not unique on an album, recordings are sometimes repeated e.g. "[silence]" or on a disc set
  db.ExecuteQuery(
      "CREATE UNIQUE INDEX idxSong7 ON song( idAlbum, iTrack, strMusicBrainzTrackID(36) )");

  db.ExecuteQuery("CREATE UNIQUE INDEX idxSongArtist_1 ON song_artist ( idSong, idArtist, idRole )");
  db.ExecuteQuery("CREATE INDEX idxSongArtist_2 ON song_artist ( idSong, idRole )");
  db.ExecuteQuery("CREATE INDEX idxSongArtist_3 ON song_artist ( idArtist, idRole )");
  db.ExecuteQuery("CREATE INDEX idxSongArtist_4 ON song_artist ( idRole )");

  db.ExecuteQuery("CREATE UNIQUE INDEX idxSongGenre_1 ON song_genre ( idSong, idGenre )");
  db.ExecuteQuery("CREATE UNIQUE INDEX idxSongGenre_2 ON song_genre ( idGenre, idSong )");

  db.ExecuteQuery("CREATE INDEX idxRole on role(strRole(255))");

  db.ExecuteQuery("CREATE INDEX idxDiscography_1 ON discography ( idArtist )");

  db.ExecuteQuery("CREATE INDEX ix_art ON art(media_id, media_type(20), type(20))");

  CLog::Log(LOGINFO, "create triggers");
  db.ExecuteQuery("CREATE TRIGGER tgrDeleteAlbum AFTER delete ON album FOR EACH ROW BEGIN"
                  "  DELETE FROM song WHERE song.idAlbum = old.idAlbum;"
                  "  DELETE FROM album_artist WHERE album_artist.idAlbum = old.idAlbum;"
                  "  DELETE FROM album_source WHERE album_source.idAlbum = old.idAlbum;"
                  "  DELETE FROM art WHERE media_id=old.idAlbum AND media_type='album';"
                  " END");
  db.ExecuteQuery("CREATE TRIGGER tgrDeleteArtist AFTER delete ON artist FOR EACH ROW BEGIN"
                  "  DELETE FROM album_artist WHERE album_artist.idArtist = old.idArtist;"
                  "  DELETE FROM song_artist WHERE song_artist.idArtist = old.idArtist;"
                  "  DELETE FROM discography WHERE discography.idArtist = old.idArtist;"
                  "  DELETE FROM art WHERE media_id=old.idArtist AND media_type='artist';"
                  " END");
  db.ExecuteQuery("CREATE TRIGGER tgrDeleteSong AFTER delete ON song FOR EACH ROW BEGIN"
                  "  DELETE FROM song_artist WHERE song_artist.idSong = old.idSong;"
                  "  DELETE FROM song_genre WHERE song_genre.idSong = old.idSong;"
                  "  DELETE FROM art WHERE media_id=old.idSong AND media_type='song';"
                  " END");
  db.ExecuteQuery("CREATE TRIGGER tgrDeleteSource AFTER delete ON source FOR EACH ROW BEGIN"
                  "  DELETE FROM source_path WHERE source_path.idSource = old.idSource;"
                  "  DELETE FROM album_source WHERE album_source.idSource = old.idSource;"
                  " END");

  /* Maintain date new and last modified for songs, albums and artists using triggers
     MySQL triggers cannot modify a table that is already being used by the statement that invoked
     the trigger (to avoid recursion), but can set NEW column values before insert or update.
     Meanwhile SQLite triggers cannot set NEW column values in that way, but can update same table.
     Recursion avoided using WHEN but SQLite has PRAGMA recursive-triggers off by default anyway.
     @todo: once on SQLite v3.31 we could use a generated column for dateModified as real
  */
  bool bisMySQL = StringUtils::EqualsNoCase(
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type, "mysql");

  if (!bisMySQL)
  { // SQLite trigger syntax - AFTER INSERT/UPDATE
    db.ExecuteQuery("CREATE TRIGGER tgrInsertSong AFTER INSERT ON song FOR EACH ROW BEGIN"
                    " UPDATE song SET dateNew = DATETIME('now') WHERE idSong = NEW.idSong"
                    " AND NEW.dateNew IS NULL;"
                    " UPDATE song SET dateModified = DATETIME('now') WHERE idSong = NEW.idSong;"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateSong AFTER UPDATE ON song FOR EACH ROW"
                    " WHEN NEW.dateModified <= OLD.dateModified BEGIN"
                    " UPDATE song SET dateModified = DATETIME('now') WHERE idSong = OLD.idSong;"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrInsertAlbum AFTER INSERT ON album FOR EACH ROW BEGIN"
                    " UPDATE album SET dateNew = DATETIME('now') WHERE idAlbum = NEW.idAlbum"
                    " AND NEW.dateNew IS NULL;"
                    " UPDATE album SET dateModified = DATETIME('now') WHERE idAlbum = NEW.idAlbum;"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateAlbum AFTER UPDATE ON album FOR EACH ROW"
                    " WHEN NEW.dateModified <= OLD.dateModified BEGIN"
                    " UPDATE album SET dateModified = DATETIME('now') WHERE idAlbum = OLD.idAlbum;"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrInsertArtist AFTER INSERT ON artist FOR EACH ROW BEGIN"
                    " UPDATE artist SET dateNew = DATETIME('now') WHERE idArtist = NEW.idArtist"
                    " AND NEW.dateNew IS NULL;"
                    " UPDATE artist SET dateModified = DATETIME('now') WHERE idArtist = "
                    "NEW.idArtist;"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateArtist AFTER UPDATE ON artist FOR EACH ROW"
                    " WHEN NEW.dateModified <= OLD.dateModified BEGIN"
                    " UPDATE artist SET dateModified = DATETIME('now') WHERE idArtist = "
                    "OLD.idArtist;"
                    " END");

    db.ExecuteQuery("CREATE TRIGGER tgrInsertGenre AFTER INSERT ON genre"
                    " BEGIN UPDATE versiontagscan SET genresupdated = DATETIME('now');"
                    " END");
  }
  else
  { // MySQL trigger syntax - BEFORE INSERT/UPDATE
    db.ExecuteQuery("CREATE TRIGGER tgrInsertSong BEFORE INSERT ON song FOR EACH ROW BEGIN"
                    "  IF NEW.dateNew IS NULL THEN SET NEW.dateNew = now();  END IF;"
                    "  SET NEW.dateModified = now();"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateSong BEFORE UPDATE ON song FOR EACH ROW"
                    " SET NEW.dateModified = now()");

    db.ExecuteQuery("CREATE TRIGGER tgrInsertAlbum BEFORE INSERT ON album FOR EACH ROW BEGIN"
                    "  IF NEW.dateNew IS NULL THEN SET NEW.dateNew = now();  END IF;"
                    "  SET NEW.dateModified = now();"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateAlbum BEFORE UPDATE ON album FOR EACH ROW"
                    " SET NEW.dateModified = now()");

    db.ExecuteQuery("CREATE TRIGGER tgrInsertArtist BEFORE INSERT ON artist FOR EACH ROW BEGIN"
                    "  IF NEW.dateNew IS NULL THEN SET NEW.dateNew = now();  END IF;"
                    "  SET NEW.dateModified = now();"
                    " END");
    db.ExecuteQuery("CREATE TRIGGER tgrUpdateArtist BEFORE UPDATE ON artist FOR EACH ROW"
                    " SET NEW.dateModified = now()");

    db.ExecuteQuery("CREATE TRIGGER tgrInsertGenre AFTER INSERT ON genre FOR EACH ROW"
                    " UPDATE versiontagscan SET genresupdated = now()");
  }

  // Triggers to maintain recent changes to album and song artist links in removed_link table
  db.ExecuteQuery(
      "CREATE TRIGGER tgrInsertSongArtist AFTER INSERT ON song_artist FOR EACH ROW BEGIN "
      "DELETE FROM removed_link "
      "WHERE idArtist = NEW.idArtist AND idMedia = NEW.idSong AND idRole = NEW.idRole; "
      "END");
  db.ExecuteQuery(
      "CREATE TRIGGER tgrInsertAlbumArtist AFTER INSERT ON album_artist FOR EACH ROW BEGIN "
      "DELETE FROM removed_link "
      "WHERE idArtist = NEW.idArtist AND idMedia = NEW.idAlbum AND idRole = -1; "
      "END");
  CreateRemovedLinkTriggers(db); // DELETE ON song_artist and album_artist tables

  // Create native functions stored in DB (MySQL/MariaDB only)
  CreateNativeDBFunctions(db);

  // we create views last to ensure all indexes are rolled in
  CreateViews(db);
}

void CMusicSchemaManager::CreateRemovedLinkTriggers(CDatabase& db)
{
  // DELETE ON song_artist and album_artist tables need to be recreated after cleanup
  db.ExecuteQuery(
      "CREATE TRIGGER tgrDeleteSongArtist AFTER DELETE ON song_artist FOR EACH ROW BEGIN"
      " INSERT INTO removed_link (idArtist, idMedia, idRole)"
      " VALUES(OLD.idArtist, OLD.idSong, OLD.idRole);"
      " END");
  db.ExecuteQuery(
      "CREATE TRIGGER tgrDeleteAlbumArtist AFTER DELETE ON album_artist FOR EACH ROW BEGIN"
      " INSERT INTO removed_link (idArtist, idMedia, idRole)"
      " VALUES(OLD.idArtist, OLD.idAlbum, -1);"
      " END");
}

void CMusicSchemaManager::CreateViews(CDatabase& db)
{
  CLog::Log(LOGINFO, "create song view");
  db.ExecuteQuery("CREATE VIEW songview AS SELECT "
                  "        song.idSong AS idSong, "
                  "        song.strArtistDisp AS strArtists,"
                  "        song.strArtistSort AS strArtistSort,"
                  "        song.strGenres AS strGenres,"
                  "        strTitle, "
                  "        iTrack, iDuration, "
                  "        song.strReleaseDate as strReleaseDate, "
                  "        song.strOrigReleaseDate as strOrigReleaseDate, "
                  "        song.strDiscSubtitle as strDiscSubtitle, "
                  "        strFileName, "
                  "        strMusicBrainzTrackID, "
                  "        iTimesPlayed, iStartOffset, iEndOffset, "
                  "        lastplayed, "
                  "        song.rating, "
                  "        song.userrating, "
                  "        song.votes, "
                  "        comment, "
                  "        song.idAlbum AS idAlbum, "
                  "        strAlbum, "
                  "        strPath, "
                  "        album.strReleaseStatus as strReleaseStatus,"
                  "        album.bCompilation AS bCompilation,"
                  "        album.bBoxedSet AS bBoxedSet, "
                  "        album.strArtistDisp AS strAlbumArtists,"
                  "        album.strArtistSort AS strAlbumArtistSort,"
                  "        album.strReleaseType AS strAlbumReleaseType,"
                  "        song.mood as mood,"
                  "        song.strReplayGain, "
                  "        iBPM, "
                  "        iBitRate, "
                  "        iSampleRate, "
                  "        iChannels, "
                  "        song.strVideoURL as strVideoURL, "
                  "        album.iAlbumDuration AS iAlbumDuration, "
                  "        album.iDiscTotal as iDiscTotal, "
                  "        song.dateAdded as dateAdded, "
                  "        song.dateNew AS dateNew, "
                  "        song.dateModified AS dateModified "
                  "FROM song"
                  "  JOIN album ON"
                  "    song.idAlbum=album.idAlbum"
                  "  JOIN path ON"
                  "    song.idPath=path.idPath");

  CLog::Log(LOGINFO, "create album view");
  db.ExecuteQuery("CREATE VIEW albumview AS SELECT "
                  "album.idAlbum AS idAlbum, "
                  "strAlbum, "
                  "strMusicBrainzAlbumID, "
                  "strReleaseGroupMBID, "
                  "album.strArtistDisp AS strArtists, "
                  "album.strArtistSort AS strArtistSort, "
                  "album.strGenres AS strGenres, "
                  "album.strReleaseDate as strReleaseDate, "
                  "album.strOrigReleaseDate as strOrigReleaseDate, "
                  "album.bBoxedSet AS bBoxedSet, "
                  "album.strMoods AS strMoods, "
                  "album.strStyles AS strStyles, "
                  "strThemes, "
                  "strReview, "
                  "strLabel, "
                  "strType, "
                  "strReleaseStatus, "
                  "album.strImage as strImage, "
                  "album.fRating, "
                  "album.iUserrating, "
                  "album.iVotes, "
                  "bCompilation, "
                  "bScrapedMBID,"
                  "lastScraped,"
                  "dateAdded, dateNew, dateModified, "
                  "(SELECT ROUND(AVG(song.iTimesPlayed)) FROM song "
                  "WHERE song.idAlbum = album.idAlbum) AS iTimesPlayed, "
                  "strReleaseType, "
                  "iDiscTotal, "
                  "(SELECT MAX(song.lastplayed) FROM song "
                  "WHERE song.idAlbum = album.idAlbum) AS lastplayed, "
                  "iAlbumDuration "
                  "FROM album");

  CLog::Log(LOGINFO, "create artist view");
  db.ExecuteQuery("CREATE VIEW artistview AS SELECT"
                  "  idArtist, strArtist, strSortName, "
                  "  strMusicBrainzArtistID, "
                  "  strType, strGender, strDisambiguation, "
                  "  strBorn, strFormed, strGenres,"
                  "  strMoods, strStyles, strInstruments, "
                  "  strBiography, strDied, strDisbanded, "
                  "  strYearsActive, strImage, "
                  "  bScrapedMBID, lastScraped, "
                  "  dateAdded, dateNew, dateModified "
                  "FROM artist");

  CLog::Log(LOGINFO, "create albumartist view");
  db.ExecuteQuery("CREATE VIEW albumartistview AS SELECT"
                  "  album_artist.idAlbum AS idAlbum, "
                  "  album_artist.idArtist AS idArtist, "
                  "  0 AS idRole, "
                  "  'AlbumArtist' AS strRole, "
                  "  artist.strArtist AS strArtist, "
                  "  artist.strSortName AS strSortName,"
                  "  artist.strMusicBrainzArtistID AS strMusicBrainzArtistID, "
                  "  album_artist.iOrder AS iOrder "
                  "FROM album_artist "
                  "JOIN artist ON "
                  "     album_artist.idArtist = artist.idArtist");

  CLog::Log(LOGINFO, "create songartist view");
  db.ExecuteQuery("CREATE VIEW songartistview AS SELECT"
                  "  song_artist.idSong AS idSong, "
                  "  song_artist.idArtist AS idArtist, "
                  "  song_artist.idRole AS idRole, "
                  "  role.strRole AS strRole, "
                  "  artist.strArtist AS strArtist, "
                  "  artist.strSortName AS strSortName,"
                  "  artist.strMusicBrainzArtistID AS strMusicBrainzArtistID, "
                  "  song_artist.iOrder AS iOrder "
                  "FROM song_artist "
                  "JOIN artist ON "
                  "     song_artist.idArtist = artist.idArtist "
                  "JOIN role ON "
                  "     song_artist.idRole = role.idRole");
}

void CMusicSchemaManager::CreateNativeDBFunctions(CDatabase& db)
{
  // Create native functions in MySQL/MariaDB database only
  if (!StringUtils::EqualsNoCase(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type,
          "mysql"))
    return;
  CLog::Log(LOGINFO, "Create native MySQL/MariaDB functions");
  /* Functions to do the natural number sorting and all ascii symbol char at top adjustments to
     default utf8_general_ci collation that SQLite does via a collation sequence callback
     function to StringUtils::AlphaNumericCompare
     !@todo: the video needs these defined too for sorting in DB, then creation can be made common
  */
  // clang-format off
  // udfFirstNumberPos finds the position of the first digit in a string
  db.ExecuteQuery("DROP FUNCTION IF EXISTS udfFirstNumberPos");
  db.ExecuteQuery("CREATE FUNCTION udfFirstNumberPos (instring VARCHAR(512))\n"
    "RETURNS int \n"
    "LANGUAGE SQL \n"
    "DETERMINISTIC \n"
    "NO SQL \n"
    "SQL SECURITY INVOKER \n"
    "BEGIN \n"
    "  DECLARE position int; \n"
    "  DECLARE tmppos int; \n"
    "  SET position = 5000; \n"
    "  SET tmppos = LOCATE('0', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('1', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('2', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('3', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('4', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('5', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('6', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('7', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('8', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  SET tmppos = LOCATE('9', instring); IF(tmppos > 0 AND tmppos < position) THEN SET position = tmppos; END IF;\n"
    "  IF(position = 5000) THEN RETURN 0; END IF;\n"
    "  RETURN position; \n"
    "END\n");

  // udfSymbolShift adds "/" (the  last symbol before "0"), in front any of the chars input
  db.ExecuteQuery("DROP FUNCTION IF EXISTS udfSymbolShift");
  db.ExecuteQuery("CREATE FUNCTION udfSymbolShift(instring varchar(512), symbolChars char(25))\n"
    "RETURNS varchar(1024)\n"
    "LANGUAGE SQL\n"
    "DETERMINISTIC\n"
    "NO SQL\n"
    "SQL SECURITY INVOKER\n"
    "BEGIN\n"
    "  DECLARE sortString varchar(1024); -- Allow for every char to be symbol\n"
    "  DECLARE i int;\n"
    "  DECLARE symbolCharsLen int;\n"
    "  DECLARE symbol char(1);\n"
    "  SET sortString = instring;\n"
    "  SET i = 1;\n"
    "  SET symbolCharsLen = CHAR_LENGTH(symbolChars);\n"
    "  WHILE(i <= symbolCharsLen) DO\n"
    "    SET symbol = SUBSTRING(symbolChars, i, 1);\n"
    "    SET sortString = REPLACE(sortString, symbol, CONCAT('/', symbol));\n"
    "    SET i = i + 1;\n"
    "  END WHILE;\n"
    "  RETURN sortString;\n"
    "END\n");

  // udfNaturalSortFormat - provide natural number sorting and ascii symbols above numbers
  db.ExecuteQuery("DROP FUNCTION IF EXISTS udfNaturalSortFormat");
  db.ExecuteQuery("CREATE FUNCTION udfNaturalSortFormat(instring varchar(512), numberLength int, "
    "sameOrderChars char(25))\n"
    "RETURNS varchar(1024)\n"
    "LANGUAGE SQL\n"
    "DETERMINISTIC\n"
    "NO SQL\n"
    "SQL SECURITY INVOKER\n"
    "BEGIN\n"
    "  DECLARE sortString varchar(1024);\n"
    "  DECLARE shiftedString varchar(1024);\n"
    "  DECLARE inLength int;\n"
    "  DECLARE shiftedLength int;\n"
    "  DECLARE totalSympadLength int;\n"
    "  DECLARE symbolshifted512 varchar(1024);\n"
    "  DECLARE numStartIndex int; \n"
    "  DECLARE numEndIndex int; \n"
    "  DECLARE padLength int; \n"
    "  DECLARE totalPadLength int; \n"
    "  DECLARE i int; \n"
    "  DECLARE sameOrderCharsLen int;\n"
    "  SET totalPadLength = 0; \n"
    "  SET instring = TRIM(instring);\n"
    "  SET inLength = CHAR_LENGTH(inString);\n"
    "  SET sortString = instring; \n"
    "  SET numStartIndex = udfFirstNumberPos(instring); \n"
    "  SET numEndIndex = 0; \n"
    "  SET i = 1; \n"
    "  SET sameOrderCharsLen = CHAR_LENGTH(sameOrderChars); \n"
    "  WHILE(i <= sameOrderCharsLen) DO \n"
    "    SET sortString = REPLACE(sortString, SUBSTRING(sameOrderChars, i, 1), ' '); \n"
    "    SET i = i + 1; \n"
    "  END WHILE; \n"
    "  WHILE(numStartIndex <> 0) DO \n"
    "    SET numStartIndex = numStartIndex + numEndIndex; \n"
    "    SET numEndIndex = numStartIndex; \n"
    "    WHILE(udfFirstNumberPos(SUBSTRING(instring, numEndIndex, 1)) = 1) DO \n"
    "      SET numEndIndex = numEndIndex + 1; \n"
    "    END WHILE; \n"
    "    SET numEndIndex = numEndIndex - 1; \n"
    "    SET padLength = numberLength - (numEndIndex + 1 - numStartIndex); \n"
    "    IF padLength < 0 THEN \n"
    "      SET padLength = 0; \n"
    "    END IF; \n"
    "    IF inLength + totalPadLength + padlength > 1024 THEN \n"
    "      -- Padding more digits would be too long, pad this one just enough \n"
    "      SET padLength = 1024 - inLength - totalPadLength; \n"
    "      SET numStartIndex = 0; \n"
    "    END IF; \n"
    "    SET sortString = INSERT(sortString, numStartIndex + totalPadLength, 0, REPEAT('0', padLength)); \n"
    "    SET totalPadLength = totalPadLength + padLength; \n"
    "    IF numStartIndex <> 0 THEN \n"
    "      SET numStartIndex = udfFirstNumberPos(RIGHT(instring, inLength - numEndIndex)); \n"
    "    END IF; \n"
    "  END WHILE; \n"
    "  -- Handle symbol order inserting '/' to shift ascii symbols :;<=>?@[\\]^_ `{|}~ above 0 \n"
    "  -- when there is space as this could double string length.  Note '\\' needs escaping \n"
    "  SET numStartIndex = 1; \n"
    "  SET numEndIndex = inLength + totalPadLength; \n"
    "  IF numEndIndex < 1024 THEN \n"
    "    SET shiftedLength = 0; \n"
    "    SET totalSympadLength = 0; \n"
    "    WHILE numStartIndex < numEndIndex AND totalSympadLength < 1024 DO \n"
    "      SET symbolshifted512 = udfSymbolShift(SUBSTRING(sortString, numStartIndex, 512), ':;<=>?@[\\\\]^_`{|}~'); \n"
    "      SET numStartIndex = numStartIndex + 512; \n"
    "      SET shiftedLength = CHAR_LENGTH(symbolshifted512); \n"
    "      IF totalSympadLength = 0 THEN \n"
    "        SET shiftedString = symbolshifted512; \n"
    "      ELSE \n"
    "        IF totalSympadLength + shiftedLength > 1024 THEN \n"
    "          SET shiftedLength = 1024 - totalSympadLength; \n"
    "          SET symbolshifted512 = LEFT(symbolshifted512, shiftedLength); \n"
    "        END IF; \n"
    "        SET shiftedString = CONCAT(shiftedString, symbolshifted512); \n"
    "      END IF; \n"
    "      SET totalSympadLength = totalSympadLength + shiftedLength; \n"
    "    END WHILE; \n"
    "    SET sortString = shiftedString; \n"
    "  END IF; \n"
    "  RETURN sortString; \n"
    "  END\n");
  // clang-format on
}
