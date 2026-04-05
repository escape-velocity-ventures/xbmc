/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicCRUDRepository.h"

#include "Album.h"
#include "Artist.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "MusicDatabase.h"
#include "MusicDatasetHelper.h"
#include "MusicDbUrl.h"
#include "ServiceBroker.h"
#include "Song.h"
#include "URL.h"
#include "Util.h"
#include "dbwrappers/dataset.h"
#include "interfaces/AnnouncementManager.h"
#include "music/MusicLibraryQueue.h"
#include "music/tags/MusicInfoTag.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <chrono>
#include <inttypes.h>
#include <set>
#include <string>
#include <vector>

using namespace MUSIC_INFO;

namespace
{
void AnnounceRemove(const std::string& content, int id)
{
  CVariant data;
  data["type"] = content;
  data["id"] = id;
  if (CMusicLibraryQueue::GetInstance().IsScanningLibrary())
    data["transaction"] = true;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnRemove", data);
}

void AnnounceUpdate(const std::string& content, int id, bool added = false)
{
  CVariant data;
  data["type"] = content;
  data["id"] = id;
  if (added)
    data["added"] = true;
  if (CMusicLibraryQueue::GetInstance().IsScanningLibrary())
    data["transaction"] = true;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnUpdate", data);
}
} // anonymous namespace

CMusicCRUDRepository::CMusicCRUDRepository(CMusicDatabase& db) : m_db(db)
{
}

// ---------------------------------------------------------------------------
// Song CRUD
// ---------------------------------------------------------------------------

int CMusicCRUDRepository::AddSong(const int idSong,
                                  const CDateTime& dtDateNew,
                                  const int idAlbum,
                                  const std::string& strTitle,
                                  const std::string& strMusicBrainzTrackID,
                                  const std::string& strPathAndFileName,
                                  const std::string& strComment,
                                  const std::string& strMood,
                                  const std::string& strThumb,
                                  const std::string& artistDisp,
                                  const std::string& artistSort,
                                  const std::vector<std::string>& genres,
                                  int iTrack,
                                  int iDuration,
                                  const std::string& strReleaseDate,
                                  const std::string& strOrigReleaseDate,
                                  std::string& strDiscSubtitle,
                                  const int iTimesPlayed,
                                  int iStartOffset,
                                  int iEndOffset,
                                  const CDateTime& dtLastPlayed,
                                  float rating,
                                  int userrating,
                                  int votes,
                                  int iBPM,
                                  int iBitRate,
                                  int iSampleRate,
                                  int iChannels,
                                  const std::string& songVideoURL,
                                  const ReplayGain& replayGain)
{
  int idNew = -1;
  std::string strSQL;
  try
  {
    // We need at least the title
    if (strTitle.empty())
      return -1;

    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;

    std::string strPath;
    std::string strFileName;
    m_db.SplitPath(strPathAndFileName, strPath, strFileName);
    int idPath = m_db.AddPath(strPath);

    if (idSong <= 1)
    {
      if (!strMusicBrainzTrackID.empty())
        strSQL = m_db.PrepareSQL("SELECT idSong FROM song WHERE "
                                 "idAlbum = %i AND iTrack=%i AND strMusicBrainzTrackID = '%s'",
                                 idAlbum, iTrack, strMusicBrainzTrackID.c_str());
      else
        strSQL = m_db.PrepareSQL("SELECT idSong FROM song WHERE "
                                 "idAlbum=%i AND strFileName='%s' AND strTitle='%s' AND iTrack=%i "
                                 "AND strMusicBrainzTrackID IS NULL",
                                 idAlbum, strFileName.c_str(), strTitle.c_str(), iTrack);

      if (!m_db.m_pDS->query(strSQL))
        return -1;
    }
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();

      // As all discs in a boxset have to have a title, generate one in the form of 'Disc N'
      bool isBoxset = m_db.IsAlbumBoxset(idAlbum);
      if (isBoxset && strDiscSubtitle.empty())
      {
        int discno = iTrack >> 16;
        strDiscSubtitle = StringUtils::Format(
            "{} {}", CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(427), discno);
      }

      // Validate ISO8601 dates and ensure none missing
      std::string strRelease = strReleaseDate;
      std::string strOriginal = strOrigReleaseDate;
      m_db.NormaliseSongDates(strRelease, strOriginal);

      // Get dateAdded from music file timestamp
      std::string strDateMedia = m_db.GetMediaDateFromFile(strPathAndFileName);

      strSQL = "INSERT INTO song ("
               "idSong, dateNew, idAlbum, idPath, strArtistDisp, "
               "strTitle, iTrack, iDuration, "
               "strReleaseDate, strOrigReleaseDate, iBPM, "
               "iBitrate, iSampleRate, iChannels, "
               "strDiscSubtitle, strFileName, dateAdded,  "
               "strMusicBrainzTrackID, strArtistSort, "
               "iTimesPlayed, iStartOffset, iEndOffset, "
               "lastplayed, rating, userrating, votes, comment, mood, strReplayGain) ";

      if (idSong <= 0)
        // Song ID is autoincremented and dateNew set by trigger
        strSQL += m_db.PrepareSQL("VALUES (NULL, NULL, ");
      else
        //Reuse song Id and original date when the Id added
        strSQL += m_db.PrepareSQL("VALUES (%i, '%s', ", idSong, dtDateNew.GetAsDBDateTime().c_str());

      strSQL +=
          m_db.PrepareSQL("%i, %i, '%s', '%s', %i, %i, '%s', '%s', %i, %i, %i, %i,'%s', '%s', '%s' ",
                          idAlbum, idPath, artistDisp.c_str(), strTitle.c_str(), iTrack, iDuration,
                          strRelease.c_str(), strOriginal.c_str(), iBPM, iBitRate, iSampleRate,
                          iChannels, strDiscSubtitle.c_str(), strFileName.c_str(), strDateMedia.c_str());

      if (strMusicBrainzTrackID.empty())
        strSQL += m_db.PrepareSQL(",NULL");
      else
        strSQL += m_db.PrepareSQL(",'%s'", strMusicBrainzTrackID.c_str());
      if (artistSort.empty() || artistSort.compare(artistDisp) == 0)
        strSQL += m_db.PrepareSQL(",NULL");
      else
        strSQL += m_db.PrepareSQL(",'%s'", artistSort.c_str());

      if (dtLastPlayed.IsValid())
        strSQL += m_db.PrepareSQL(",%i,%i,%i,'%s', %.1f, %i, %i, '%s','%s', '%s')", //
                                  iTimesPlayed, iStartOffset, iEndOffset,
                                  dtLastPlayed.GetAsDBDateTime().c_str(), //
                                  static_cast<double>(rating), userrating, votes, //
                                  strComment.c_str(), strMood.c_str(), replayGain.Get().c_str());
      else
        strSQL += m_db.PrepareSQL(",%i,%i,%i,NULL, %.1f, %i, %i,'%s', '%s', '%s')", //
                                  iTimesPlayed, iStartOffset, iEndOffset, //
                                  static_cast<double>(rating), userrating, votes, //
                                  strComment.c_str(), strMood.c_str(), replayGain.Get().c_str());
      m_db.m_pDS->exec(strSQL);
      if (idSong <= 0)
        idNew = static_cast<int>(m_db.m_pDS->lastinsertid());
      else
        idNew = idSong;
    }
    else
    {
      idNew = m_db.m_pDS->fv("idSong").get_asInt();
      m_db.m_pDS->close();
      UpdateSong(idNew, //
                 strTitle, //
                 strMusicBrainzTrackID, //
                 strPathAndFileName, //
                 strComment, //
                 strMood, //
                 strThumb, //
                 artistDisp, //
                 artistSort, //
                 genres, //
                 iTrack, //
                 iDuration, //
                 strReleaseDate, //
                 strOrigReleaseDate, //
                 strDiscSubtitle, //
                 iTimesPlayed, //
                 iStartOffset, iEndOffset, //
                 dtLastPlayed, //
                 rating, userrating, votes, //
                 replayGain, //
                 iBPM, iBitRate, iSampleRate, iChannels, songVideoURL);
    }
    if (!strThumb.empty())
      m_db.SetArtForItem(idNew, MediaTypeSong, "thumb", strThumb);

    // Song genres added, and genre string updated to use the standardised genre names
    AddSongGenres(idNew, genres);

    AnnounceUpdate(MediaTypeSong, idNew, true);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to addsong ({})", strSQL);
  }
  return idNew;
}

bool CMusicCRUDRepository::GetSong(int idSong, CSong& song)
{
  try
  {
    song.Clear();

    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL =
        m_db.PrepareSQL("SELECT songview.*,songartistview.* FROM songview "
                        " JOIN songartistview ON songview.idSong = songartistview.idSong "
                        " WHERE songview.idSong = %i "
                        " ORDER BY songartistview.idRole, songartistview.iOrder",
                        idSong);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    int songArtistOffset = song_enumCount;

    song = CMusicDatasetHelper::GetSongFromDataset(m_db.m_pDS->get_sql_record());
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

      int idSongArtistRole = record->at(songArtistOffset + artistCredit_idRole).get_asInt();
      if (idSongArtistRole == ROLE_ARTIST)
        song.artistCredits.emplace_back(
            CMusicDatasetHelper::GetArtistCreditFromDataset(record, songArtistOffset));
      else
        song.AppendArtistRole(
            CMusicDatasetHelper::GetArtistRoleFromDataset(record, songArtistOffset));

      m_db.m_pDS->next();
    }
    m_db.m_pDS->close(); // cleanup recordset data
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idSong);
  }

  return false;
}

bool CMusicCRUDRepository::UpdateSong(CSong& song,
                                      bool bArtists /*= true*/,
                                      bool bArtistLinks /*= true*/)
{
  int result = UpdateSong(song.idSong,
                          song.strTitle, //
                          song.strMusicBrainzTrackID, //
                          song.strFileName, //
                          song.strComment, //
                          song.strMood, //
                          song.strThumb, //
                          song.GetArtistString(), //
                          song.GetArtistSort(), //
                          song.genre, //
                          song.iTrack, //
                          song.iDuration, //
                          song.strReleaseDate, //
                          song.strOrigReleaseDate, //
                          song.strDiscSubtitle, //
                          song.iTimesPlayed, //
                          song.iStartOffset, song.iEndOffset, //
                          song.lastPlayed, //
                          song.rating, song.userrating, song.votes, //
                          song.replayGain, //
                          song.iBPM, song.iBitRate, song.iSampleRate, song.iChannels, //
                          song.songVideoURL);
  if (result < 0)
    return false;

  // Replace Song genres and update genre string using the standardised genre names
  AddSongGenres(song.idSong, song.genre);
  if (bArtists)
  {
    //Replace song artists and contributors
    DeleteSongArtistsBySong(song.idSong);
    // Song must have at least one artist so set artist to [Missing]
    if (song.artistCredits.empty())
      AddSongArtist(BLANKARTIST_ID, song.idSong, ROLE_ARTIST, BLANKARTIST_NAME, 0);
    for (auto artistCredit = song.artistCredits.begin(); artistCredit != song.artistCredits.end();
         ++artistCredit)
    {
      artistCredit->idArtist =
          AddArtist(artistCredit->GetArtist(), artistCredit->GetMusicBrainzArtistID(),
                    artistCredit->GetSortName());
      AddSongArtist(artistCredit->idArtist, song.idSong, ROLE_ARTIST, artistCredit->GetArtist(),
                    static_cast<int>(std::distance(song.artistCredits.begin(), artistCredit)));
    }
    // Having added artist credits (maybe with MBID) add the other contributing artists (MBID unknown)
    // and use COMPOSERSORT tag data to provide sort names for artists that are composers
    AddSongContributors(song.idSong, song.GetContributors(), song.GetComposerSort());

    if (bArtistLinks)
      CheckArtistLinksChanged();
  }

  return true;
}

