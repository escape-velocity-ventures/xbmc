/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicMaintenanceService.h"

#include "MusicDatabase.h"
#include "MusicSchemaManager.h"

#include "Artist.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/audiodecoder.h"
#include "dbwrappers/dataset.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogSelect.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "interfaces/AnnouncementManager.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "music/MusicLibraryQueue.h"
#ifdef HAS_OPTICAL_DRIVE
#include "network/Network.h"
#include "network/cddb.h"
#include "storage/cdioSupport.h"
#endif // HAS_OPTICAL_DRIVE
#include "profiles/ProfileManager.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "storage/MediaManager.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

using namespace XFILE;
using namespace KODI::MESSAGING;
using KODI::MESSAGING::HELPERS::DialogResponse;
#ifdef HAS_OPTICAL_DRIVE
using CDDB::Xcddb;
using MEDIA_DETECT::CCdInfo;
#endif

CMusicMaintenanceService::CMusicMaintenanceService(CMusicDatabase& db) : m_db(db)
{
}

bool CMusicMaintenanceService::CleanupSongsByIds(const std::string& strSongIds)
{
  try
  {
    if (nullptr == m_db.m_pDB)
      return false;
    if (nullptr == m_db.m_pDS)
      return false;
    // ok, now find all idSong's
    std::string strSQL =
        m_db.PrepareSQL("SELECT * FROM song JOIN path ON song.idPath = path.idPath "
                        "WHERE song.idSong IN %s",
                        strSongIds.c_str());
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }
    std::vector<std::string> songsToDelete;
    while (!m_db.m_pDS->eof())
    { // get the full song path
      std::string strFileName = URIUtils::AddFileToFolder(
          m_db.m_pDS->fv("path.strPath").get_asString(),
          m_db.m_pDS->fv("song.strFileName").get_asString());

      //  Special case for streams inside an audio decoder package file.
      //  The last dir in the path is the audio file that
      //  contains the stream, so test if its there
      if (StringUtils::EndsWith(URIUtils::GetExtension(strFileName),
                                KODI_ADDON_AUDIODECODER_TRACK_EXT))
      {
        strFileName = URIUtils::GetDirectory(strFileName);
        // we are dropping back to a file, so remove the slash at end
        URIUtils::RemoveSlashAtEnd(strFileName);
      }

      if (!CFile::Exists(strFileName, false))
      { // file no longer exists, so add to deletion list
        songsToDelete.push_back(m_db.m_pDS->fv("song.idSong").get_asString());
      }
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    if (!songsToDelete.empty())
    {
      std::string strSongsToDelete = "(" + StringUtils::Join(songsToDelete, ",") + ")";
      // ok, now delete these songs + all references to them from the linked tables
      strSQL = "delete from song where idSong in " + strSongsToDelete;
      m_db.m_pDS->exec(strSQL);
      m_db.m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupSongsByIds()");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupSongs(CGUIDialogProgress* progressDialog /*= nullptr*/)
{
  try
  {
    if (!m_db.m_pDS)
      return false;

    // Count total number of songs
    const int total = m_db.GetSingleValueInt("SELECT COUNT(1) FROM song", *m_db.m_pDS);
    // No songs to clean
    if (total == 0)
      return true;

    // run through all songs and get all unique path ids
    int iLIMIT = 1000;
    for (int i = 0;; i += iLIMIT)
    {
      std::string strSQL = m_db.PrepareSQL("SELECT song.idSong FROM song "
                                           "ORDER BY song.idSong LIMIT %i OFFSET %i",
                                           iLIMIT, i);
      if (!m_db.m_pDS->query(strSQL))
        return false;
      int iRowsFound = m_db.m_pDS->num_rows();
      // keep going until no rows are left!
      if (iRowsFound == 0)
      {
        m_db.m_pDS->close();
        return true;
      }

      std::vector<std::string> songIds;
      while (!m_db.m_pDS->eof())
      {
        songIds.push_back(m_db.m_pDS->fv("song.idSong").get_asString());
        m_db.m_pDS->next();
      }
      m_db.m_pDS->close();
      std::string strSongIds = "(" + StringUtils::Join(songIds, ",") + ")";
      CLog::Log(LOGDEBUG, "Checking songs from song ID list: {}", strSongIds);
      if (progressDialog)
      {
        int percentage = i * 100 / total;
        if (percentage > progressDialog->GetPercentage())
        {
          progressDialog->SetPercentage(percentage);
          progressDialog->Progress();
        }
        if (progressDialog->IsCanceled())
        {
          m_db.m_pDS->close();
          return false;
        }
      }
      if (!CleanupSongsByIds(strSongIds))
        return false;
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupSongs()");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupAlbums()
{
  try
  {
    // This must be run AFTER songs have been cleaned up
    // delete albums with no reference to songs
    std::string strSQL = "SELECT * FROM album "
                         "WHERE album.idAlbum NOT IN (SELECT idAlbum FROM song)";
    if (!m_db.m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }

    std::vector<std::string> albumIds;
    while (!m_db.m_pDS->eof())
    {
      albumIds.push_back(m_db.m_pDS->fv("album.idAlbum").get_asString());
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    std::string strAlbumIds = "(" + StringUtils::Join(albumIds, ",") + ")";
    // ok, now we can delete them and the references in the linked tables
    strSQL = "delete from album where idAlbum in " + strAlbumIds;
    m_db.m_pDS->exec(strSQL);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupAlbums()");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupPaths()
{
  try
  {
    // needs to be done AFTER the songs and albums have been cleaned up.
    // we can happily delete any path that has no reference to a song
    // but we must keep all paths that have been scanned that may contain songs in subpaths

    // first create a temporary table of song paths
    m_db.m_pDS->exec("CREATE TEMPORARY TABLE songpaths (idPath integer, strPath varchar(512))\n");
    m_db.m_pDS->exec("INSERT INTO songpaths "
                     "SELECT idPath, strPath FROM path "
                     "WHERE idPath IN (SELECT idPath FROM song)\n");

    // grab all paths that aren't immediately connected with a song
    std::string sql = "SELECT * FROM path WHERE idPath NOT IN (SELECT idPath FROM song)";
    if (!m_db.m_pDS->query(sql))
      return false;
    int iRowsFound = m_db.m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_db.m_pDS->close();
      return true;
    }
    // and construct a list to delete
    std::vector<std::string> pathIds;
    while (!m_db.m_pDS->eof())
    {
      // anything that isn't a parent path of a song path is to be deleted
      std::string path = m_db.m_pDS->fv("strPath").get_asString();
      sql = m_db.PrepareSQL(
          "SELECT COUNT(idPath) FROM songpaths WHERE SUBSTR(strPath,1,%i)='%s'",
          StringUtils::utf8_strlen(path), path.c_str());
      if (m_db.m_pDS2->query(sql) && m_db.m_pDS2->num_rows() == 1 &&
          m_db.m_pDS2->fv(0).get_asInt() == 0)
        pathIds.push_back(m_db.m_pDS->fv("idPath").get_asString()); // nothing found, so delete
      m_db.m_pDS2->close();
      m_db.m_pDS->next();
    }
    m_db.m_pDS->close();

    if (!pathIds.empty())
    {
      // do the deletion, and drop our temp table
      std::string deleteSQL =
          "DELETE FROM path WHERE idPath IN (" + StringUtils::Join(pathIds, ",") + ")";
      m_db.m_pDS->exec(deleteSQL);
    }
    m_db.m_pDS->exec("drop table songpaths");
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupPaths() or was aborted");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupArtists()
{
  try
  {
    // (nested queries by Bobbin007)
    // must be executed AFTER the song, album and their artist link tables are cleaned.
    // Don't delete [Missing] the missing artist tag artist

    // Create temp table to avoid 1442 trigger hell on mysql
    m_db.m_pDS->exec("CREATE TEMPORARY TABLE tmp_delartists (idArtist integer)");
    m_db.m_pDS->exec("INSERT INTO tmp_delartists select idArtist from song_artist");
    m_db.m_pDS->exec("INSERT INTO tmp_delartists select idArtist from album_artist");
    m_db.m_pDS->exec(m_db.PrepareSQL("INSERT INTO tmp_delartists VALUES(%i)", BLANKARTIST_ID));
    // tmp_delartists contains duplicate ids, and on a large library with small changes can be very large.
    // To avoid MySQL hanging or timeout create a table of unique ids with primary key
    m_db.m_pDS->exec("CREATE TEMPORARY TABLE tmp_keep (idArtist INTEGER PRIMARY KEY)");
    m_db.m_pDS->exec("INSERT INTO tmp_keep SELECT DISTINCT idArtist from tmp_delartists");
    m_db.m_pDS->exec("DELETE FROM artist WHERE idArtist NOT IN (SELECT idArtist FROM tmp_keep)");
    // Tidy up temp tables
    m_db.m_pDS->exec("DROP TABLE tmp_delartists");
    m_db.m_pDS->exec("DROP TABLE tmp_keep");

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupArtists() or was aborted");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupGenres()
{
  try
  {
    // Cleanup orphaned song genres (ie those that don't belong to a song entry)
    // (nested queries by Bobbin007)
    // Must be executed AFTER the song, and song_genre have been cleaned.
    std::string strSQL =
        "DELETE FROM genre WHERE idGenre NOT IN (SELECT idGenre FROM song_genre)";
    m_db.m_pDS->exec(strSQL);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupGenres() or was aborted");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupInfoSettings()
{
  try
  {
    // Cleanup orphaned info settings (ie those that don't belong to an album or artist entry)
    // Must be executed AFTER the album and artist tables have been cleaned.
    std::string strSQL = "DELETE FROM infosetting "
                         "WHERE idSetting NOT IN (SELECT idInfoSetting FROM artist) "
                         "AND idSetting NOT IN (SELECT idInfoSetting FROM album)";
    m_db.m_pDS->exec(strSQL);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR,
              "Exception in CMusicMaintenanceService::CleanupInfoSettings() or was aborted");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupRoles()
{
  try
  {
    // Cleanup orphaned roles (ie those that don't belong to a song entry)
    // Must be executed AFTER the song, and song_artist tables have been cleaned.
    // Do not remove default role (ROLE_ARTIST)
    std::string strSQL =
        "DELETE FROM role "
        "WHERE idRole > 1 AND idRole NOT IN (SELECT idRole FROM song_artist)";
    m_db.m_pDS->exec(strSQL);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::CleanupRoles() or was aborted");
  }
  return false;
}

bool CMusicMaintenanceService::DeleteRemovedLinks()
{
  try
  {
    std::string strSQL = "DELETE FROM removed_link";
    m_db.m_pDS->exec(strSQL);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Exception in CMusicMaintenanceService::DeleteRemovedLinks");
  }
  return false;
}

bool CMusicMaintenanceService::CleanupOrphanedItems()
{
  // paths aren't cleaned up here - they're cleaned up in RemoveSongsFromPath()
  // remove_links not cleared here - done in CheckArtistLinksChanged()
  if (nullptr == m_db.m_pDB)
    return false;
  if (nullptr == m_db.m_pDS)
    return false;
  m_db.SetLibraryLastUpdated();
  if (!CleanupAlbums())
    return false;
  if (!CleanupArtists())
    return false;
  if (!CleanupGenres())
    return false;
  if (!CleanupRoles())
    return false;
  if (!CleanupInfoSettings())
    return false;
  return true;
}

int CMusicMaintenanceService::Cleanup(CGUIDialogProgress* progressDialog /*= nullptr*/)
{
  if (nullptr == m_db.m_pDB)
    return ERROR_DATABASE;
  if (nullptr == m_db.m_pDS)
    return ERROR_DATABASE;

  int ret;
  std::chrono::seconds duration;
  auto time = std::chrono::steady_clock::now();
  CLog::Log(LOGINFO, "Starting musicdatabase cleanup ...");
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnCleanStarted");

  m_db.SetLibraryLastCleaned();

  // Drop triggers  song_artist and album_artist to avoid creation of entries in removed_link
  // Check that triggers actually exist first as interrupting the clean causes them to not be
  // re-created

  m_db.m_pDS->exec("DROP TRIGGER IF EXISTS tgrDeleteSongArtist");
  m_db.m_pDS->exec("DROP TRIGGER IF EXISTS tgrDeleteAlbumArtist");

  // first cleanup any songs with invalid paths
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{318});
    progressDialog->SetLine(2, CVariant{330});
    progressDialog->SetPercentage(0);
    progressDialog->Progress();
  }
  if (!CleanupSongs(progressDialog))
  {
    ret = ERROR_REORG_SONGS;
    goto error;
  }
  // then the albums that are not linked to a song or to album, or whose path is removed
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{326});
    progressDialog->SetPercentage(20);
    progressDialog->Progress();
    if (progressDialog->IsCanceled())
    {
      ret = ERROR_CANCEL;
      goto error;
    }
  }
  if (!CleanupAlbums())
  {
    ret = ERROR_REORG_ALBUM;
    goto error;
  }
  // now the paths
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{324});
    progressDialog->SetPercentage(40);
    progressDialog->Progress();
    if (progressDialog->IsCanceled())
    {
      ret = ERROR_CANCEL;
      goto error;
    }
  }
  if (!CleanupPaths())
  {
    ret = ERROR_REORG_PATH;
    goto error;
  }
  // and finally artists + genres
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{320});
    progressDialog->SetPercentage(60);
    progressDialog->Progress();
    if (progressDialog->IsCanceled())
    {
      ret = ERROR_CANCEL;
      goto error;
    }
  }
  if (!CleanupArtists())
  {
    ret = ERROR_REORG_ARTIST;
    goto error;
  }
  //Genres, roles and info settings progress in one step
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{322});
    progressDialog->SetPercentage(80);
    progressDialog->Progress();
    if (progressDialog->IsCanceled())
    {
      ret = ERROR_CANCEL;
      goto error;
    }
  }
  if (!CleanupGenres())
  {
    ret = ERROR_REORG_OTHER;
    goto error;
  }
  if (!CleanupRoles())
  {
    ret = ERROR_REORG_OTHER;
    goto error;
  }
  if (!CleanupInfoSettings())
  {
    ret = ERROR_REORG_OTHER;
    goto error;
  }
  if (!DeleteRemovedLinks())
  {
    ret = ERROR_REORG_OTHER;
    goto error;
  }

  // commit transaction
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{328});
    progressDialog->SetPercentage(90);
    progressDialog->Progress();
    if (progressDialog->IsCanceled())
    {
      ret = ERROR_CANCEL;
      goto error;
    }
  }
  if (!m_db.CommitTransaction())
  {
    ret = ERROR_WRITING_CHANGES;
    goto error;
  }

  // Recreate DELETE triggers on song_artist and album_artist
  KODI::DATABASE::CMusicSchemaManager::CreateRemovedLinkTriggers(m_db);

  // and compress the database
  if (progressDialog)
  {
    progressDialog->SetLine(1, CVariant{331});
    progressDialog->SetPercentage(100);
    progressDialog->Close();
  }

  duration =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - time);
  CLog::Log(LOGINFO, "Cleaning musicdatabase done. Operation took {}s", duration.count());
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnCleanFinished");

  if (!m_db.Compress(false))
  {
    return ERROR_COMPRESSING;
  }
  return ERROR_OK;

error:
  m_db.RollbackTransaction();
  // Recreate DELETE triggers on song_artist and album_artist
  KODI::DATABASE::CMusicSchemaManager::CreateRemovedLinkTriggers(m_db);
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnCleanFinished");
  return ret;
}

void CMusicMaintenanceService::Clean() const
{
  // If we are scanning for music info in the background,
  // other writing access to the database is prohibited.
  if (CMusicLibraryQueue::GetInstance().IsScanningLibrary())
  {
    HELPERS::ShowOKDialogText(CVariant{189}, CVariant{14057});
    return;
  }

  if (HELPERS::ShowYesNoDialogText(CVariant{313}, CVariant{333}) == DialogResponse::CHOICE_YES)
  {
    CMusicDatabase musicdatabase;
    if (musicdatabase.Open())
    {
      int iReturnString = musicdatabase.Cleanup();
      musicdatabase.Close();

      if (iReturnString != ERROR_OK)
      {
        HELPERS::ShowOKDialogText(CVariant{313}, CVariant{iReturnString});
      }
    }
  }
}

bool CMusicMaintenanceService::TrimImageURLs(std::string& strImage, const size_t space) const
{
  if (strImage.length() > space)
  {
    strImage.resize(space);
    // Tidy to last </thumb> tag
    size_t iPos = strImage.rfind("</thumb>");
    if (iPos == std::string::npos)
      return false;
    strImage.resize(iPos + 8);
  }
  return true;
}

bool CMusicMaintenanceService::LookupCDDBInfo(bool bRequery /*=false*/) const
{
#ifdef HAS_OPTICAL_DRIVE
  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
          CSettings::SETTING_AUDIOCDS_USECDDB))
    return false;

  // check network connectivity
  if (!CServiceBroker::GetNetwork().IsAvailable())
    return false;

  // Get information for the inserted disc
  CCdInfo* pCdInfo = CServiceBroker::GetMediaManager().GetCdInfo();
  if (!pCdInfo)
    return false;

  // If the disc has no tracks, we are finished here.
  int nTracks = pCdInfo->GetTrackCount();
  if (nTracks <= 0)
    return false;

  //  Delete old info if any
  if (bRequery)
  {
    std::string strFile = StringUtils::Format("{:x}.cddb", pCdInfo->GetCddbDiscId());
    CFile::Delete(
        URIUtils::AddFileToFolder(m_db.m_profileManager.GetCDDBFolder(), strFile));
  }

  // Prepare cddb
  Xcddb cddb;
  cddb.setCacheDir(m_db.m_profileManager.GetCDDBFolder());

  // Do we have to look for cddb information
  if (pCdInfo->HasCDDBInfo() && !cddb.isCDCached(pCdInfo))
  {
    CGUIDialogProgress* pDialogProgress =
        CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
            WINDOW_DIALOG_PROGRESS);
    CGUIDialogSelect* pDlgSelect =
        CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
            WINDOW_DIALOG_SELECT);

    if (!pDialogProgress)
      return false;
    if (!pDlgSelect)
      return false;

    // Show progress dialog if we have to connect to freedb.org
    pDialogProgress->SetHeading(CVariant{255}); //CDDB
    pDialogProgress->SetLine(0, CVariant{""}); // Querying freedb for CDDB info
    pDialogProgress->SetLine(1, CVariant{256});
    pDialogProgress->SetLine(2, CVariant{""});
    pDialogProgress->ShowProgressBar(false);
    pDialogProgress->Open();

    // get cddb information
    if (!cddb.queryCDinfo(pCdInfo))
    {
      pDialogProgress->Close();
      int lasterror = cddb.getLastError();

      // Have we found more then on match in cddb for this disc,...
      if (lasterror == E_WAIT_FOR_INPUT)
      {
        // ...yes, show the matches found in a select dialog
        // and let the user choose an entry.
        pDlgSelect->Reset();
        pDlgSelect->SetHeading(CVariant{255});
        int i = 1;
        while (true)
        {
          std::string strTitle = cddb.getInexactTitle(i);
          if (strTitle.empty())
            break;

          const std::string& strArtist = cddb.getInexactArtist(i);
          if (!strArtist.empty())
            strTitle += " - " + strArtist;

          pDlgSelect->Add(strTitle);
          i++;
        }
        pDlgSelect->Open();

        // Has the user selected a match...
        int iSelectedCD = pDlgSelect->GetSelectedItem();
        if (iSelectedCD >= 0)
        {
          // ...query cddb for the inexact match
          if (!cddb.queryCDinfo(pCdInfo, 1 + iSelectedCD))
            pCdInfo->SetNoCDDBInfo();
        }
        else
          pCdInfo->SetNoCDDBInfo();
      }
      else if (lasterror == E_NO_MATCH_FOUND)
      {
        pCdInfo->SetNoCDDBInfo();
      }
      else
      {
        pCdInfo->SetNoCDDBInfo();
        // ..no, an error occurred, display it to the user
        std::string strErrorText =
            StringUtils::Format("[{}] {}", cddb.getLastError(), cddb.getLastErrorText());
        HELPERS::ShowOKDialogLines(CVariant{255}, CVariant{257},
                                   CVariant{std::move(strErrorText)}, CVariant{0});
      }
    } // if ( !cddb.queryCDinfo( pCdInfo ) )
    else
      pDialogProgress->Close();
  }

  // Filling the file items with cddb info happens in CMusicInfoTagLoaderCDDA

  return pCdInfo->HasCDDBInfo();
#else
  return false;
#endif
}

void CMusicMaintenanceService::DeleteCDDBInfo() const
{
#ifdef HAS_OPTICAL_DRIVE
  CFileItemList items;
  if (!CDirectory::GetDirectory(m_db.m_profileManager.GetCDDBFolder(), items, ".cddb",
                                DIR_FLAG_NO_FILE_DIRS))
  {
    HELPERS::ShowOKDialogText(CVariant{313}, CVariant{426});
    return;
  }
  // Show a selectdialog that the user can select the album to delete
  CGUIDialogSelect* pDlg =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
          WINDOW_DIALOG_SELECT);
  if (pDlg)
  {
    pDlg->SetHeading(
        CVariant{CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(181)});
    pDlg->Reset();

    std::map<uint32_t, std::string> mapCDDBIds;
    for (const auto& i : items)
    {
      if (i->IsFolder())
        continue;

      std::string strFile = URIUtils::GetFileName(i->GetPath());
      strFile.erase(strFile.size() - 5, 5);
      const auto lDiscId = static_cast<uint32_t>(std::strtoul(strFile.c_str(), nullptr, 16));
      Xcddb cddb;
      cddb.setCacheDir(m_db.m_profileManager.GetCDDBFolder());

      if (!cddb.queryCache(lDiscId))
        continue;

      std::string strDiskTitle;
      std::string strDiskArtist;
      cddb.getDiskTitle(strDiskTitle);
      cddb.getDiskArtist(strDiskArtist);

      std::string str;
      if (strDiskArtist.empty())
        str = strDiskTitle;
      else
        str = strDiskTitle + " - " + strDiskArtist;

      pDlg->Add(str);
      mapCDDBIds.try_emplace(lDiscId, str);
    }

    pDlg->Sort();
    pDlg->Open();

    // and wait till user selects one
    int iSelectedAlbum = pDlg->GetSelectedItem();
    if (iSelectedAlbum < 0)
    {
      mapCDDBIds.erase(mapCDDBIds.begin(), mapCDDBIds.end());
      return;
    }

    std::string strSelectedAlbum = pDlg->GetSelectedFileItem()->GetLabel();
    for (const auto& [discId, discTitle] : mapCDDBIds)
    {
      if (discTitle == strSelectedAlbum)
      {
        const std::string strFile = StringUtils::Format("{:x}.cddb", discId);
        CFile::Delete(
            URIUtils::AddFileToFolder(m_db.m_profileManager.GetCDDBFolder(), strFile));
        break;
      }
    }
    mapCDDBIds.erase(mapCDDBIds.begin(), mapCDDBIds.end());
  }
#endif
}