int CMusicCRUDRepository::UpdateSong(int idSong,
                                     const std::string& strTitle,
                                     const std::string& strMusicBrainzTrackID,
                                     const std::string& strPathAndFileName,
                                     const std::string& strComment,
                                     const std::string& strMood,
                                     const std::string& /*strThumb*/, //! @todo implement or remove.
                                     const std::string& artistDisp,
                                     const std::string& artistSort,
                                     const std::vector<std::string>& genres,
                                     int iTrack,
                                     int iDuration,
                                     const std::string& strReleaseDate,
                                     const std::string& strOrigReleaseDate,
                                     const std::string& strDiscSubtitle,
                                     int iTimesPlayed,
                                     int iStartOffset,
                                     int iEndOffset,
                                     const CDateTime& dtLastPlayed,
                                     float rating,
                                     int userrating,
                                     int votes,
                                     const ReplayGain& replayGain,
                                     int iBPM,
                                     int iBitRate,
                                     int iSampleRate,
                                     int iChannels,
                                     const std::string& songVideoURL)
{
  if (idSong < 0)
    return -1;

  std::string strSQL;
  std::string strPath;
  std::string strFileName;
  m_db.SplitPath(strPathAndFileName, strPath, strFileName);
  int idPath = m_db.AddPath(strPath);

  // Validate ISO8601 dates and ensure none missing
  std::string strRelease = strReleaseDate;
  std::string strOriginal = strOrigReleaseDate;
  m_db.NormaliseSongDates(strRelease, strOriginal);

  std::string strDateMedia = m_db.GetMediaDateFromFile(strPathAndFileName);

  strSQL = m_db.PrepareSQL(
      "UPDATE song SET idPath = %i, strArtistDisp = '%s', strGenres = '%s', "
      " strTitle = '%s', iTrack = %i, iDuration = %i, "
      "strReleaseDate = '%s', strOrigReleaseDate = '%s', strDiscSubtitle = '%s', "
      "strFileName = '%s', iBPM = %i, iBitrate = %i, iSampleRate = %i, iChannels = %i, "
      "dateAdded = '%s', strVideoURL = '%s'",
      idPath, artistDisp.c_str(),
      StringUtils::Join(
          genres,
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator)
          .c_str(),
      strTitle.c_str(), iTrack, iDuration, strRelease.c_str(), strOriginal.c_str(),
      strDiscSubtitle.c_str(), strFileName.c_str(), iBPM, iBitRate, iSampleRate, iChannels,
      strDateMedia.c_str(), songVideoURL.c_str());
  if (strMusicBrainzTrackID.empty())
    strSQL += m_db.PrepareSQL(", strMusicBrainzTrackID = NULL");
  else
    strSQL += m_db.PrepareSQL(", strMusicBrainzTrackID = '%s'", strMusicBrainzTrackID.c_str());
  if (artistSort.empty() || artistSort.compare(artistDisp) == 0)
    strSQL += m_db.PrepareSQL(", strArtistSort = NULL");
  else
    strSQL += m_db.PrepareSQL(", strArtistSort = '%s'", artistSort.c_str());

  strSQL += m_db.PrepareSQL(", iStartOffset = %i, iEndOffset = %i, rating = %.1f, userrating = %i, "
                            "votes = %i, comment = '%s', mood = '%s', strReplayGain = '%s' ",
                            iStartOffset, iEndOffset, static_cast<double>(rating), userrating, votes,
                            strComment.c_str(), strMood.c_str(), replayGain.Get().c_str());

  if (dtLastPlayed.IsValid())
    strSQL += m_db.PrepareSQL(", iTimesPlayed = %i, lastplayed = '%s' ", iTimesPlayed,
                              dtLastPlayed.GetAsDBDateTime().c_str());
  else if (iTimesPlayed > 0)
    strSQL += m_db.PrepareSQL(", iTimesPlayed = %i, lastplayed = '%s' ", iTimesPlayed,
                              CDateTime::GetCurrentDateTime().GetAsDBDateTime().c_str());
  else
    strSQL += ", iTimesPlayed = 0, lastplayed = NULL ";
  strSQL += m_db.PrepareSQL("WHERE idSong = %i", idSong);

  bool status = m_db.ExecuteQuery(strSQL);

  if (status)
    AnnounceUpdate(MediaTypeSong, idSong);
  return idSong;
}

bool CMusicCRUDRepository::GetSongByFileName(const std::string& strFileNameAndPath,
                                             CSong& song,
                                             int64_t startOffset)
{
  song.Clear();
  CURL url(strFileNameAndPath);

  if (url.IsProtocol("musicdb"))
  {
    std::string strFile = URIUtils::GetFileName(strFileNameAndPath);
    URIUtils::RemoveExtension(strFile);
    return GetSong(atoi(strFile.c_str()), song);
  }

  if (nullptr == m_db.m_pDB)
    return false;
  if (nullptr == m_db.m_pDS)
    return false;

  std::string strPath;
  std::string strFileName;
  m_db.SplitPath(strFileNameAndPath, strPath, strFileName);
  URIUtils::AddSlashAtEnd(strPath);

  std::string strSQL = m_db.PrepareSQL("SELECT idSong FROM songview "
                                       "WHERE strFileName='%s' AND strPath='%s'",
                                       strFileName.c_str(), strPath.c_str());
  if (startOffset)
    strSQL += m_db.PrepareSQL(" AND iStartOffset=%" PRIi64, startOffset);

  int idSong = m_db.GetSingleValueInt(strSQL);
  if (idSong > 0)
    return GetSong(idSong, song);

  return false;
}

bool CMusicCRUDRepository::GetSongsByPath(const std::string& strPath1,
                                          std::map<std::string, std::vector<CSong>>& songmap,
                                          bool bAppendToMap)
{
  std::string strPath(strPath1);
  try
  {
    if (!URIUtils::HasSlashAtEnd(strPath))
      URIUtils::AddSlashAtEnd(strPath);

    if (!bAppendToMap)
      songmap.clear();

    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL = m_db.PrepareSQL("SELECT * FROM songview "
                                         "WHERE strPath='%s' ORDER BY strFileName",
                                         strPath.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    std::vector<CSong> songs;
    std::string filename;
    while (!m_db.m_pDS->eof())
    {
      CSong song = CMusicDatasetHelper::GetSongFromDataset(m_db.m_pDS->get_sql_record());
      if (!filename.empty() && filename != song.strFileName)
      {
        songmap.try_emplace(filename, songs);
        songs.clear();
      }
      filename = song.strFileName;
      songs.emplace_back(song);
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();
    songmap.try_emplace(filename, songs);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", strPath);
  }

  return false;
}

bool CMusicCRUDRepository::Search(const std::string& search, CFileItemList& items)
{
  auto start = std::chrono::steady_clock::now();
  m_db.SearchArtists(search, items);
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Artist search in {} ms", duration.count());

  start = std::chrono::steady_clock::now();
  m_db.SearchAlbums(search, items);
  end = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Album search in {} ms", duration.count());

  start = std::chrono::steady_clock::now();
  m_db.SearchSongs(search, items);
  end = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  CLog::LogF(LOGDEBUG, "Songs search in {} ms", duration.count());

  return true;
}

bool CMusicCRUDRepository::RemoveSongsFromPath(const std::string& path1,
                                               std::map<std::string, std::vector<CSong>>& songmap,
                                               bool exact)
{
  std::string path(path1);
  m_db.SetLibraryLastUpdated();
  try
  {
    if (!URIUtils::HasSlashAtEnd(path))
      URIUtils::AddSlashAtEnd(path);

    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string where;
    if (exact)
      where = m_db.PrepareSQL(" WHERE strPath='%s'", path.c_str());
    else
      where = m_db.PrepareSQL(" WHERE SUBSTR(strPath,1,%i)='%s'", StringUtils::utf8_strlen(path),
                              path.c_str());
    std::string sql = "SELECT * FROM songview" + where + " ORDER BY strFileName";
    if (!m_db.m_pDS->query(sql))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound > 0)
    {
      std::vector<CSong> songs;
      std::string filename;
      std::vector<std::string> songIds;
      while (!m_db.m_pDS->eof())
      {
        CSong song = CMusicDatasetHelper::GetSongFromDataset(m_db.m_pDS->get_sql_record());
        if (!filename.empty() && filename != song.strFileName)
        {
          songmap.try_emplace(filename, songs);
          songs.clear();
        }
        song.strThumb = m_db.GetArtForItem(song.idSong, MediaTypeSong, "thumb");
        songs.emplace_back(song);
        songIds.push_back(m_db.PrepareSQL("%i", song.idSong));
        filename = song.strFileName;

        m_db.m_pDS->next();
      }
      m_db.m_pDS->close();
      songmap.try_emplace(filename, songs);

      //! @todo move this below the m_pDS->exec block, once UPnP doesn't rely on this anymore
      for (const auto& id : songIds)
        AnnounceRemove(MediaTypeSong, atoi(id.c_str()));

      std::string strIDs = StringUtils::Join(songIds, ",");
      sql = "DELETE FROM song WHERE idSong in (" + strIDs + ")";
      m_db.m_pDS->exec(sql);
    }
    sql = "delete from path" + where;
    m_db.m_pDS->exec(sql);
    return iRowsFound > 0;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", path);
  }
  return false;
}

void CMusicCRUDRepository::CheckArtistLinksChanged()
{
  if (!m_db.m_pDS)
    return;

  std::string strSQL = "SELECT COUNT(1) FROM removed_link ";
  const int iLinks = m_db.GetSingleValueInt(strSQL, *m_db.m_pDS);
  if (iLinks > 0)
  {
    m_db.SetArtistLinksUpdated();
    m_db.DeleteRemovedLinks();
  }
}

bool CMusicCRUDRepository::SetSongUserrating(const std::string& filePath, int userrating)
{
  try
  {
    if (filePath.empty())
      return false;
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    int songID = m_db.GetSongIDFromPath(filePath);
    if (-1 == songID)
      return false;

    return SetSongUserrating(songID, userrating);
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({},{}) failed", filePath, userrating);
  }
  return false;
}

bool CMusicCRUDRepository::SetSongUserrating(int idSong, int userrating)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string sql =
        m_db.PrepareSQL("UPDATE song SET userrating ='%i' WHERE idSong = %i", userrating, idSong);
    m_db.m_pDS->exec(sql);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({},{}) failed", idSong, userrating);
  }
  return false;
}

bool CMusicCRUDRepository::SetSongVotes(const std::string& filePath, int votes)
{
  try
  {
    if (filePath.empty())
      return false;
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    int songID = m_db.GetSongIDFromPath(filePath);
    if (-1 == songID)
      return false;

    std::string sql = m_db.PrepareSQL("UPDATE song SET votes ='%i' WHERE idSong = %i", votes, songID);

    m_db.m_pDS->exec(sql);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({},{}) failed", filePath, votes);
  }
  return false;
}

int CMusicCRUDRepository::GetSongByArtistAndAlbumAndTitle(const std::string& strArtist,
                                                          const std::string& strAlbum,
                                                          const std::string& strTitle)
{
  try
  {
    std::string strSQL =
        m_db.PrepareSQL("SELECT idSong FROM songview "
                        "WHERE strArtists LIKE '%s' AND strAlbum LIKE '%s' AND strTitle LIKE '%s'",
                        strArtist.c_str(), strAlbum.c_str(), strTitle.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return -1;
    }
    int lResult = m_db.m_pDS->fv(0).get_asInt();
    m_db.m_pDS->close();
    return lResult;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({},{},{}) failed", strArtist, strAlbum, strTitle);
  }

  return -1;
}

// ---------------------------------------------------------------------------
// Album CRUD
// ---------------------------------------------------------------------------

bool CMusicCRUDRepository::AddAlbum(CAlbum& album, int idSource)
{
  m_db.BeginTransaction();
  m_db.SetLibraryLastUpdated();

  album.idAlbum = AddAlbum(album.strAlbum, //
                           album.strMusicBrainzAlbumID, //
                           album.strReleaseGroupMBID, //
                           album.GetAlbumArtistString(), //
                           album.GetAlbumArtistSort(), //
                           album.GetGenreString(), //
                           album.strReleaseDate, //
                           album.strOrigReleaseDate, //
                           album.bBoxedSet, //
                           album.strLabel, //
                           album.strType, //
                           album.strReleaseStatus, //
                           album.bCompilation, //
                           album.releaseType);

  // Add the album artists
  if (album.artistCredits.empty())
    AddAlbumArtist(BLANKARTIST_ID, album.idAlbum, BLANKARTIST_NAME, 0);
  for (auto artistCredit = album.artistCredits.begin(); artistCredit != album.artistCredits.end();
       ++artistCredit)
  {
    artistCredit->idArtist =
        AddArtist(artistCredit->GetArtist(), artistCredit->GetMusicBrainzArtistID(),
                  artistCredit->GetSortName());
    AddAlbumArtist(artistCredit->idArtist, album.idAlbum, artistCredit->GetArtist(),
                   static_cast<int>(std::distance(album.artistCredits.begin(), artistCredit)));
  }

  // Add songs
  for (auto song = album.songs.begin(); song != album.songs.end(); ++song)
  {
    song->idAlbum = album.idAlbum;

    bool finished = false;
    int index = 0;

    while (!finished)
    {
      if (!song->m_chapters.empty())
      {
        const ChapterDetails& chapter{song->m_chapters[index]};
        if (StringUtils::IsNaturalNumber(chapter.name) || chapter.name.empty())
          song->strTitle = StringUtils::Format(
              "{} {:03}",
              CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(21396),
              index + 1);
        else
          song->strTitle = chapter.name;

        song->iStartOffset = static_cast<int>(chapter.startTimeMs.count());
        song->iEndOffset = static_cast<int>(chapter.endTimeMs.count());
        song->iDuration = static_cast<int>(
            CUtil::ConvertMilliSecsToSecsInt(song->iEndOffset - song->iStartOffset));
        song->iTrack = (1 << 16) + index + 1;
        song->idSong = -1;
      }

      ++index;
      song->idSong = AddSong(song->idSong, //
                             song->dateNew, //
                             song->idAlbum, //
                             song->strTitle, //
                             song->strMusicBrainzTrackID, //
                             song->strFileName, //
                             song->strComment, //
                             song->strMood, //
                             song->strThumb, //
                             song->GetArtistString(), //
                             song->GetArtistSort(), //
                             song->genre, //
                             song->iTrack, //
                             song->iDuration, //
                             song->strReleaseDate, //
                             song->strOrigReleaseDate, //
                             song->strDiscSubtitle, //
                             song->iTimesPlayed, //
                             song->iStartOffset, song->iEndOffset, //
                             song->lastPlayed, //
                             song->rating, //
                             song->userrating, //
                             song->votes, //
                             song->iBPM, song->iBitRate, song->iSampleRate, song->iChannels, //
                             song->songVideoURL, //
                             song->replayGain);

      if (song->artistCredits.empty())
        AddSongArtist(BLANKARTIST_ID, song->idSong, ROLE_ARTIST, BLANKARTIST_NAME, 0);

      for (auto artistCredit = song->artistCredits.begin();
           artistCredit != song->artistCredits.end(); ++artistCredit)
      {
        artistCredit->idArtist =
            AddArtist(artistCredit->GetArtist(), artistCredit->GetMusicBrainzArtistID(),
                      artistCredit->GetSortName());
        AddSongArtist(
            artistCredit->idArtist, song->idSong, ROLE_ARTIST,
            artistCredit->GetArtist(),
            static_cast<int>(std::distance(song->artistCredits.begin(), artistCredit)));
      }
      AddSongContributors(song->idSong, song->GetContributors(), song->GetComposerSort());

      finished = index >= static_cast<int>(song->m_chapters.size());
    }
  }

  std::string strSQL;
  strSQL = m_db.PrepareSQL("SELECT SUM(iDuration) FROM song WHERE idAlbum = %i", album.idAlbum);
  int albumDuration = m_db.GetSingleValueInt(strSQL);
  m_db.m_pDS->exec(m_db.PrepareSQL("UPDATE album SET iAlbumDuration = %i WHERE idAlbum = %i",
                                    albumDuration, album.idAlbum));

  if (idSource > 0)
    m_db.AddAlbumSource(album.idAlbum, idSource);
  else
  {
    m_db.AddAlbumSources(album.idAlbum, album.strPath);
  }

  for (const auto& [type, url] : album.art)
    m_db.SetArtForItem(album.idAlbum, MediaTypeAlbum, type, url);

  m_db.m_pDS->exec(
      m_db.PrepareSQL("UPDATE album SET iDisctotal = (SELECT COUNT(DISTINCT iTrack >> 16) FROM song "
                      "WHERE song.idAlbum = album.idAlbum) WHERE idAlbum = %i",
                      album.idAlbum));
  if (!album.bBoxedSet && !album.bCompilation)
  {
    strSQL = m_db.PrepareSQL("SELECT COUNT(DISTINCT strDiscSubtitle) FROM song WHERE song.idAlbum = %i",
                             album.idAlbum);
    int numTitles = m_db.GetSingleValueInt(strSQL);
    if (numTitles >= 3)
    {
      strSQL = m_db.PrepareSQL("UPDATE album SET bBoxedSet=1 WHERE album.idAlbum=%i", album.idAlbum);
      m_db.m_pDS->exec(strSQL);
    }
  }
  m_db.m_pDS->exec(m_db.PrepareSQL("UPDATE album SET strReleaseDate = (SELECT DISTINCT strReleaseDate "
                                    "FROM song WHERE song.idAlbum = album.idAlbum LIMIT 1) WHERE idAlbum = %i",
                                    album.idAlbum));
  m_db.m_pDS->exec(
      m_db.PrepareSQL("UPDATE album SET strOrigReleaseDate = (SELECT DISTINCT strOrigReleaseDate "
                      "FROM song WHERE song.idAlbum = album.idAlbum LIMIT 1) WHERE idAlbum = %i",
                      album.idAlbum));

  std::string albumdateadded =
      m_db.GetSingleValue("song", "MAX(dateAdded)", m_db.PrepareSQL("idAlbum = %i", album.idAlbum));
  m_db.m_pDS->exec(m_db.PrepareSQL("UPDATE album SET dateAdded = '%s' WHERE idAlbum = %i",
                                    albumdateadded.c_str(), album.idAlbum));

  std::vector<std::string> artistIDs;
  GetArtistsByAlbum(album.idAlbum, artistIDs);
  std::string strIDs = "(" + StringUtils::Join(artistIDs, ",") + ")";
  strSQL = m_db.PrepareSQL("UPDATE artist SET dateAdded = '%s' "
                           "WHERE idArtist IN %s AND (dateAdded < '%s' OR dateAdded IS NULL)",
                           albumdateadded.c_str(), strIDs.c_str(), albumdateadded.c_str());
  m_db.m_pDS->exec(strSQL);

  m_db.CommitTransaction();
  return true;
}

bool CMusicCRUDRepository::UpdateAlbum(CAlbum& album)
{
  m_db.BeginTransaction();
  m_db.SetLibraryLastUpdated();

  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  if (album.bBoxedSet)
  {
    bool isBoxset = m_db.IsAlbumBoxset(album.idAlbum);
    if (!isBoxset)
    {
      bool canBeBoxset = false;
      std::string strSQL;
      strSQL = m_db.PrepareSQL("SELECT iDiscTotal FROM album WHERE idAlbum = %i", album.idAlbum);
      int numDiscs = m_db.GetSingleValueInt(strSQL);
      if (numDiscs >= 2)
      {
        canBeBoxset = true;
        for (int discValue = 1; discValue <= numDiscs; discValue++)
        {
          strSQL =
              m_db.PrepareSQL("SELECT DISTINCT strDiscSubtitle FROM song WHERE song.idAlbum = %i AND "
                              "song.iTrack >> 16 = %i",
                              album.idAlbum, discValue);
          std::string currentTitle = m_db.GetSingleValue(strSQL);
          if (currentTitle.empty())
          {
            currentTitle = StringUtils::Format(
                "{} {}", CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(427),
                discValue);
            strSQL =
                m_db.PrepareSQL("UPDATE song SET strDiscSubtitle = '%s' WHERE song.idAlbum = %i AND "
                                "song.iTrack >> 16 = %i",
                                currentTitle.c_str(), album.idAlbum, discValue);
            m_db.ExecuteQuery(strSQL);
          }
        }
      }
      if (!canBeBoxset && album.bBoxedSet)
      {
        CLog::Log(LOGINFO, "Album with id [{}] does not meet the requirements for a boxset.",
                  album.idAlbum);
        album.bBoxedSet = false;
      }
    }
  }
  UpdateAlbum(album.idAlbum, album.strAlbum, album.strMusicBrainzAlbumID, //
              album.strReleaseGroupMBID, //
              album.GetAlbumArtistString(), album.GetAlbumArtistSort(), //
              album.GetGenreString(), //
              StringUtils::Join(album.moods, itemSeparator), //
              StringUtils::Join(album.styles, itemSeparator), //
              StringUtils::Join(album.themes, itemSeparator), //
              album.strReview, //
              album.thumbURL.GetData(), //
              album.strLabel, //
              album.strType, //
              album.strReleaseStatus, //
              album.fRating, album.iUserrating, album.iVotes, //
              album.strReleaseDate, //
              album.strOrigReleaseDate, //
              album.bBoxedSet, //
              album.bCompilation, //
              album.releaseType, //
              album.bScrapedMBID);

  if (!album.bArtistSongMerge)
  {
    for (const auto& artistCredit : album.artistCredits)
      UpdateArtistScrapedMBID(artistCredit.GetArtistId(), artistCredit.GetMusicBrainzArtistID());
  }
  else
  {
    DeleteAlbumArtistsByAlbum(album.idAlbum);
    if (album.artistCredits.empty())
      AddAlbumArtist(BLANKARTIST_ID, album.idAlbum, BLANKARTIST_NAME, 0);
    for (auto artistCredit = album.artistCredits.begin(); artistCredit != album.artistCredits.end();
         ++artistCredit)
    {
      artistCredit->idArtist =
          AddArtist(artistCredit->GetArtist(), artistCredit->GetMusicBrainzArtistID(),
                    artistCredit->GetSortName(), true);
      AddAlbumArtist(artistCredit->idArtist, album.idAlbum, artistCredit->GetArtist(),
                     static_cast<int>(std::distance(album.artistCredits.begin(), artistCredit)));
    }
    int albumDuration = 0;
    for (auto& song : album.songs)
    {
      UpdateSong(song);
      albumDuration += song.iDuration;
    }
    if (albumDuration > 0)
      m_db.m_pDS->exec(m_db.PrepareSQL("UPDATE album SET iAlbumDuration = %i WHERE album.idAlbum = %i",
                                        albumDuration, album.idAlbum));
  }

  if (!album.art.empty())
    m_db.SetArtForItem(album.idAlbum, MediaTypeAlbum, album.art);

  CheckArtistLinksChanged();

  m_db.CommitTransaction();
  return true;
}

int CMusicCRUDRepository::AddAlbum(const std::string& strAlbum,
                                   const std::string& strMusicBrainzAlbumID,
                                   const std::string& strReleaseGroupMBID,
                                   const std::string& strArtist,
                                   const std::string& strArtistSort,
                                   const std::string& strGenre,
                                   const std::string& strReleaseDate,
                                   const std::string& strOrigReleaseDate,
                                   bool bBoxedSet,
                                   const std::string& strRecordLabel,
                                   const std::string& strType,
                                   const std::string& strReleaseStatus,
                                   bool bCompilation,
                                   ReleaseType releaseType)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;

    if (!strMusicBrainzAlbumID.empty())
      strSQL = m_db.PrepareSQL("SELECT * FROM album WHERE strMusicBrainzAlbumID = '%s'",
                               strMusicBrainzAlbumID.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT * FROM album "
                               "WHERE strArtistDisp LIKE '%s' AND strAlbum LIKE '%s' "
                               "AND strMusicBrainzAlbumID IS NULL",
                               strArtist.c_str(), strAlbum.c_str());
    m_db.m_pDS->query(strSQL);
    std::string strCheckFlag = strType;
    StringUtils::ToLower(strCheckFlag);
    if (strCheckFlag.find("boxset") != std::string::npos)
      bBoxedSet = true;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      strSQL =
          m_db.PrepareSQL("INSERT INTO album (idAlbum, strAlbum, strArtistDisp, strGenres, "
                          "strReleaseDate, strOrigReleaseDate, bBoxedSet, "
                          "strLabel, strType, strReleaseStatus, bCompilation, strReleaseType,  "
                          "strMusicBrainzAlbumID, "
                          "strReleaseGroupMBID, strArtistSort) "
                          "values(NULL, '%s', '%s', '%s', '%s', '%s', %i, '%s', '%s', '%s', %i, '%s'",
                          strAlbum.c_str(), strArtist.c_str(), strGenre.c_str(), //
                          strReleaseDate.c_str(), strOrigReleaseDate.c_str(), bBoxedSet, //
                          strRecordLabel.c_str(), strType.c_str(), strReleaseStatus.c_str(), //
                          bCompilation, CAlbum::ReleaseTypeToString(releaseType).c_str());

      if (strMusicBrainzAlbumID.empty())
        strSQL += m_db.PrepareSQL(", NULL");
      else
        strSQL += m_db.PrepareSQL(",'%s'", strMusicBrainzAlbumID.c_str());
      if (strReleaseGroupMBID.empty())
        strSQL += m_db.PrepareSQL(", NULL");
      else
        strSQL += m_db.PrepareSQL(",'%s'", strReleaseGroupMBID.c_str());
      if (strArtistSort.empty() || strArtistSort.compare(strArtist) == 0)
        strSQL += m_db.PrepareSQL(", NULL");
      else
        strSQL += m_db.PrepareSQL(", '%s'", strArtistSort.c_str());
      strSQL += ")";
      m_db.m_pDS->exec(strSQL);

      return static_cast<int>(m_db.m_pDS->lastinsertid());
    }
    else
    {
      int idAlbum = m_db.m_pDS->fv("idAlbum").get_asInt();
      m_db.m_pDS->close();

      strSQL = "UPDATE album SET ";
      if (!strMusicBrainzAlbumID.empty())
        strSQL += m_db.PrepareSQL("strAlbum = '%s', strArtistDisp = '%s', ", //
                                  strAlbum.c_str(), strArtist.c_str());
      if (strReleaseGroupMBID.empty())
        strSQL += m_db.PrepareSQL(" strReleaseGroupMBID = NULL,");
      else
        strSQL += m_db.PrepareSQL(" strReleaseGroupMBID ='%s', ", strReleaseGroupMBID.c_str());
      if (strArtistSort.empty() || strArtistSort.compare(strArtist) == 0)
        strSQL += m_db.PrepareSQL(" strArtistSort = NULL");
      else
        strSQL += m_db.PrepareSQL(" strArtistSort = '%s'", strArtistSort.c_str());

      strSQL +=
          m_db.PrepareSQL(", strGenres = '%s', strReleaseDate= '%s', strOrigReleaseDate= '%s', "
                          "bBoxedSet=%i, strLabel = '%s', strType = '%s', strReleaseStatus = '%s', "
                          "bCompilation=%i, strReleaseType = '%s', "
                          "lastScraped = NULL "
                          "WHERE idAlbum=%i",
                          strGenre.c_str(), strReleaseDate.c_str(), strOrigReleaseDate.c_str(), //
                          bBoxedSet, strRecordLabel.c_str(), strType.c_str(), strReleaseStatus.c_str(),
                          bCompilation, CAlbum::ReleaseTypeToString(releaseType).c_str(), //
                          idAlbum);
      m_db.m_pDS->exec(strSQL);
      DeleteAlbumArtistsByAlbum(idAlbum);
      m_db.DeleteAlbumSources(idAlbum);
      return idAlbum;
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed with query ({})", strSQL);
  }

  return -1;
}

bool CMusicCRUDRepository::GetAlbum(int idAlbum, CAlbum& album, bool getSongs /* = true */)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    if (idAlbum == -1)
      return false;

    std::string sql;
    sql = m_db.PrepareSQL("SELECT albumview.*,albumartistview.* "
                          " FROM albumview "
                          " JOIN albumartistview ON albumview.idAlbum = albumartistview.idAlbum "
                          " WHERE albumview.idAlbum = %ld "
                          " ORDER BY albumartistview.iOrder",
                          idAlbum);

    CLog::Log(LOGDEBUG, "{}", sql);
    if (!m_db.m_pDS->query(sql))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    int albumArtistOffset = album_enumCount;

    album = CMusicDatasetHelper::GetAlbumFromDataset(m_db.m_pDS->get_sql_record(), 0, true);
    while (!m_db.m_pDS->eof())
    {
      const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();
      album.artistCredits.push_back(
          CMusicDatasetHelper::GetArtistCreditFromDataset(record, albumArtistOffset));
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    if (getSongs)
    {
      sql = m_db.PrepareSQL("SELECT songview.*, songartistview.*"
                            " FROM songview "
                            " JOIN songartistview ON songview.idSong = songartistview.idSong "
                            " WHERE songview.idAlbum = %ld "
                            " ORDER BY songview.iTrack, songartistview.idRole, songartistview.iOrder",
                            idAlbum);

      CLog::Log(LOGDEBUG, "{}", sql);
      if (!m_db.m_pDS->query(sql))
        return false;
      if (m_db.m_pDS->num_rows() == 0)
      {
        m_db.m_pDS->close();
        return false;
      }

      int songArtistOffset = song_enumCount;
      std::set<int> songs;
      while (!m_db.m_pDS->eof())
      {
        const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();

        int idSong = record->at(song_idSong).get_asInt();
        if (!songs.contains(idSong))
        {
          album.songs.emplace_back(CMusicDatasetHelper::GetSongFromDataset(record));
          songs.insert(idSong);
        }

        int idSongArtistRole = record->at(songArtistOffset + artistCredit_idRole).get_asInt();
        if (idSongArtistRole == ROLE_ARTIST)
          album.songs.back().artistCredits.emplace_back(
              CMusicDatasetHelper::GetArtistCreditFromDataset(record, songArtistOffset));
        else
          album.songs.back().AppendArtistRole(
              CMusicDatasetHelper::GetArtistRoleFromDataset(record, songArtistOffset));

        m_db.m_pDS->next();
      }
      m_db.m_pDS->close();
    }

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idAlbum);
  }

  return false;
}

int CMusicCRUDRepository::UpdateAlbum(int idAlbum,
                                      const std::string& strAlbum,
                                      const std::string& strMusicBrainzAlbumID,
                                      const std::string& strReleaseGroupMBID,
                                      const std::string& strArtist,
                                      const std::string& strArtistSort,
                                      const std::string& strGenre,
                                      const std::string& strMoods,
                                      const std::string& strStyles,
                                      const std::string& strThemes,
                                      const std::string& strReview,
                                      const std::string& strImage,
                                      const std::string& strLabel,
                                      const std::string& strType,
                                      const std::string& strReleaseStatus,
                                      float fRating,
                                      int iUserrating,
                                      int iVotes,
                                      const std::string& strReleaseDate,
                                      const std::string& strOrigReleaseDate,
                                      bool bBoxedSet,
                                      bool bCompilation,
                                      ReleaseType releaseType,
                                      bool bScrapedMBID)
{
  if (idAlbum < 0)
    return -1;

  std::string strImageURLs = strImage;
  if (StringUtils::EqualsNoCase(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type,
          "mysql"))
    m_db.TrimImageURLs(strImageURLs, 65535);

  std::string strSQL;
  strSQL = m_db.PrepareSQL("UPDATE album SET "
                           " strAlbum = '%s', strArtistDisp = '%s', strGenres = '%s', "
                           " strMoods = '%s', strStyles = '%s', strThemes = '%s', "
                           " strReview = '%s', strImage = '%s', strLabel = '%s', "
                           " strType = '%s', fRating = %f, iUserrating = %i, iVotes = %i,"
                           " strReleaseDate= '%s', strOrigReleaseDate= '%s', "
                           " bBoxedSet = %i, bCompilation = %i,"
                           " strReleaseType = '%s', strReleaseStatus = '%s', "
                           " lastScraped = '%s', bScrapedMBID = %i",
                           strAlbum.c_str(), strArtist.c_str(), strGenre.c_str(), //
                           strMoods.c_str(), strStyles.c_str(), strThemes.c_str(), //
                           strReview.c_str(), strImageURLs.c_str(), strLabel.c_str(), //
                           strType.c_str(), static_cast<double>(fRating), iUserrating, iVotes, //
                           strReleaseDate.c_str(), strOrigReleaseDate.c_str(), //
                           bBoxedSet, bCompilation, //
                           CAlbum::ReleaseTypeToString(releaseType).c_str(), strReleaseStatus.c_str(), //
                           CDateTime::GetUTCDateTime().GetAsDBDateTime().c_str(), bScrapedMBID);
  if (strMusicBrainzAlbumID.empty())
    strSQL += m_db.PrepareSQL(", strMusicBrainzAlbumID = NULL");
  else
    strSQL += m_db.PrepareSQL(", strMusicBrainzAlbumID = '%s'", strMusicBrainzAlbumID.c_str());
  if (strReleaseGroupMBID.empty())
    strSQL += m_db.PrepareSQL(", strReleaseGroupMBID = NULL");
  else
    strSQL += m_db.PrepareSQL(", strReleaseGroupMBID = '%s'", strReleaseGroupMBID.c_str());
  if (strArtistSort.empty() || strArtistSort.compare(strArtist) == 0)
    strSQL += m_db.PrepareSQL(", strArtistSort = NULL");
  else
    strSQL += m_db.PrepareSQL(", strArtistSort = '%s'", strArtistSort.c_str());

  strSQL += m_db.PrepareSQL(" WHERE idAlbum = %i", idAlbum);

  bool status = m_db.ExecuteQuery(strSQL);
  if (status)
    AnnounceUpdate(MediaTypeAlbum, idAlbum);
  return idAlbum;
}

bool CMusicCRUDRepository::ClearAlbumLastScrapedTime(int idAlbum)
{
  std::string strSQL =
      m_db.PrepareSQL("UPDATE album SET lastScraped = NULL WHERE idAlbum = %i", idAlbum);
  return m_db.ExecuteQuery(strSQL);
}

bool CMusicCRUDRepository::HasAlbumBeenScraped(int idAlbum) const
{
  std::string strSQL =
      m_db.PrepareSQL("SELECT idAlbum FROM album WHERE idAlbum = %i AND lastScraped IS NULL", idAlbum);
  return m_db.GetSingleValue(strSQL).empty();
}

int CMusicCRUDRepository::GetAlbumIdByPath(const std::string& strPath)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL = m_db.PrepareSQL("SELECT DISTINCT idAlbum FROM song "
                                         "JOIN path ON song.idPath = path.idPath "
                                         "WHERE path.strPath='%s'",
                                         strPath.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();

    int idAlbum = -1;
    if (iRowsFound == 1)
      idAlbum = m_db.m_pDS->fv(0).get_asInt();

    m_db.m_pDS->close();

    return idAlbum;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", strPath);
  }

  return -1;
}

bool CMusicCRUDRepository::GetAlbumFromSong(int idSong, CAlbum& album)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL = m_db.PrepareSQL("SELECT albumview.* FROM song "
                                         "JOIN albumview on song.idAlbum = albumview.idAlbum "
                                         "WHERE song.idSong='%i'",
                                         idSong);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return false;
    }

    album = CMusicDatasetHelper::GetAlbumFromDataset(m_db.m_pDS.get());

    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

int CMusicCRUDRepository::GetAlbumByName(const std::string& strAlbum, const std::string& strArtist)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL;
    if (strArtist.empty())
      strSQL =
          m_db.PrepareSQL("SELECT idAlbum FROM album WHERE album.strAlbum LIKE '%s'", strAlbum.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT idAlbum FROM album "
                               "WHERE album.strAlbum LIKE '%s' AND album.strArtistDisp LIKE '%s'",
                               strAlbum.c_str(), strArtist.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return -1;
    }
    return m_db.m_pDS->fv("idAlbum").get_asInt();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

int CMusicCRUDRepository::GetAlbumByName(const std::string& strAlbum,
                                         const std::vector<std::string>& artist)
{
  return GetAlbumByName(
      strAlbum,
      StringUtils::Join(
          artist,
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator));
}

bool CMusicCRUDRepository::GetMatchingMusicVideoAlbum(const std::string& strAlbum,
                                                      const std::string& strArtist,
                                                      int& idAlbum,
                                                      std::string& strReview)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL;
    if (strArtist.empty())
      strSQL = m_db.PrepareSQL("SELECT idAlbum, strReview FROM album WHERE album.strAlbum LIKE '%s'",
                               strAlbum.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT idAlbum, strReview FROM album "
                               "WHERE album.strAlbum LIKE '%s' AND album.strArtistDisp LIKE '%s'",
                               strAlbum.c_str(), strArtist.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound > 0)
    {
      idAlbum = m_db.m_pDS->fv("idAlbum").get_asInt();
      strReview = m_db.m_pDS->fv("strReview").get_asString();
      return true;
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicCRUDRepository::SearchAlbumsByArtistName(const std::string& strArtist,
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
      CAlbum album = CMusicDatasetHelper::GetAlbumFromDataset(m_db.m_pDS.get());
      std::string path = StringUtils::Format("musicdb://albums/{}/", album.idAlbum);
      auto pItem{std::make_shared<CFileItem>(path, album)};
      std::string label =
          StringUtils::Format("{} ({})", album.strAlbum, pItem->GetMusicInfoTag()->GetYear());
      pItem->SetLabel(label);
      items.Add(std::move(pItem));
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

int CMusicCRUDRepository::GetAlbumByMatch(const CAlbum& album)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB || nullptr == m_db.m_pDS)
      return false;
    if (!album.strMusicBrainzAlbumID.empty())
      strSQL = m_db.PrepareSQL("SELECT idAlbum FROM album WHERE strMusicBrainzAlbumID = '%s'",
                               album.strMusicBrainzAlbumID.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT idAlbum FROM album "
                               "WHERE strArtistDisp LIKE '%s' AND strAlbum LIKE '%s' "
                               "AND strMusicBrainzAlbumID IS NULL",
                               album.GetAlbumArtistString().c_str(), album.strAlbum.c_str());
    m_db.m_pDS->query(strSQL);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return GetAlbumByName(album.strAlbum, album.GetAlbumArtistString());
    }
    int lResult = m_db.m_pDS->fv("idAlbum").get_asInt();
    m_db.m_pDS->close();
    return lResult;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed to execute {}", strSQL);
  }
  return -1;
}

std::string CMusicCRUDRepository::GetAlbumById(int id) const
{
  return m_db.GetSingleValue("album", "strAlbum", m_db.PrepareSQL("idAlbum=%i", id));
}

std::string CMusicCRUDRepository::GetAlbumDiscTitle(int idAlbum, int idDisc) const
{
  std::string disctitle;
  std::string albumtitle;
  if (idAlbum > 0)
    albumtitle = GetAlbumById(idAlbum);
  if (idDisc > 0)
  {
    disctitle = m_db.GetSingleValue("song", "strDiscSubtitle",
                                    m_db.PrepareSQL("idAlbum = %i AND iTrack >> 16 = %i", idAlbum, idDisc));
    if (disctitle.empty())
      disctitle = StringUtils::Format(
          "{} {}", CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(427),
          idDisc);
    if (albumtitle.empty())
      albumtitle = disctitle;
    else
      albumtitle = albumtitle + " - " + disctitle;
  }
  return albumtitle;
}

bool CMusicCRUDRepository::SetAlbumUserrating(const int idAlbum, int userrating)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    if (-1 == idAlbum)
      return false;
    std::string sql =
        m_db.PrepareSQL("UPDATE album SET iUserrating='%i' WHERE idAlbum = %i", userrating, idAlbum);
    m_db.m_pDS->exec(sql);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({},{}) failed", idAlbum, userrating);
  }
  return false;
}

int CMusicCRUDRepository::GetAlbumDiscsCount(int idAlbum) const
{
  std::string strSQL = m_db.PrepareSQL("SELECT iDiscTotal FROM album WHERE album.idAlbum = %i", idAlbum);
  return m_db.GetSingleValueInt(strSQL);
}

// ---------------------------------------------------------------------------
// Artist CRUD
// ---------------------------------------------------------------------------

bool CMusicCRUDRepository::UpdateArtist(const CArtist& artist)
{
  m_db.SetLibraryLastUpdated();

  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  UpdateArtist(artist.idArtist, //
               artist.strArtist, //
               artist.strSortName, //
               artist.strMusicBrainzArtistID, //
               artist.bScrapedMBID, //
               artist.strType, //
               artist.strGender, //
               artist.strDisambiguation, //
               artist.strBorn, //
               artist.strFormed, //
               StringUtils::Join(artist.genre, itemSeparator), //
               StringUtils::Join(artist.moods, itemSeparator), //
               StringUtils::Join(artist.styles, itemSeparator), //
               StringUtils::Join(artist.instruments, itemSeparator), //
               artist.strBiography, //
               artist.strDied, //
               artist.strDisbanded, //
               StringUtils::Join(artist.yearsActive, itemSeparator), //
               artist.thumbURL.GetData());

  DeleteArtistDiscography(artist.idArtist);
  for (const auto& disc : artist.discography)
  {
    AddArtistDiscography(artist.idArtist, disc);
  }

  if (!DeleteArtistVideoLinks(artist.idArtist))
    CLog::Log(LOGERROR, "MusicDatabase:  Error deleting ArtistVideoLinks");

  AddArtistVideoLinks(artist);

  if (!artist.art.empty())
    m_db.SetArtForItem(artist.idArtist, MediaTypeArtist, artist.art);

  return true;
}

int CMusicCRUDRepository::AddArtist(const std::string& strArtist,
                                    const std::string& strMusicBrainzArtistID,
                                    const std::string& strSortName,
                                    bool bScrapedMBID /* = false*/)
{
  std::string strSQL;
  int idArtist = AddArtist(strArtist, strMusicBrainzArtistID, bScrapedMBID);
  if (idArtist < 0 || strSortName.empty())
    return idArtist;

  try
  {
    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;

    strSQL = m_db.PrepareSQL("SELECT strArtist, strSortName FROM artist WHERE idArtist = %i", idArtist);
    m_db.m_pDS->query(strSQL);
    if (m_db.m_pDS->num_rows() != 1)
    {
      m_db.m_pDS->close();
      return -1;
    }
    const std::string strArtistName{m_db.m_pDS->fv("strArtist").get_asString()};
    const std::string strArtistSort{m_db.m_pDS->fv("strSortName").get_asString()};
    m_db.m_pDS->close();

    if (!strArtistSort.empty())
    {
      if (strSortName.compare(strArtistName) == 0)
        m_db.m_pDS->exec(
            m_db.PrepareSQL("UPDATE artist SET strSortName = NULL WHERE idArtist = %i", idArtist));
    }
    else if (strSortName.compare(strArtistName) != 0)
      m_db.m_pDS->exec(m_db.PrepareSQL("UPDATE artist SET strSortName = '%s' WHERE idArtist = %i",
                                        strSortName.c_str(), idArtist));

    return idArtist;
  }

  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to addartist with sortname ({})", strSQL);
  }

  return -1;
}

int CMusicCRUDRepository::AddArtist(const std::string& strArtist,
                                    const std::string& strMusicBrainzArtistID,
                                    bool bScrapedMBID /* = false*/)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;

    // 1) MusicBrainz
    if (!strMusicBrainzArtistID.empty())
    {
      strSQL =
          m_db.PrepareSQL("SELECT idArtist, strArtist FROM artist WHERE strMusicBrainzArtistID = '%s'",
                          strMusicBrainzArtistID.c_str());
      m_db.m_pDS->query(strSQL);
      if (m_db.m_pDS->num_rows() > 0)
      {
        int idArtist = m_db.m_pDS->fv("idArtist").get_asInt();
        bool update = m_db.m_pDS->fv("strArtist").get_asString().compare(strMusicBrainzArtistID) == 0;
        m_db.m_pDS->close();
        if (update)
        {
          strSQL = m_db.PrepareSQL("UPDATE artist SET strArtist = '%s' "
                                   "WHERE idArtist = %i",
                                   strArtist.c_str(), idArtist);
          m_db.m_pDS->exec(strSQL);
          m_db.m_pDS->close();
        }
        return idArtist;
      }
      m_db.m_pDS->close();

      strSQL = m_db.PrepareSQL("SELECT idArtist FROM artist "
                               "WHERE strArtist LIKE '%s' AND strMusicBrainzArtistID IS NULL",
                               strArtist.c_str());
      m_db.m_pDS->query(strSQL);
      if (m_db.m_pDS->num_rows() > 0)
      {
        int idArtist = m_db.m_pDS->fv("idArtist").get_asInt();
        m_db.m_pDS->close();
        strSQL =
            m_db.PrepareSQL("UPDATE artist SET strArtist = '%s', strMusicBrainzArtistID = '%s', "
                            "bScrapedMBID = %i WHERE idArtist = %i",
                            strArtist.c_str(), strMusicBrainzArtistID.c_str(), bScrapedMBID, idArtist);
        m_db.m_pDS->exec(strSQL);
        return idArtist;
      }
    }
    else
    {
      strSQL =
          m_db.PrepareSQL("SELECT idArtist FROM artist WHERE strArtist LIKE '%s'", strArtist.c_str());

      m_db.m_pDS->query(strSQL);
      if (m_db.m_pDS->num_rows() > 0)
      {
        int idArtist = m_db.m_pDS->fv("idArtist").get_asInt();
        m_db.m_pDS->close();
        return idArtist;
      }
      m_db.m_pDS->close();
    }

    // 3) No artist exists at all - add it
    if (strMusicBrainzArtistID.empty())
      strSQL = m_db.PrepareSQL("INSERT INTO artist "
                               "(idArtist, strArtist, strMusicBrainzArtistID) "
                               "VALUES( NULL, '%s', NULL)",
                               strArtist.c_str());
    else
      strSQL = m_db.PrepareSQL("INSERT INTO artist (idArtist, strArtist, strMusicBrainzArtistID, "
                               "bScrapedMBID) "
                               "VALUES( NULL, '%s', '%s', %i )",
                               strArtist.c_str(), strMusicBrainzArtistID.c_str(), bScrapedMBID);

    m_db.m_pDS->exec(strSQL);
    return static_cast<int>(m_db.m_pDS->lastinsertid());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to addartist ({})", strSQL);
  }

  return -1;
}

bool CMusicCRUDRepository::GetArtist(int idArtist, CArtist& artist, bool fetchAll /* = false */)
{
  try
  {
    auto start = std::chrono::steady_clock::now();
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;
    if (nullptr == m_db.m_pDS2)
      return false;

    if (idArtist == -1)
      return false;

    std::string strSQL;
    if (fetchAll)
      strSQL = m_db.PrepareSQL("SELECT * FROM artistview "
                               "LEFT JOIN discography ON artistview.idArtist = discography.idArtist "
                               "WHERE artistview.idArtist = %i",
                               idArtist);
    else
      strSQL = m_db.PrepareSQL("SELECT * FROM artistview WHERE artistview.idArtist = %i", idArtist);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }
    std::string debugSQL = strSQL + " - ";
    int discographyOffset = artist_enumCount;

    artist.discography.clear();
    artist = CMusicDatasetHelper::GetArtistFromDataset(m_db.m_pDS->get_sql_record(), 0, true,
                                                       m_db.m_translateBlankArtist);
    if (fetchAll)
    {
      while (!m_db.m_pDS->eof())
      {
        const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();
        CDiscoAlbum discoAlbum;
        discoAlbum.strAlbum = record->at(discographyOffset + 1).get_asString();
        discoAlbum.strYear = record->at(discographyOffset + 2).get_asString();
        discoAlbum.strReleaseGroupMBID = record->at(discographyOffset + 3).get_asString();
        artist.discography.emplace_back(discoAlbum);
        m_db.m_pDS->next();
      }
    }
    m_db.m_pDS->close();

    artist.videolinks.clear();
    if (fetchAll)
    {
      strSQL = m_db.PrepareSQL("SELECT idSong, strTitle, strMusicBrainzTrackID, strVideoURL, url "
                               "FROM song JOIN album_artist ON song.idAlbum = album_artist.idAlbum "
                               "LEFT JOIN art ON art.media_id = song.idSong AND art.type = 'videothumb' "
                               "WHERE album_artist.idArtist = %i AND "
                               "song.strVideoURL is not NULL GROUP by song.strVideoURL ORDER BY idSong",
                               idArtist);
      debugSQL += strSQL;
      m_db.m_pDS->query(strSQL);
      while (!m_db.m_pDS->eof())
      {
        const dbiplus::sql_record* const record = m_db.m_pDS->get_sql_record();
        ArtistVideoLinks videoLink;
        videoLink.title = record->at(1).get_asString();
        videoLink.mbTrackID = record->at(2).get_asString();
        videoLink.videoURL = record->at(3).get_asString();
        videoLink.thumbURL = record->at(4).get_asString();

        artist.videolinks.emplace_back(std::move(videoLink));
        m_db.m_pDS->next();
      }
      m_db.m_pDS->close();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    CLog::LogF(LOGDEBUG, "{} - took {} ms", debugSQL, duration.count());
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }

  return false;
}

bool CMusicCRUDRepository::GetArtistExists(int idArtist)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL =
        m_db.PrepareSQL("SELECT 1 FROM artist WHERE artist.idArtist = %i LIMIT 1", idArtist);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }

  return false;
}

int CMusicCRUDRepository::GetLastArtist() const
{
  std::string strSQL = "SELECT MAX(idArtist) FROM artist";
  std::string lastArtist = m_db.GetSingleValue(strSQL);
  if (lastArtist.empty())
    return -1;

  return static_cast<int>(std::strtol(lastArtist.c_str(), nullptr, 10));
}

int CMusicCRUDRepository::GetArtistFromMBID(const std::string& strMusicBrainzArtistID,
                                            std::string& artistname)
{
  if (strMusicBrainzArtistID.empty())
    return -1;

  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB || nullptr == m_db.m_pDS2)
      return -1;
    strSQL =
        m_db.PrepareSQL("SELECT idArtist, strArtist FROM artist WHERE strMusicBrainzArtistID = '%s'",
                        strMusicBrainzArtistID.c_str());
    if (!m_db.m_pDS2->query(strSQL))
      return -1;
    int idArtist = -1;
    if (m_db.m_pDS2->num_rows() > 0)
    {
      idArtist = m_db.m_pDS2->fv("idArtist").get_asInt();
      artistname = m_db.m_pDS2->fv("strArtist").get_asString();
    }
    m_db.m_pDS2->close();
    return idArtist;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed to execute {}", strSQL);
  }
  return -1;
}

int CMusicCRUDRepository::UpdateArtist(int idArtist,
                                       const std::string& strArtist,
                                       const std::string& strSortName,
                                       const std::string& strMusicBrainzArtistID,
                                       const bool bScrapedMBID,
                                       const std::string& strType,
                                       const std::string& strGender,
                                       const std::string& strDisambiguation,
                                       const std::string& strBorn,
                                       const std::string& strFormed,
                                       const std::string& strGenres,
                                       const std::string& strMoods,
                                       const std::string& strStyles,
                                       const std::string& strInstruments,
                                       const std::string& strBiography,
                                       const std::string& strDied,
                                       const std::string& strDisbanded,
                                       const std::string& strYearsActive,
                                       const std::string& strImage)
{
  if (idArtist < 0)
    return -1;

  bool useMBIDNull = strMusicBrainzArtistID.empty();
  bool isScrapedMBID = bScrapedMBID;
  std::string artistname;
  int idArtistMbid = GetArtistFromMBID(strMusicBrainzArtistID, artistname);
  if (idArtistMbid > 0 && idArtistMbid != idArtist)
  {
    CLog::LogF(LOGDEBUG, "Updating {} (Id: {}) mbid {} already assigned to {} (Id: {})", strArtist,
               idArtist, strMusicBrainzArtistID, artistname, idArtistMbid);
    useMBIDNull = true;
    isScrapedMBID = false;
  }

  std::string strImageURLs = strImage;
  if (StringUtils::EqualsNoCase(
          CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type,
          "mysql"))
    m_db.TrimImageURLs(strImageURLs, 65535);

  std::string strSQL;
  strSQL = m_db.PrepareSQL("UPDATE artist SET "
                           " strArtist = '%s', "
                           " strType = '%s', strGender = '%s', strDisambiguation = '%s', "
                           " strBorn = '%s', strFormed = '%s', strGenres = '%s', "
                           " strMoods = '%s', strStyles = '%s', strInstruments = '%s', "
                           " strBiography = '%s', strDied = '%s', strDisbanded = '%s', "
                           " strYearsActive = '%s', strImage = '%s', "
                           " lastScraped = '%s', bScrapedMBID = %i",
                           strArtist.c_str(),
                           strType.c_str(), strGender.c_str(), strDisambiguation.c_str(), //
                           strBorn.c_str(), strFormed.c_str(), strGenres.c_str(), //
                           strMoods.c_str(), strStyles.c_str(), strInstruments.c_str(), //
                           strBiography.c_str(), strDied.c_str(), strDisbanded.c_str(), //
                           strYearsActive.c_str(), strImageURLs.c_str(), //
                           CDateTime::GetUTCDateTime().GetAsDBDateTime().c_str(), isScrapedMBID);
  if (useMBIDNull)
    strSQL += m_db.PrepareSQL(", strMusicBrainzArtistID = NULL");
  else
    strSQL += m_db.PrepareSQL(", strMusicBrainzArtistID = '%s'", strMusicBrainzArtistID.c_str());
  if (strSortName.empty())
    strSQL += m_db.PrepareSQL(", strSortName = NULL");
  else
    strSQL += m_db.PrepareSQL(", strSortName = '%s'", strSortName.c_str());

  strSQL += m_db.PrepareSQL(" WHERE idArtist = %i", idArtist);

  bool status = m_db.ExecuteQuery(strSQL);
  if (status)
    AnnounceUpdate(MediaTypeArtist, idArtist);
  return idArtist;
}

bool CMusicCRUDRepository::UpdateArtistScrapedMBID(int idArtist,
                                                   const std::string& strMusicBrainzArtistID)
{
  if (strMusicBrainzArtistID.empty() || idArtist < 0)
    return false;

  std::string artistname;
  int idArtistMbid = GetArtistFromMBID(strMusicBrainzArtistID, artistname);
  if (idArtistMbid > 0 && idArtistMbid != idArtist)
  {
    CLog::LogF(LOGDEBUG, "Artist mbid {} already assigned to {} (Id: {})", strMusicBrainzArtistID,
               artistname, idArtistMbid);
    return false;
  }

  std::string strSQL;
  strSQL = m_db.PrepareSQL("UPDATE artist SET strMusicBrainzArtistID = '%s', bScrapedMBID = 1 "
                           "WHERE idArtist = %i AND strMusicBrainzArtistID IS NULL",
                           strMusicBrainzArtistID.c_str(), idArtist);

  bool status = m_db.ExecuteQuery(strSQL);
  if (status)
  {
    AnnounceUpdate(MediaTypeArtist, idArtist);
    return true;
  }
  return false;
}

bool CMusicCRUDRepository::HasArtistBeenScraped(int idArtist) const
{
  std::string strSQL = m_db.PrepareSQL(
      "SELECT idArtist FROM artist WHERE idArtist = %i AND lastScraped IS NULL", idArtist);
  return m_db.GetSingleValue(strSQL).empty();
}

bool CMusicCRUDRepository::ClearArtistLastScrapedTime(int idArtist)
{
  std::string strSQL =
      m_db.PrepareSQL("UPDATE artist SET lastScraped = NULL WHERE idArtist = %i", idArtist);
  return m_db.ExecuteQuery(strSQL);
}

bool CMusicCRUDRepository::AddArtistVideoLinks(const CArtist& artist)
{
  auto start = std::chrono::steady_clock::now();
  std::string dbSong;

  try
  {
    if (nullptr == m_db.m_pDB || nullptr == m_db.m_pDS || nullptr == m_db.m_pDS2)
      return false;

    for (const auto& videoURL : artist.videolinks)
    {
      dbSong = videoURL.title;
      std::string strSQL = m_db.PrepareSQL(
          "SELECT idSong, strTitle FROM song WHERE strMusicBrainzTrackID = '%s' OR (EXISTS "
          "(SELECT 1 FROM album_artist WHERE album_artist.idAlbum = song.idAlbum AND "
          "album_artist.idArtist = '%i' AND song.strTitle LIKE '%%%s%%'))",
          videoURL.mbTrackID.c_str(), artist.idArtist, videoURL.title.c_str());

      if (!m_db.m_pDS->query(strSQL))
        return false;
      if (m_db.m_pDS->num_rows() == 0)
        continue;

      while (!m_db.m_pDS->eof())
      {
        const int songId = m_db.m_pDS->fv(0).get_asInt();
        std::string strSQL2 = m_db.PrepareSQL("UPDATE song SET strVideoURL='%s' WHERE idSong = %i",
                                              videoURL.videoURL.c_str(), songId);
        CLog::Log(LOGDEBUG, "Adding videolink for song {} with id {}", dbSong, songId);
        m_db.m_pDS2->exec(strSQL2);

        if (!videoURL.thumbURL.empty())
        {
          strSQL2 = m_db.PrepareSQL("SELECT art_id FROM art "
                                    "WHERE media_id=%i AND media_type='%s' AND type='videothumb'",
                                    songId, MediaTypeSong);
          m_db.m_pDS2->query(strSQL2);
          if (!m_db.m_pDS2->eof())
          {
            const int artId = m_db.m_pDS2->fv(0).get_asInt();
            m_db.m_pDS2->close();
            strSQL2 = m_db.PrepareSQL("UPDATE art SET url='%s' where art_id=%d",
                                      videoURL.thumbURL.c_str(), artId);
            m_db.m_pDS2->exec(strSQL2);
          }
          else
          {
            m_db.m_pDS2->close();
            strSQL2 = m_db.PrepareSQL("INSERT INTO art(media_id, media_type, type, url) "
                                      "VALUES (%d, '%s', '%s', '%s')",
                                      songId, MediaTypeSong, "videothumb", videoURL.thumbURL.c_str());
            m_db.m_pDS2->exec(strSQL2);
          }
          m_db.m_pDS2->close();
        }
        m_db.m_pDS->next();
      }
      m_db.m_pDS->close();
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    CLog::LogF(LOGDEBUG, "Time to store videolinks {}ms ", duration.count());
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "MusicDatabase: Unable to add videolink for song ({})", dbSong);
    return false;
  }
}

bool CMusicCRUDRepository::DeleteArtistVideoLinks(const int idArtist)
{
  std::string strSQL = m_db.PrepareSQL("UPDATE song SET strVideoURL = NULL WHERE idAlbum IN "
                                       "(SELECT idAlbum FROM album_artist WHERE idArtist = %i)",
                                       idArtist);
  if (!m_db.ExecuteQuery(strSQL))
    return false;
  strSQL = m_db.PrepareSQL(
      "DELETE FROM art WHERE art.type = 'videothumb' AND art.media_id IN (SELECT idSong FROM song "
      "JOIN album_artist ON song.idAlbum = album_artist.idAlbum WHERE album_artist.idArtist = %i)",
      idArtist);
  return m_db.ExecuteQuery(strSQL);
}

int CMusicCRUDRepository::AddArtistDiscography(int idArtist, const CDiscoAlbum& discoAlbum)
{
  std::string strSQL = m_db.PrepareSQL("INSERT INTO discography "
                                       "(idArtist, strAlbum, strYear, strReleaseGroupMBID) "
                                       "VALUES(%i, '%s', '%s', '%s')",
                                       idArtist, discoAlbum.strAlbum.c_str(), discoAlbum.strYear.c_str(),
                                       discoAlbum.strReleaseGroupMBID.c_str());
  return m_db.ExecuteQuery(strSQL);
}

bool CMusicCRUDRepository::DeleteArtistDiscography(int idArtist)
{
  std::string strSQL = m_db.PrepareSQL("DELETE FROM discography WHERE idArtist = %i", idArtist);
  return m_db.ExecuteQuery(strSQL);
}

bool CMusicCRUDRepository::GetArtistDiscography(int idArtist, CFileItemList& items)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    m_db.m_pDS->exec("CREATE TABLE tempDisco "
                     "(strAlbum TEXT, strYear VARCHAR(4), mbid TEXT, idAlbum INTEGER)");
    m_db.m_pDS->exec("CREATE TABLE tempAlbum "
                     "(strAlbum TEXT, strYear VARCHAR(4), mbid TEXT, idAlbum INTEGER)");

    std::string strSQL;
    strSQL = m_db.PrepareSQL("INSERT INTO tempDisco(strAlbum, strYear, mbid, idAlbum) "
                             "SELECT strAlbum, SUBSTR(discography.strYear, 1, 4) AS strYear, "
                             "strReleaseGroupMBID, NULL "
                             "FROM discography WHERE idArtist = %i",
                             idArtist);
    m_db.m_pDS->exec(strSQL);

    strSQL = m_db.PrepareSQL("INSERT INTO tempAlbum(strAlbum, strYear, mbid, idAlbum) "
                             "SELECT strAlbum, SUBSTR(strOrigReleaseDate, 1, 4) AS strYear, "
                             "strReleaseGroupMBID, album.idAlbum "
                             "FROM album JOIN album_artist ON album_artist.idAlbum = album.idAlbum "
                             "WHERE idArtist = %i",
                             idArtist);
    m_db.m_pDS->exec(strSQL);

    strSQL = "UPDATE tempDisco SET idAlbum = (SELECT tempAlbum.idAlbum FROM tempAlbum "
             "WHERE tempAlbum.mbid = tempDisco.mbid AND tempAlbum.mbid IS NOT NULL)";
    m_db.m_pDS->exec(strSQL);
    strSQL = "DELETE FROM tempAlbum "
             "WHERE EXISTS(SELECT 1 FROM tempDisco WHERE tempDisco.idAlbum = tempAlbum.idAlbum)";
    m_db.m_pDS->exec(strSQL);

    strSQL = "UPDATE tempDisco SET idAlbum = (SELECT idAlbum FROM tempAlbum "
             "WHERE tempAlbum.strAlbum = tempDisco.strAlbum "
             "AND tempAlbum.strYear = tempDisco.strYear) "
             "WHERE tempDisco.idAlbum is NULL";
    m_db.m_pDS->exec(strSQL);
    strSQL = "DELETE FROM tempAlbum "
             "WHERE EXISTS(SELECT 1 FROM tempDisco WHERE tempDisco.idAlbum = tempAlbum.idAlbum)";
    m_db.m_pDS->exec(strSQL);

    strSQL = "UPDATE tempDisco SET idAlbum = (SELECT idAlbum FROM tempAlbum "
             "WHERE tempAlbum.strAlbum = tempDisco.strAlbum) "
             "WHERE tempDisco.idAlbum is NULL";
    m_db.m_pDS->exec(strSQL);
    strSQL = "UPDATE tempDisco SET strYear = (SELECT strYear FROM tempAlbum "
             "WHERE tempAlbum.idAlbum = tempDisco.idAlbum) "
             "WHERE EXISTS(SELECT 1 FROM tempAlbum WHERE tempAlbum.idAlbum = tempDisco.idAlbum)";
    m_db.m_pDS->exec(strSQL);
    strSQL = "DELETE FROM tempAlbum "
             "WHERE EXISTS(SELECT 1 FROM tempDisco WHERE tempDisco.idAlbum = tempAlbum.idAlbum)";
    m_db.m_pDS->exec(strSQL);

    strSQL = "SELECT strAlbum, strYear, idAlbum FROM tempDisco "
             "UNION "
             "SELECT strAlbum, strYear, idAlbum FROM tempAlbum "
             "ORDER BY strYear, strAlbum, idAlbum";

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    while (!m_db.m_pDS->eof())
    {
      int idAlbum = m_db.m_pDS->fv("idAlbum").get_asInt();
      if (idAlbum == 0)
        idAlbum = -1;
      std::string strAlbum = m_db.m_pDS->fv("strAlbum").get_asString();
      if (!strAlbum.empty())
      {
        auto pItem{std::make_shared<CFileItem>(strAlbum)};
        pItem->SetLabel2(m_db.m_pDS->fv("strYear").get_asString());
        pItem->GetMusicInfoTag()->SetDatabaseId(idAlbum, MediaTypeAlbum);
        items.Add(std::move(pItem));
      }
      m_db.m_pDS->next();
    }

    m_db.m_pDS->close();
    m_db.m_pDS->exec("DROP TABLE tempDisco");
    m_db.m_pDS->exec("DROP TABLE tempAlbum");

    return true;
  }
  catch (...)
  {
    m_db.m_pDS->exec("DROP TABLE tempDisco");
    m_db.m_pDS->exec("DROP TABLE tempAlbum");
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

std::string CMusicCRUDRepository::GetArtistById(int id) const
{
  return m_db.GetSingleValue("artist", "strArtist", m_db.PrepareSQL("idArtist=%i", id));
}

int CMusicCRUDRepository::GetArtistByName(const std::string& strArtist)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL = m_db.PrepareSQL("SELECT idArtist FROM artist WHERE artist.strArtist LIKE '%s'",
                                         strArtist.c_str());

    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return -1;
    }
    int lResult = m_db.m_pDS->fv("artist.idArtist").get_asInt();
    m_db.m_pDS->close();
    return lResult;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

int CMusicCRUDRepository::GetArtistByMatch(const CArtist& artist)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB || nullptr == m_db.m_pDS)
      return false;
    if (!artist.strMusicBrainzArtistID.empty())
      strSQL = m_db.PrepareSQL("SELECT idArtist FROM artist "
                               "WHERE strMusicBrainzArtistID = '%s'",
                               artist.strMusicBrainzArtistID.c_str());
    else
      strSQL = m_db.PrepareSQL("SELECT idArtist FROM artist "
                               "WHERE strArtist LIKE '%s' AND strMusicBrainzArtistID IS NULL",
                               artist.strArtist.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return GetArtistByName(artist.strArtist);
    }
    int lResult = m_db.m_pDS->fv("idArtist").get_asInt();
    m_db.m_pDS->close();
    return lResult;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed to execute {}", strSQL);
  }
  return -1;
}

bool CMusicCRUDRepository::GetArtistFromSong(int idSong, CArtist& artist)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL = m_db.PrepareSQL(
        "SELECT artistview.* FROM song_artist "
        "JOIN artistview ON song_artist.idArtist = artistview.idArtist "
        "WHERE song_artist.idSong= %i AND song_artist.idRole = 1 AND song_artist.iOrder = 0",
        idSong);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return false;
    }

    artist = CMusicDatasetHelper::GetArtistFromDataset(m_db.m_pDS.get(), 0, true,
                                                       m_db.m_translateBlankArtist);

    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicCRUDRepository::IsSongArtist(int idSong, int idArtist) const
{
  std::string strSQL = m_db.PrepareSQL("SELECT 1 FROM song_artist "
                                       "WHERE song_artist.idSong= %i AND "
                                       "song_artist.idArtist = %i AND song_artist.idRole = 1",
                                       idSong, idArtist);
  return m_db.GetSingleValue(strSQL).empty();
}

bool CMusicCRUDRepository::IsSongAlbumArtist(int idSong, int idArtist) const
{
  std::string strSQL =
      m_db.PrepareSQL("SELECT 1 FROM song JOIN album_artist ON song.idAlbum = album_artist.idAlbum "
                      "WHERE song.idSong = %i AND album_artist.idArtist = %i",
                      idSong, idArtist);
  return m_db.GetSingleValue(strSQL).empty();
}

std::string CMusicCRUDRepository::GetRoleById(int id) const
{
  return m_db.GetSingleValue("role", "strRole", m_db.PrepareSQL("idRole=%i", id));
}

bool CMusicCRUDRepository::UpdateArtistSortNames(int idArtist /*=-1*/)
{
  std::string strSQL;

  bool bisMySQL = StringUtils::EqualsNoCase(
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type, "mysql");

  m_db.BeginMultipleExecute();
  if (bisMySQL)
    strSQL = "(SELECT GROUP_CONCAT("
             "CASE WHEN artist.strSortName IS NULL THEN artist.strArtist "
             "ELSE artist.strSortName END "
             "ORDER BY album_artist.idAlbum, album_artist.iOrder "
             "SEPARATOR '; ') as val "
             "FROM album_artist JOIN artist on artist.idArtist = album_artist.idArtist "
             "WHERE album_artist.idAlbum = album.idAlbum GROUP BY idAlbum) ";
  else
    strSQL = "(SELECT GROUP_CONCAT(val, '; ') "
             "FROM(SELECT album_artist.idAlbum, "
             "CASE WHEN artist.strSortName IS NULL THEN artist.strArtist "
             "ELSE artist.strSortName END as val "
             "FROM album_artist JOIN artist on artist.idArtist = album_artist.idArtist "
             "WHERE album_artist.idAlbum = album.idAlbum "
             "ORDER BY album_artist.idAlbum, album_artist.iOrder) GROUP BY idAlbum) ";

  strSQL = "UPDATE album SET strArtistSort = " + strSQL +
           "WHERE (album.strArtistSort = '' OR album.strArtistSort IS NULL) "
           "AND strArtistDisp <> " +
           strSQL;
  if (idArtist > 0)
    strSQL +=
        m_db.PrepareSQL(" AND EXISTS (SELECT 1 FROM album_artist WHERE album_artist.idArtist = %ld "
                        "AND album_artist.idAlbum = album.idAlbum)",
                        idArtist);
  m_db.ExecuteQuery(strSQL);
  CLog::LogF(LOGDEBUG, "query: {}", strSQL);

  if (bisMySQL)
    strSQL = "(SELECT GROUP_CONCAT("
             "CASE WHEN artist.strSortName IS NULL THEN artist.strArtist "
             "ELSE artist.strSortName END "
             "ORDER BY song_artist.idSong, song_artist.iOrder "
             "SEPARATOR '; ') as val "
             "FROM song_artist JOIN artist on artist.idArtist = song_artist.idArtist "
             "WHERE song_artist.idSong = song.idSong AND song_artist.idRole = 1 GROUP BY idSong) ";
  else
    strSQL = "(SELECT GROUP_CONCAT(val, '; ') "
             "FROM(SELECT song_artist.idSong, "
             "CASE WHEN artist.strSortName IS NULL THEN artist.strArtist "
             "ELSE artist.strSortName END as val "
             "FROM song_artist JOIN artist on artist.idArtist = song_artist.idArtist "
             "WHERE song_artist.idSong = song.idSong AND song_artist.idRole = 1 "
             "ORDER BY song_artist.idSong, song_artist.iOrder) GROUP BY idSong) ";

  strSQL = "UPDATE song SET strArtistSort = " + strSQL +
           "WHERE (song.strArtistSort = '' OR song.strArtistSort IS NULL) "
           "AND strArtistDisp <> " +
           strSQL;
  if (idArtist > 0)
    strSQL += m_db.PrepareSQL(" AND EXISTS (SELECT 1 FROM song_artist WHERE song_artist.idArtist = %ld "
                              "AND song_artist.idSong = song.idSong AND song_artist.idRole = 1)",
                              idArtist);
  m_db.ExecuteQuery(strSQL);
  CLog::LogF(LOGDEBUG, "query: {}", strSQL);

  if (m_db.CommitMultipleExecute())
    return true;
  else
    CLog::LogF(LOGERROR, "failed");
  return false;
}

// ---------------------------------------------------------------------------
// Link tables
// ---------------------------------------------------------------------------

int CMusicCRUDRepository::AddRole(std::string_view strRole)
{
  int idRole = -1;
  std::string strSQL;

  try
  {
    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;
    strSQL = m_db.PrepareSQL("SELECT idRole FROM role WHERE strRole LIKE '%s'", strRole.data());
    m_db.m_pDS->query(strSQL);
    if (m_db.m_pDS->num_rows() > 0)
      idRole = m_db.m_pDS->fv("idRole").get_asInt();
    m_db.m_pDS->close();

    if (idRole < 0)
    {
      strSQL = m_db.PrepareSQL("INSERT INTO role (strRole) VALUES ('%s')", strRole.data());
      m_db.m_pDS->exec(strSQL);
      idRole = static_cast<int>(m_db.m_pDS->lastinsertid());
      m_db.m_pDS->close();
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to AddRole ({})", strSQL);
  }
  return idRole;
}

bool CMusicCRUDRepository::AddSongArtist(
    int idArtist, int idSong, std::string_view strRole, std::string_view strArtist, int iOrder)
{
  int idRole = AddRole(strRole);
  return AddSongArtist(idArtist, idSong, idRole, strArtist, iOrder);
}

bool CMusicCRUDRepository::AddSongArtist(
    int idArtist, int idSong, int idRole, std::string_view strArtist, int iOrder)
{
  std::string strSQL;
  strSQL = m_db.PrepareSQL("REPLACE INTO song_artist (idArtist, idSong, idRole, strArtist, iOrder) "
                           "VALUES(%i, %i, %i,'%s', %i)",
                           idArtist, idSong, idRole, strArtist.data(), iOrder);
  return m_db.ExecuteQuery(strSQL);
}

int CMusicCRUDRepository::AddSongContributor(int idSong,
                                             const std::string& strRole,
                                             const std::string& strArtist,
                                             const std::string& strSort)
{
  if (strArtist.empty())
    return -1;

  std::string strSQL;
  try
  {
    if (nullptr == m_db.m_pDB)
      return -1;
    if (nullptr == m_db.m_pDS)
      return -1;

    int idArtist = -1;
    strSQL =
        m_db.PrepareSQL("SELECT idArtist FROM song_artist WHERE idSong = %i AND strArtist LIKE '%s' ",
                        idSong, strArtist.c_str());
    m_db.m_pDS->query(strSQL);
    if (m_db.m_pDS->num_rows() > 0)
      idArtist = m_db.m_pDS->fv("idArtist").get_asInt();
    m_db.m_pDS->close();

    if (idArtist < 0)
      idArtist = AddArtist(strArtist, "", strSort);

    AddSongArtist(idArtist, idSong, strRole, strArtist, 0);

    return idArtist;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to AddSongContributor ({})", strSQL);
  }

  return -1;
}

void CMusicCRUDRepository::AddSongContributors(int idSong,
                                               const std::vector<CMusicRole>& contributors,
                                               const std::string& strSort)
{
  std::vector<std::string> composerSort;
  size_t countComposer = 0;
  if (!strSort.empty())
  {
    composerSort = StringUtils::Split(
        strSort,
        CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator);
  }

  for (const auto& credit : contributors)
  {
    std::string strSortName;
    if (countComposer < composerSort.size() && credit.GetRoleDesc().compare("Composer") == 0)
    {
      strSortName = composerSort[countComposer];
      countComposer++;
    }
    AddSongContributor(idSong, credit.GetRoleDesc(), credit.GetArtist(), strSortName);
  }
}

int CMusicCRUDRepository::GetRoleByName(const std::string& strRole)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;

    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT idRole FROM role WHERE strRole like '%s'", strRole.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_db.m_pDS->close();
      return -1;
    }
    return m_db.m_pDS->fv("idRole").get_asInt();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

bool CMusicCRUDRepository::GetRolesByArtist(int idArtist, CFileItem* item)
{
  try
  {
    std::string strSQL =
        m_db.PrepareSQL("SELECT DISTINCT song_artist.idRole, Role.strRole "
                        "FROM song_artist JOIN role ON song_artist.idRole = role.idRole "
                        "WHERE idArtist = %i ORDER BY song_artist.idRole ASC",
                        idArtist);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    CVariant artistRoles(CVariant::VariantTypeArray);

    while (!m_db.m_pDS->eof())
    {
      CVariant roleObj;
      roleObj["role"] = m_db.m_pDS->fv("strRole").get_asString();
      roleObj["roleid"] = m_db.m_pDS->fv("idrole").get_asInt();
      artistRoles.push_back(roleObj);
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    item->SetProperty("roles", artistRoles);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}

bool CMusicCRUDRepository::DeleteSongArtistsBySong(int idSong)
{
  return m_db.ExecuteQuery(m_db.PrepareSQL("DELETE FROM song_artist WHERE idSong = %i", idSong));
}

bool CMusicCRUDRepository::AddAlbumArtist(int idArtist,
                                          int idAlbum,
                                          std::string_view strArtist,
                                          int iOrder)
{
  std::string strSQL;
  strSQL = m_db.PrepareSQL("REPLACE INTO album_artist (idArtist, idAlbum, strArtist, iOrder) "
                           "VALUES(%i,%i,'%s',%i)",
                           idArtist, idAlbum, strArtist.data(), iOrder);
  return m_db.ExecuteQuery(strSQL);
}

bool CMusicCRUDRepository::GetAlbumsByArtist(int idArtist, std::vector<int>& albums)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT idAlbum  FROM album_artist WHERE idArtist = %i", idArtist);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    while (!m_db.m_pDS->eof())
    {
      albums.push_back(m_db.m_pDS->fv("idAlbum").get_asInt());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}

bool CMusicCRUDRepository::GetArtistsByAlbum(int idAlbum, CFileItem* item)
{
  try
  {
    std::string strSQL;

    strSQL = m_db.PrepareSQL("SELECT * FROM albumartistview WHERE idAlbum = %i", idAlbum);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    std::vector<CArtistCredit> artistCredits;
    while (!m_db.m_pDS->eof())
    {
      artistCredits.emplace_back(
          CMusicDatasetHelper::GetArtistCreditFromDataset(m_db.m_pDS->get_sql_record(), 0));
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    std::vector<std::string> musicBrainzID;
    std::vector<std::string> albumartists;
    CVariant artistidObj(CVariant::VariantTypeArray);
    for (const auto& artistCredit : artistCredits)
    {
      artistidObj.push_back(artistCredit.GetArtistId());
      albumartists.emplace_back(artistCredit.GetArtist());
      if (!artistCredit.GetMusicBrainzArtistID().empty())
        musicBrainzID.emplace_back(artistCredit.GetMusicBrainzArtistID());
    }
    item->GetMusicInfoTag()->SetAlbumArtist(albumartists);
    item->GetMusicInfoTag()->SetMusicBrainzAlbumArtistID(musicBrainzID);
    item->SetProperty("albumartistid", artistidObj);

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idAlbum);
  }
  return false;
}

bool CMusicCRUDRepository::GetArtistsByAlbum(int idAlbum, std::vector<std::string>& artistIDs)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL(
        "SELECT DISTINCT idArtist FROM album_artist WHERE album_artist.idAlbum = %i \n"
        "UNION \n"
        "SELECT DISTINCT idArtist FROM song_artist JOIN song ON song.idSong = song_artist.idSong "
        "WHERE song_artist.idRole = 1 AND song.idAlbum = %i ",
        idAlbum, idAlbum);

    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }
    while (!m_db.m_pDS->eof())
    {
      artistIDs.push_back(m_db.m_pDS->fv("idArtist").get_asString());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idAlbum);
  }
  return false;
}

bool CMusicCRUDRepository::DeleteAlbumArtistsByAlbum(int idAlbum)
{
  return m_db.ExecuteQuery(m_db.PrepareSQL("DELETE FROM album_artist WHERE idAlbum = %i", idAlbum));
}

bool CMusicCRUDRepository::GetSongsByArtist(int idArtist, std::vector<int>& songs)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT idSong FROM song_artist WHERE idArtist = %i AND idRole = 1", //
                             idArtist);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    while (!m_db.m_pDS->eof())
    {
      songs.push_back(m_db.m_pDS->fv("idSong").get_asInt());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}

bool CMusicCRUDRepository::GetArtistsBySong(int idSong, std::vector<int>& artists)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT idArtist FROM song_artist WHERE idSong = %i AND idRole = 1", //
                             idSong);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return false;
    }

    while (!m_db.m_pDS->eof())
    {
      artists.push_back(m_db.m_pDS->fv("idArtist").get_asInt());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idSong);
  }
  return false;
}

bool CMusicCRUDRepository::AddSongGenres(int idSong, const std::vector<std::string>& genres)
{
  if (idSong == -1)
    return true;

  std::string strSQL;
  try
  {
    strSQL = m_db.PrepareSQL("DELETE FROM song_genre WHERE idSong = %i", idSong);
    if (!m_db.ExecuteQuery(strSQL))
      return false;
    unsigned int index = 0;
    std::vector<std::string> modgenres = genres;
    for (auto& strGenre : modgenres)
    {
      int idGenre = m_db.AddGenre(strGenre);
      strSQL = m_db.PrepareSQL("INSERT INTO song_genre (idGenre, idSong, iOrder) VALUES(%i,%i,%i)",
                               idGenre, idSong, index++);
      if (!m_db.ExecuteQuery(strSQL))
        return false;
    }
    std::string strGenres = StringUtils::Join(
        modgenres,
        CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator);
    strSQL = m_db.PrepareSQL("UPDATE song SET strGenres = '%s' WHERE idSong = %i", //
                             strGenres.c_str(), idSong);
    if (!m_db.ExecuteQuery(strSQL))
      return false;

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) {} failed", idSong, strSQL);
  }
  return false;
}

bool CMusicCRUDRepository::GetGenresBySong(int idSong, std::vector<int>& genres)
{
  try
  {
    std::string strSQL = m_db.PrepareSQL("SELECT idGenre FROM song_genre "
                                         "WHERE idSong = %i ORDER BY iOrder ASC",
                                         idSong);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    while (!m_db.m_pDS->eof())
    {
      genres.push_back(m_db.m_pDS->fv("idGenre").get_asInt());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idSong);
  }
  return false;
}

bool CMusicCRUDRepository::GetGenresByAlbum(int idAlbum, CFileItem* item)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT DISTINCT song_genre.idGenre, genre.strGenre FROM "
                             "song JOIN song_genre ON song.idSong = song_genre.idSong "
                             "JOIN genre ON song_genre.idGenre = genre.idGenre "
                             "WHERE song.idAlbum = %i "
                             "ORDER BY song_genre.idSong, song_genre.iOrder",
                             idAlbum);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    CVariant albumSongGenres(CVariant::VariantTypeArray);

    while (!m_db.m_pDS->eof())
    {
      CVariant genreObj;
      genreObj["title"] = m_db.m_pDS->fv("strGenre").get_asString();
      genreObj["genreid"] = m_db.m_pDS->fv("idGenre").get_asInt();
      albumSongGenres.push_back(genreObj);
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    item->SetProperty("songgenres", albumSongGenres);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idAlbum);
  }
  return false;
}

bool CMusicCRUDRepository::GetGenresByArtist(int idArtist, CFileItem* item)
{
  try
  {
    std::string strSQL;
    strSQL = m_db.PrepareSQL("SELECT DISTINCT song_genre.idGenre, genre.strGenre "
                             "FROM album_artist "
                             "JOIN song ON album_artist.idAlbum = song.idAlbum "
                             "JOIN song_genre ON song.idSong = song_genre.idSong "
                             "JOIN genre ON song_genre.idGenre = genre.idGenre "
                             "WHERE album_artist.idArtist = %i "
                             "ORDER BY song_genre.idGenre",
                             idArtist);
    if (!m_db.m_pDS->query(strSQL))
      return false;
    if (m_db.m_pDS->num_rows() == 0)
    {
      m_db.m_pDS->close();
      strSQL = m_db.PrepareSQL("SELECT DISTINCT song_genre.idGenre, genre.strGenre "
                               "FROM song_artist "
                               "JOIN song_genre ON song_artist.idSong = song_genre.idSong "
                               "JOIN genre ON song_genre.idGenre = genre.idGenre "
                               "WHERE song_artist.idArtist = %i "
                               "ORDER BY song_genre.idGenre",
                               idArtist);
      if (!m_db.m_pDS->query(strSQL))
        return false;
      if (m_db.m_pDS->num_rows() == 0)
      {
        m_db.m_pDS->close();
        return true;
      }
    }

    CVariant artistSongGenres(CVariant::VariantTypeArray);

    while (!m_db.m_pDS->eof())
    {
      CVariant genreObj;
      genreObj["title"] = m_db.m_pDS->fv("strGenre").get_asString();
      genreObj["genreid"] = m_db.m_pDS->fv("idGenre").get_asInt();
      artistSongGenres.push_back(genreObj);
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    item->SetProperty("songgenres", artistSongGenres);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}

bool CMusicCRUDRepository::GetIsAlbumArtist(int idArtist, CFileItem* item) const
{
  try
  {
    int countalbum =
        m_db.GetSingleValueInt("album_artist", "count(idArtist)", m_db.PrepareSQL("idArtist=%i", idArtist));
    CVariant IsAlbumArtistObj(CVariant::VariantTypeBoolean);
    IsAlbumArtistObj = (countalbum > 0);
    item->SetProperty("isalbumartist", IsAlbumArtistObj);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}
