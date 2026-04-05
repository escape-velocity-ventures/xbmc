/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicDatabase.h"

#include "MusicCRUDRepository.h"
#include "MusicDatasetHelper.h"
#include "MusicMaintenanceService.h"
#include "MusicNavRepository.h"
#include "MusicPlaylistService.h"
#include "MusicQueryBuilder.h"
#include "MusicSchemaManager.h"
#include "MusicSearchService.h"

#include "Album.h"
#include "Artist.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "GUIInfoManager.h"
#include "LangInfo.h"
#include "ServiceBroker.h"
#include "Song.h"
#include "TextureCache.h"
#include "URL.h"
#include "Util.h"
#include "addons/Addon.h"
#include "addons/AddonManager.h"
#include "addons/AddonSystemSettings.h"
#include "addons/Scraper.h"
#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/audiodecoder.h"
#include "dbwrappers/dataset.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogSelect.h"
#include "events/EventLog.h"
#include "events/NotificationEvent.h"
#include "filesystem/Directory.h"
#include "filesystem/DirectoryCache.h"
#include "filesystem/File.h"
#include "filesystem/MusicDatabaseDirectory/DirectoryNode.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "imagefiles/ImageFileURL.h"
#include "interfaces/AnnouncementManager.h"
#include "messaging/helpers/DialogHelper.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "music/MusicDbUrl.h"
#include "music/MusicLibraryQueue.h"
#include "music/tags/MusicInfoTag.h"
#include "network/Network.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#ifdef HAS_OPTICAL_DRIVE
#include "network/cddb.h"
#endif // HAS_OPTICAL_DRIVE
#include "playlists/SmartPlayList.h"
#include "profiles/ProfileManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "storage/MediaManager.h"
#include "utils/FileUtils.h"
#include "utils/LegacyPathTranslation.h"
#include "utils/MathUtils.h"
#include "utils/Random.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <array>
#include <chrono>
#include <inttypes.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace KODI;
using namespace XFILE;
using namespace MUSICDATABASEDIRECTORY;
using namespace KODI::MESSAGING;
using namespace MUSIC_INFO;

using ADDON::AddonPtr;
using KODI::MESSAGING::HELPERS::DialogResponse;

#ifdef HAS_OPTICAL_DRIVE
using namespace CDDB;
using namespace MEDIA_DETECT;
#endif

namespace
{
constexpr unsigned int RECENTLY_PLAYED_LIMIT = 25;
constexpr size_t MIN_FULL_SEARCH_LENGTH = 3;

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
  if (CMusicLibraryQueue::GetInstance().IsScanningLibrary())
    data["transaction"] = true;
  if (added)
    data["added"] = true;
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnUpdate", data);
}
} // unnamed namespace

CMusicDatabase::CMusicDatabase() = default;

CMusicDatabase::~CMusicDatabase()
{
  EmptyCache();
}

bool CMusicDatabase::Open()
{
  return CDatabase::Open(
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic);
}

void CMusicDatabase::CreateTables()
{
  KODI::DATABASE::CMusicSchemaManager::CreateTables(*this);
}


void CMusicDatabase::CreateAnalytics()
{
  KODI::DATABASE::CMusicSchemaManager::CreateAnalytics(*this);
}


void CMusicDatabase::CreateRemovedLinkTriggers()
{
  KODI::DATABASE::CMusicSchemaManager::CreateRemovedLinkTriggers(*this);
}



void CMusicDatabase::CreateViews()
{
  KODI::DATABASE::CMusicSchemaManager::CreateViews(*this);
}


void CMusicDatabase::CreateNativeDBFunctions()
{
  KODI::DATABASE::CMusicSchemaManager::CreateNativeDBFunctions(*this);
}


void CMusicDatabase::SplitPath(const std::string& strFileNameAndPath,
                               std::string& strPath,
                               std::string& strFileName) const
{
  URIUtils::Split(strFileNameAndPath, strPath, strFileName);
  // Keep protocol options as part of the path
  if (URIUtils::IsURL(strFileNameAndPath))
  {
    CURL url(strFileNameAndPath);
    if (!url.GetProtocolOptions().empty())
      strPath += "|" + url.GetProtocolOptions();
  }
}

bool CMusicDatabase::AddAlbum(CAlbum& album, int idSource)
{
  return m_crudRepo.AddAlbum(album, idSource);
}


bool CMusicDatabase::UpdateAlbum(CAlbum& album)
{
  return m_crudRepo.UpdateAlbum(album);
}


void CMusicDatabase::NormaliseSongDates(std::string& strRelease, std::string& strOriginal) const
{
  // Validate we have ISO8601 format date strings YYYY, YYYY-MM, or YYYY-MM-DD
  int iDate;
  iDate = StringUtils::DateStringToYYYYMMDD(strRelease);
  if (iDate < 0)
    strRelease.clear();
  iDate = StringUtils::DateStringToYYYYMMDD(strOriginal);
  if (iDate < 0)
    strOriginal.clear();
  // Avoid missing release or original values unless both invalid or empty
  if (!strRelease.empty() && strOriginal.empty())
    strOriginal = strRelease;
  else if (strRelease.empty() && !strOriginal.empty())
    strRelease = strOriginal;
}

int CMusicDatabase::AddSong(const int idSong,
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
  return m_crudRepo.AddSong(idSong, dtDateNew, idAlbum, strTitle, strMusicBrainzTrackID,
                            strPathAndFileName, strComment, strMood, strThumb, artistDisp,
                            artistSort, genres, iTrack, iDuration, strReleaseDate,
                            strOrigReleaseDate, strDiscSubtitle, iTimesPlayed, iStartOffset,
                            iEndOffset, dtLastPlayed, rating, userrating, votes, iBPM, iBitRate,
                            iSampleRate, iChannels, songVideoURL, replayGain);
}


bool CMusicDatabase::GetSong(int idSong, CSong& song)
{
  return m_crudRepo.GetSong(idSong, song);
}


bool CMusicDatabase::UpdateSong(CSong& song, bool bArtists /*= true*/, bool bArtistLinks /*= true*/)
{
  return m_crudRepo.UpdateSong(song, bArtists, bArtistLinks);
}


int CMusicDatabase::UpdateSong(int idSong,
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
  return m_crudRepo.UpdateSong(idSong, strTitle, strMusicBrainzTrackID, strPathAndFileName,
                               strComment, strMood, strThumb, artistDisp, artistSort, genres,
                               iTrack, iDuration, strReleaseDate, strOrigReleaseDate,
                               strDiscSubtitle, iTimesPlayed, iStartOffset, iEndOffset,
                               dtLastPlayed, rating, userrating, votes, replayGain, iBPM,
                               iBitRate, iSampleRate, iChannels, songVideoURL);
}


int CMusicDatabase::AddAlbum(const std::string& strAlbum,
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
  return m_crudRepo.AddAlbum(strAlbum, strMusicBrainzAlbumID, strReleaseGroupMBID, strArtist,
                             strArtistSort, strGenre, strReleaseDate, strOrigReleaseDate,
                             bBoxedSet, strRecordLabel, strType, strReleaseStatus, bCompilation,
                             releaseType);
}


int CMusicDatabase::UpdateAlbum(int idAlbum,
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
  return m_crudRepo.UpdateAlbum(idAlbum, strAlbum, strMusicBrainzAlbumID, strReleaseGroupMBID,
                                strArtist, strArtistSort, strGenre, strMoods, strStyles, strThemes,
                                strReview, strImage, strLabel, strType, strReleaseStatus, fRating,
                                iUserrating, iVotes, strReleaseDate, strOrigReleaseDate, bBoxedSet,
                                bCompilation, releaseType, bScrapedMBID);
}


bool CMusicDatabase::GetAlbum(int idAlbum, CAlbum& album, bool getSongs /* = true */)
{
  return m_crudRepo.GetAlbum(idAlbum, album, getSongs);
}


bool CMusicDatabase::ClearAlbumLastScrapedTime(int idAlbum)
{
  return m_crudRepo.ClearAlbumLastScrapedTime(idAlbum);
}


bool CMusicDatabase::HasAlbumBeenScraped(int idAlbum) const
{
  return m_crudRepo.HasAlbumBeenScraped(idAlbum);
}


int CMusicDatabase::AddGenre(std::string& strGenre)
{
  std::string strSQL;
  try
  {
    StringUtils::Trim(strGenre);

    if (strGenre.empty())
      strGenre = CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(13205); // Unknown

    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    auto it = m_genreCache.find(strGenre);
    if (it != m_genreCache.end())
      return it->second;


    strSQL = PrepareSQL("SELECT idGenre, strGenre FROM genre WHERE strGenre LIKE '%s'",
                        strGenre.c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      // doesn't exists, add it
      strSQL = PrepareSQL("INSERT INTO genre (idGenre, strGenre) values( NULL, '%s' )",
                          strGenre.c_str());
      m_pDS->exec(strSQL);

      const auto idGenre = static_cast<int>(m_pDS->lastinsertid());
      m_genreCache.try_emplace(strGenre, idGenre);
      return idGenre;
    }
    else
    {
      int idGenre = m_pDS->fv("idGenre").get_asInt();
      strGenre = m_pDS->fv("strGenre").get_asString();
      m_genreCache.try_emplace(strGenre, idGenre);
      m_pDS->close();
      return idGenre;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to addgenre ({})", strSQL);
  }

  return -1;
}

bool CMusicDatabase::UpdateArtist(const CArtist& artist)
{
  return m_crudRepo.UpdateArtist(artist);
}


int CMusicDatabase::AddArtist(const std::string& strArtist,
                              const std::string& strMusicBrainzArtistID,
                              const std::string& strSortName,
                              bool bScrapedMBID /* = false*/)
{
  return m_crudRepo.AddArtist(strArtist, strMusicBrainzArtistID, strSortName, bScrapedMBID);
}

int CMusicDatabase::AddArtist(const std::string& strArtist,
                              const std::string& strMusicBrainzArtistID,
                              bool bScrapedMBID /* = false*/)
{
  return m_crudRepo.AddArtist(strArtist, strMusicBrainzArtistID, bScrapedMBID);
}

int CMusicDatabase::UpdateArtist(int idArtist,
                               const std::string& strArtist,
                               const std::string& strSortName,
                               const std::string& strMusicBrainzArtistID,
                               bool bScrapedMBID,
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
  return m_crudRepo.UpdateArtist(idArtist, strArtist, strSortName, strMusicBrainzArtistID,
                                 bScrapedMBID, strType, strGender, strDisambiguation, strBorn,
                                 strFormed, strGenres, strMoods, strStyles, strInstruments,
                                 strBiography, strDied, strDisbanded, strYearsActive, strImage);
}


bool CMusicDatabase::UpdateArtistScrapedMBID(int idArtist, const std::string& strMusicBrainzArtistID)
{
  return m_crudRepo.UpdateArtistScrapedMBID(idArtist, strMusicBrainzArtistID);
}


bool CMusicDatabase::GetArtist(int idArtist, CArtist& artist, bool fetchAll /* = false */)
{
  return m_crudRepo.GetArtist(idArtist, artist, fetchAll);
}


bool CMusicDatabase::GetArtistExists(int idArtist)
{
  return m_crudRepo.GetArtistExists(idArtist);
}


int CMusicDatabase::GetLastArtist() const
{
  return m_crudRepo.GetLastArtist();
}


int CMusicDatabase::GetArtistFromMBID(const std::string& strMusicBrainzArtistID, std::string& artistname)
{
  return m_crudRepo.GetArtistFromMBID(strMusicBrainzArtistID, artistname);
}


bool CMusicDatabase::HasArtistBeenScraped(int idArtist) const
{
  return m_crudRepo.HasArtistBeenScraped(idArtist);
}


bool CMusicDatabase::ClearArtistLastScrapedTime(int idArtist)
{
  return m_crudRepo.ClearArtistLastScrapedTime(idArtist);
}


bool CMusicDatabase::AddArtistVideoLinks(const CArtist& artist)
{
  return m_crudRepo.AddArtistVideoLinks(artist);
}


bool CMusicDatabase::DeleteArtistVideoLinks(const int idArtist)
{
  return m_crudRepo.DeleteArtistVideoLinks(idArtist);
}


int CMusicDatabase::AddArtistDiscography(int idArtist, const CDiscoAlbum& discoAlbum)
{
  return m_crudRepo.AddArtistDiscography(idArtist, discoAlbum);
}


bool CMusicDatabase::DeleteArtistDiscography(int idArtist)
{
  return m_crudRepo.DeleteArtistDiscography(idArtist);
}


bool CMusicDatabase::GetArtistDiscography(int idArtist, CFileItemList& items)
{
  return m_crudRepo.GetArtistDiscography(idArtist, items);
}


int CMusicDatabase::AddRole(std::string_view strRole)
{
  return m_crudRepo.AddRole(strRole);
}


bool CMusicDatabase::AddSongArtist(
    int idArtist, int idSong, std::string_view strRole, std::string_view strArtist, int iOrder)
{
  return m_crudRepo.AddSongArtist(idArtist, idSong, strRole, strArtist, iOrder);
}

bool CMusicDatabase::AddSongArtist(
    int idArtist, int idSong, int idRole, std::string_view strArtist, int iOrder)
{
  return m_crudRepo.AddSongArtist(idArtist, idSong, idRole, strArtist, iOrder);
}

int CMusicDatabase::AddSongContributor(int idSong,
                                     const std::string& strRole,
                                     const std::string& strArtist,
                                     const std::string& strSort)
{
  return m_crudRepo.AddSongContributor(idSong, strRole, strArtist, strSort);
}


void CMusicDatabase::AddSongContributors(int idSong,
                                       const std::vector<CMusicRole>& contributors,
                                       const std::string& strSort)
{
  m_crudRepo.AddSongContributors(idSong, contributors, strSort);
}


int CMusicDatabase::GetRoleByName(const std::string& strRole)
{
  return m_crudRepo.GetRoleByName(strRole);
}


bool CMusicDatabase::GetRolesByArtist(int idArtist, CFileItem* item)
{
  return m_crudRepo.GetRolesByArtist(idArtist, item);
}


bool CMusicDatabase::DeleteSongArtistsBySong(int idSong)
{
  return m_crudRepo.DeleteSongArtistsBySong(idSong);
}


bool CMusicDatabase::AddAlbumArtist(int idArtist, int idAlbum, std::string_view strArtist, int iOrder)
{
  return m_crudRepo.AddAlbumArtist(idArtist, idAlbum, strArtist, iOrder);
}


bool CMusicDatabase::DeleteAlbumArtistsByAlbum(int idAlbum)
{
  return m_crudRepo.DeleteAlbumArtistsByAlbum(idAlbum);
}


bool CMusicDatabase::AddSongGenres(int idSong, const std::vector<std::string>& genres)
{
  return m_crudRepo.AddSongGenres(idSong, genres);
}


bool CMusicDatabase::GetAlbumsByArtist(int idArtist, std::vector<int>& albums)
{
  return m_crudRepo.GetAlbumsByArtist(idArtist, albums);
}


bool CMusicDatabase::GetArtistsByAlbum(int idAlbum, CFileItem* item)
{
  return m_crudRepo.GetArtistsByAlbum(idAlbum, item);
}


bool CMusicDatabase::GetArtistsByAlbum(int idAlbum, std::vector<std::string>& artistIDs)
{
  return m_crudRepo.GetArtistsByAlbum(idAlbum, artistIDs);
}


bool CMusicDatabase::GetSongsByArtist(int idArtist, std::vector<int>& songs)
{
  return m_crudRepo.GetSongsByArtist(idArtist, songs);
}


bool CMusicDatabase::GetArtistsBySong(int idSong, std::vector<int>& artists)
{
  return m_crudRepo.GetArtistsBySong(idSong, artists);
}


bool CMusicDatabase::GetGenresByArtist(int idArtist, CFileItem* item)
{
  return m_crudRepo.GetGenresByArtist(idArtist, item);
}


bool CMusicDatabase::GetGenresByAlbum(int idAlbum, CFileItem* item)
{
  return m_crudRepo.GetGenresByAlbum(idAlbum, item);
}


bool CMusicDatabase::GetGenresBySong(int idSong, std::vector<int>& genres)
{
  return m_crudRepo.GetGenresBySong(idSong, genres);
}


bool CMusicDatabase::GetIsAlbumArtist(int idArtist, CFileItem* item) const
{
  return m_crudRepo.GetIsAlbumArtist(idArtist, item);
}



int CMusicDatabase::AddPath(const std::string& strPath1)
{
  std::string strSQL;
  try
  {
    std::string strPath(strPath1);
    if (!URIUtils::HasSlashAtEnd(strPath))
      URIUtils::AddSlashAtEnd(strPath);

    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    auto it = m_pathCache.find(strPath);
    if (it != m_pathCache.end())
      return it->second;

    strSQL = PrepareSQL("SELECT * FROM path WHERE strPath='%s'", strPath.c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      // doesn't exists, add it
      strSQL = PrepareSQL("INSERT INTO path (idPath, strPath) "
                          "VALUES(NULL, '%s')",
                          strPath.c_str());
      m_pDS->exec(strSQL);

      const auto idPath = static_cast<int>(m_pDS->lastinsertid());
      m_pathCache.try_emplace(strPath, idPath);
      return idPath;
    }
    else
    {
      int idPath = m_pDS->fv("idPath").get_asInt();
      m_pathCache.try_emplace(strPath, idPath);
      m_pDS->close();
      return idPath;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "musicdatabase:unable to addpath ({})", strSQL);
  }

  return -1;
}

CSong CMusicDatabase::GetSongFromDataset()
{
  return CMusicDatasetHelper::GetSongFromDataset(m_pDS->get_sql_record());
}

CSong CMusicDatabase::GetSongFromDataset(const dbiplus::sql_record* const record, int offset) const
{
  return CMusicDatasetHelper::GetSongFromDataset(record, offset);
}


void CMusicDatabase::GetFileItemFromDataset(CFileItem* item, const CMusicDbUrl& baseUrl)
{
  CMusicDatasetHelper::GetFileItemFromDataset(m_pDS->get_sql_record(), item, baseUrl);
}

void CMusicDatabase::GetFileItemFromDataset(const dbiplus::sql_record* const record, CFileItem* item, const CMusicDbUrl& baseUrl) const
{
  CMusicDatasetHelper::GetFileItemFromDataset(record, item, baseUrl);
}


void CMusicDatabase::GetFileItemFromArtistCredits(std::vector<CArtistCredit>& artistCredits, CFileItem* item) const
{
  CMusicDatasetHelper::GetFileItemFromArtistCredits(artistCredits, item);
}


CAlbum CMusicDatabase::GetAlbumFromDataset(dbiplus::Dataset* pDS, int offset, bool imageURL) const
{
  return CMusicDatasetHelper::GetAlbumFromDataset(pDS, offset, imageURL);
}


CAlbum CMusicDatabase::GetAlbumFromDataset(const dbiplus::sql_record* const record, int offset, bool imageURL) const
{
  return CMusicDatasetHelper::GetAlbumFromDataset(record, offset, imageURL);
}


CArtistCredit CMusicDatabase::GetArtistCreditFromDataset(const dbiplus::sql_record* const record, int offset) const
{
  return CMusicDatasetHelper::GetArtistCreditFromDataset(record, offset);
}


CMusicRole CMusicDatabase::GetArtistRoleFromDataset(const dbiplus::sql_record* const record, int offset) const
{
  return CMusicDatasetHelper::GetArtistRoleFromDataset(record, offset);
}


CArtist CMusicDatabase::GetArtistFromDataset(dbiplus::Dataset* pDS, int offset, bool needThumb) const
{
  return CMusicDatasetHelper::GetArtistFromDataset(pDS, offset, needThumb, m_translateBlankArtist);
}


CArtist CMusicDatabase::GetArtistFromDataset(const dbiplus::sql_record* const record, int offset, bool needThumb) const
{
  return CMusicDatasetHelper::GetArtistFromDataset(record, offset, needThumb, m_translateBlankArtist);
}


bool CMusicDatabase::GetSongByFileName(const std::string& strFileNameAndPath, CSong& song, int64_t startOffset)
{
  return m_crudRepo.GetSongByFileName(strFileNameAndPath, song, startOffset);
}


int CMusicDatabase::GetAlbumIdByPath(const std::string& path)
{
  return m_crudRepo.GetAlbumIdByPath(path);
}


int CMusicDatabase::GetSongByArtistAndAlbumAndTitle(const std::string& strArtist, const std::string& strAlbum, const std::string& strTitle)
{
  return m_crudRepo.GetSongByArtistAndAlbumAndTitle(strArtist, strAlbum, strTitle);
}


bool CMusicDatabase::SearchArtists(const std::string& search, CFileItemList& artists)
{
  return m_searchService.SearchArtists(search, artists);
}


bool CMusicDatabase::GetTop100(const std::string& strBaseDir, CFileItemList& items)
{
  return m_playlistService.GetTop100(strBaseDir, items);
}


bool CMusicDatabase::GetTop100Albums(std::vector<CAlbum>& albums)
{
  return m_playlistService.GetTop100Albums(albums);
}


bool CMusicDatabase::GetTop100AlbumSongs(const std::string& strBaseDir, CFileItemList& items)
{
  return m_playlistService.GetTop100AlbumSongs(strBaseDir, items);
}


bool CMusicDatabase::GetRecentlyPlayedAlbums(std::vector<CAlbum>& albums)
{
  return m_playlistService.GetRecentlyPlayedAlbums(albums);
}


bool CMusicDatabase::GetRecentlyPlayedAlbumSongs(const std::string& strBaseDir, CFileItemList& items)
{
  return m_playlistService.GetRecentlyPlayedAlbumSongs(strBaseDir, items);
}


bool CMusicDatabase::GetRecentlyAddedAlbums(std::vector<CAlbum>& albums, unsigned int limit)
{
  return m_playlistService.GetRecentlyAddedAlbums(albums, limit);
}


bool CMusicDatabase::GetRecentlyAddedAlbumSongs(const std::string& strBaseDir, CFileItemList& items, unsigned int limit)
{
  return m_playlistService.GetRecentlyAddedAlbumSongs(strBaseDir, items, limit);
}


void CMusicDatabase::IncrementPlayCount(const CFileItem& item)
{
  m_playlistService.IncrementPlayCount(item);
}


bool CMusicDatabase::GetSongsByPath(const std::string& strPath,
                                  std::map<std::string, std::vector<CSong>>& songmap,
                                  bool bAppendToMap)
{
  return m_crudRepo.GetSongsByPath(strPath, songmap, bAppendToMap);
}


void CMusicDatabase::EmptyCache()
{
  m_genreCache.erase(m_genreCache.begin(), m_genreCache.end());
  m_pathCache.erase(m_pathCache.begin(), m_pathCache.end());
}

bool CMusicDatabase::Search(const std::string& search, CFileItemList& items)
{
  return m_searchService.Search(search, items);
}


bool CMusicDatabase::SearchSongs(const std::string& search, CFileItemList& items)
{
  return m_searchService.SearchSongs(search, items);
}


bool CMusicDatabase::SearchAlbums(const std::string& search, CFileItemList& albums)
{
  return m_searchService.SearchAlbums(search, albums);
}


bool CMusicDatabase::CleanupSongsByIds(const std::string& strSongIds)
{
  return m_maintenanceService.CleanupSongsByIds(strSongIds);
}


bool CMusicDatabase::CleanupSongs(CGUIDialogProgress* progressDialog /*= nullptr*/)
{
  return m_maintenanceService.CleanupSongs(progressDialog);
}


bool CMusicDatabase::CleanupAlbums()
{
  return m_maintenanceService.CleanupAlbums();
}


bool CMusicDatabase::CleanupPaths()
{
  return m_maintenanceService.CleanupPaths();
}


bool CMusicDatabase::InsideScannedPath(const std::string& path) const
{
  std::string sql = PrepareSQL("SELECT idPath FROM path WHERE SUBSTR(strPath,1,%i)='%s' LIMIT 1",
                               path.size(), path.c_str());
  return !GetSingleValue(sql).empty();
}

bool CMusicDatabase::CleanupArtists()
{
  return m_maintenanceService.CleanupArtists();
}


bool CMusicDatabase::CleanupGenres()
{
  return m_maintenanceService.CleanupGenres();
}


bool CMusicDatabase::CleanupInfoSettings()
{
  return m_maintenanceService.CleanupInfoSettings();
}


bool CMusicDatabase::CleanupRoles()
{
  return m_maintenanceService.CleanupRoles();
}


bool CMusicDatabase::DeleteRemovedLinks()
{
  return m_maintenanceService.DeleteRemovedLinks();
}


bool CMusicDatabase::CleanupOrphanedItems()
{
  return m_maintenanceService.CleanupOrphanedItems();
}


int CMusicDatabase::Cleanup(CGUIDialogProgress* progressDialog /*= nullptr*/)
{
  return m_maintenanceService.Cleanup(progressDialog);
}


bool CMusicDatabase::TrimImageURLs(std::string& strImage, const size_t space) const
{
  return m_maintenanceService.TrimImageURLs(strImage, space);
}


bool CMusicDatabase::LookupCDDBInfo(bool bRequery /*=false*/) const
{
  return m_maintenanceService.LookupCDDBInfo(bRequery);
}


void CMusicDatabase::DeleteCDDBInfo() const
{
  m_maintenanceService.DeleteCDDBInfo();
}


void CMusicDatabase::Clean() const
{
  m_maintenanceService.Clean();
}


bool CMusicDatabase::GetGenresNav(const std::string& strBaseDir, CFileItemList& items,
                                const Filter& filter, bool countOnly)
{
  return m_navRepo.GetGenresNav(strBaseDir, items, filter, countOnly);
}


bool CMusicDatabase::GetSourcesNav(const std::string& strBaseDir, CFileItemList& items,
                                 const Filter& filter, bool countOnly)
{
  return m_navRepo.GetSourcesNav(strBaseDir, items, filter, countOnly);
}


bool CMusicDatabase::GetYearsNav(const std::string& strBaseDir, CFileItemList& items, const Filter& filter)
{
  return m_navRepo.GetYearsNav(strBaseDir, items, filter);
}


bool CMusicDatabase::GetRolesNav(const std::string& strBaseDir, CFileItemList& items, const Filter& filter)
{
  return m_navRepo.GetRolesNav(strBaseDir, items, filter);
}


bool CMusicDatabase::GetAlbumsByYear(const std::string& strBaseDir, CFileItemList& items, int year)
{
  return m_navRepo.GetAlbumsByYear(strBaseDir, items, year);
}


bool CMusicDatabase::GetCommonNav(const std::string& strBaseDir, const std::string& table,
                                const std::string& labelField, CFileItemList& items,
                                const Filter& filter, bool countOnly)
{
  return m_navRepo.GetCommonNav(strBaseDir, table, labelField, items, filter, countOnly);
}


bool CMusicDatabase::GetAlbumTypesNav(const std::string& strBaseDir, CFileItemList& items, const Filter& filter, bool countOnly)
{
  return m_navRepo.GetAlbumTypesNav(strBaseDir, items, filter, countOnly);
}


bool CMusicDatabase::GetMusicLabelsNav(const std::string& strBaseDir, CFileItemList& items, const Filter& filter, bool countOnly)
{
  return m_navRepo.GetMusicLabelsNav(strBaseDir, items, filter, countOnly);
}


bool CMusicDatabase::GetArtistsNav(const std::string& strBaseDir, CFileItemList& items,
                                 const SortDescription& sortDescription, bool albumArtistsOnly,
                                 int idGenre, int idAlbum, int idSong,
                                 const Filter& filter, bool countOnly)
{
  return m_navRepo.GetArtistsNav(strBaseDir, items, sortDescription, albumArtistsOnly,
                                 idGenre, idAlbum, idSong, filter, countOnly);
}


bool CMusicDatabase::GetArtistsByWhere(const std::string& strBaseDir, CFileItemList& items,
                                     const SortDescription& sortDescription,
                                     const Filter& filter, bool countOnly)
{
  return m_navRepo.GetArtistsByWhere(strBaseDir, items, sortDescription, filter, countOnly);
}


bool CMusicDatabase::GetAlbumFromSong(int idSong, CAlbum& album)
{
  return m_crudRepo.GetAlbumFromSong(idSong, album);
}


bool CMusicDatabase::GetAlbumsNav(const std::string& strBaseDir, CFileItemList& items,
                                const SortDescription& sortDescription,
                                int idGenre, int idArtist,
                                const Filter& filter, bool countOnly)
{
  return m_navRepo.GetAlbumsNav(strBaseDir, items, sortDescription, idGenre, idArtist,
                                filter, countOnly);
}


bool CMusicDatabase::GetAlbumsByWhere(const std::string& baseDir, CFileItemList& items,
                                    const SortDescription& sortDescription,
                                    const Filter& filter, bool countOnly)
{
  return m_navRepo.GetAlbumsByWhere(baseDir, items, sortDescription, filter, countOnly);
}


bool CMusicDatabase::GetDiscsNav(const std::string& strBaseDir, CFileItemList& items,
                               const SortDescription& sortDescription, int idAlbum,
                               const Filter& filter, bool countOnly)
{
  return m_navRepo.GetDiscsNav(strBaseDir, items, sortDescription, idAlbum, filter, countOnly);
}


bool CMusicDatabase::GetDiscsByWhere(const std::string& baseDir, CFileItemList& items,
                                   const SortDescription& sortDescription,
                                   const Filter& filter, bool countOnly)
{
  return m_navRepo.GetDiscsByWhere(baseDir, items, sortDescription, filter, countOnly);
}


bool CMusicDatabase::GetDiscsByWhere(CMusicDbUrl& musicUrl, CFileItemList& items,
                                   const SortDescription& sortDescription,
                                   const Filter& filter, bool countOnly)
{
  return m_navRepo.GetDiscsByWhere(musicUrl, items, sortDescription, filter, countOnly);
}

int CMusicDatabase::GetDiscsCount(const std::string& baseDir, const Filter& filter /* = Filter() */)
{
  return m_navRepo.GetDiscsCount(baseDir, filter);
}


bool CMusicDatabase::GetSongsFullByWhere(const std::string& baseDir, CFileItemList& items,
                                       const SortDescription& sortDescription,
                                       const Filter& filter, bool artistData)
{
  return m_navRepo.GetSongsFullByWhere(baseDir, items, sortDescription, filter, artistData);
}


bool CMusicDatabase::GetSongsByYear(const std::string& baseDir, CFileItemList& items, int year)
{
  return m_navRepo.GetSongsByYear(baseDir, items, year);
}


bool CMusicDatabase::GetSongsNav(const std::string& strBaseDir, CFileItemList& items,
                               const SortDescription& sortDescription,
                               int idGenre, int idArtist, int idAlbum)
{
  return m_navRepo.GetSongsNav(strBaseDir, items, sortDescription, idGenre, idArtist, idAlbum);
}


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

bool CMusicDatabase::GetArtistsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                         const std::string& baseDir,
                                         CVariant& result, int& total,
                                         const SortDescription& sortDescription)
{
  return CMusicQueryBuilder::GetArtistsByWhereJSON(fields, baseDir, result, total,
                                                   sortDescription, *this);
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

bool CMusicDatabase::GetAlbumsByWhereJSON(const std::set<std::string, std::less<>>& fields,
                                        const std::string& baseDir,
                                        CVariant& result, int& total,
                                        const SortDescription& sortDescription)
{
  return CMusicQueryBuilder::GetAlbumsByWhereJSON(fields, baseDir, result, total,
                                                  sortDescription, *this);
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

bool CMusicDatabase::GetSongsByWhereJSON(
    const std::set<std::string, std::less<>>& fields,
    const std::string& baseDir,
    CVariant& result, int& total,
    const SortDescription& sortDescription)
{
  return CMusicQueryBuilder::GetSongsByWhereJSON(fields, baseDir, result, total,
                                                 sortDescription, *this);
}


std::string CMusicDatabase::GetIgnoreArticleSQL(const std::string& strField) const
{
  return CMusicQueryBuilder::GetIgnoreArticleSQL(strField, *this);
}


std::string CMusicDatabase::SortnameBuildSQL(const std::string& strAlias,
                                           const SortAttribute& sortAttributes,
                                           const std::string& strField,
                                           const std::string& strSortField) const
{
  return CMusicQueryBuilder::SortnameBuildSQL(strAlias, sortAttributes, strField, strSortField, *this);
}


std::string CMusicDatabase::AlphanumericSortSQL(const std::string& strField, const SortOrder& sortOrder) const
{
  return CMusicQueryBuilder::AlphanumericSortSQL(strField, sortOrder, *this);
}


void CMusicDatabase::UpdateTables(int version)
{
  CLog::Log(LOGINFO, "updating tables");
  if (version < 34)
  {
    m_pDS->exec("ALTER TABLE artist ADD strMusicBrainzArtistID text\n");
    m_pDS->exec("ALTER TABLE album ADD strMusicBrainzAlbumID text\n");
    m_pDS->exec(
        "CREATE TABLE song_new ( idSong integer primary key, idAlbum integer, idPath integer, "
        "strArtists text, strGenres text, strTitle varchar(512), iTrack integer, iDuration "
        "integer, iYear integer, dwFileNameCRC text, strFileName text, strMusicBrainzTrackID text, "
        "iTimesPlayed integer, iStartOffset integer, iEndOffset integer, idThumb integer, "
        "lastplayed varchar(20) default NULL, rating char default '0', comment text)\n");
    m_pDS->exec("INSERT INTO song_new ( idSong, idAlbum, idPath, strArtists, strTitle, iTrack, "
                "iDuration, iYear, dwFileNameCRC, strFileName, strMusicBrainzTrackID, "
                "iTimesPlayed, iStartOffset, iEndOffset, idThumb, lastplayed, rating, comment) "
                "SELECT idSong, idAlbum, idPath, strArtists, strTitle, iTrack, iDuration, iYear, "
                "dwFileNameCRC, strFileName, strMusicBrainzTrackID, iTimesPlayed, iStartOffset, "
                "iEndOffset, idThumb, lastplayed, rating, comment FROM song");

    m_pDS->exec("DROP TABLE song");
    m_pDS->exec("ALTER TABLE song_new RENAME TO song");

    m_pDS->exec("UPDATE song SET strMusicBrainzTrackID = NULL");
  }

  if (version < 36)
  {
    // translate legacy musicdb:// paths
    if (m_pDS->query("SELECT strPath FROM content"))
    {
      std::vector<std::string> contentPaths;
      while (!m_pDS->eof())
      {
        contentPaths.push_back(m_pDS->fv(0).get_asString());
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto& originalPath : contentPaths)
      {
        std::string path = CLegacyPathTranslation::TranslateMusicDbPath(originalPath);
        m_pDS->exec(PrepareSQL("UPDATE content SET strPath='%s' WHERE strPath='%s'", path.c_str(),
                               originalPath.c_str()));
      }
    }
  }

  if (version < 39)
  {
    m_pDS->exec("CREATE TABLE album_new "
                "(idAlbum integer primary key, "
                " strAlbum varchar(256), strMusicBrainzAlbumID text, "
                " strArtists text, strGenres text, "
                " iYear integer, idThumb integer, "
                " bCompilation integer not null default '0', "
                " strMoods text, strStyles text, strThemes text, "
                " strReview text, strImage text, strLabel text, "
                " strType text, "
                " iRating integer, "
                " lastScraped varchar(20) default NULL, "
                " dateAdded varchar (20) default NULL)");
    m_pDS->exec("INSERT INTO album_new "
                "(idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, "
                " iRating) "
                " SELECT "
                " album.idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " album.iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, iRating "
                " FROM album LEFT JOIN albuminfo ON album.idAlbum = albuminfo.idAlbum");
    m_pDS->exec("UPDATE albuminfosong SET idAlbumInfo = (SELECT idAlbum FROM albuminfo WHERE "
                "albuminfo.idAlbumInfo = albuminfosong.idAlbumInfo)");
    m_pDS->exec(PrepareSQL(
        "UPDATE album_new SET lastScraped='%s' WHERE idAlbum IN (SELECT idAlbum FROM albuminfo)",
        CDateTime::GetCurrentDateTime().GetAsDBDateTime().c_str()));
    m_pDS->exec("DROP TABLE album");
    m_pDS->exec("DROP TABLE albuminfo");
    m_pDS->exec("ALTER TABLE album_new RENAME TO album");
  }
  if (version < 40)
  {
    m_pDS->exec("CREATE TABLE artist_new ( idArtist integer primary key, "
                " strArtist varchar(256), strMusicBrainzArtistID text, "
                " strBorn text, strFormed text, strGenres text, strMoods text, "
                " strStyles text, strInstruments text, strBiography text, "
                " strDied text, strDisbanded text, strYearsActive text, "
                " strImage text, strFanart text, "
                " lastScraped varchar(20) default NULL, "
                " dateAdded varchar (20) default NULL)");
    m_pDS->exec("INSERT INTO artist_new "
                "(idArtist, strArtist, strMusicBrainzArtistID, "
                " strBorn, strFormed, strGenres, strMoods, "
                " strStyles , strInstruments , strBiography , "
                " strDied, strDisbanded, strYearsActive, "
                " strImage, strFanart) "
                " SELECT "
                " artist.idArtist, "
                " strArtist, strMusicBrainzArtistID, "
                " strBorn, strFormed, strGenres, strMoods, "
                " strStyles, strInstruments, strBiography, "
                " strDied, strDisbanded, strYearsActive, "
                " strImage, strFanart "
                " FROM artist "
                " LEFT JOIN artistinfo ON artist.idArtist = artistinfo.idArtist");
    m_pDS->exec(PrepareSQL("UPDATE artist_new SET lastScraped='%s' WHERE idArtist IN (SELECT "
                           "idArtist FROM artistinfo)",
                           CDateTime::GetCurrentDateTime().GetAsDBDateTime().c_str()));
    m_pDS->exec("DROP TABLE artist");
    m_pDS->exec("DROP TABLE artistinfo");
    m_pDS->exec("ALTER TABLE artist_new RENAME TO artist");
  }
  if (version < 42)
  {
    m_pDS->exec("ALTER TABLE album_artist ADD strArtist text\n");
    m_pDS->exec("ALTER TABLE song_artist ADD strArtist text\n");
    // populate these
    std::string sql = "select idArtist,strArtist from artist";
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("UPDATE song_artist SET strArtist='%s' where idArtist=%i",
                              m_pDS->fv(1).get_asString().c_str(), m_pDS->fv(0).get_asInt()));
      m_pDS2->exec(PrepareSQL("UPDATE album_artist SET strArtist='%s' where idArtist=%i",
                              m_pDS->fv(1).get_asString().c_str(), m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
  }
  if (version < 48)
  { // null out columns that are no longer used
    m_pDS->exec("UPDATE song SET dwFileNameCRC=NULL, idThumb=NULL");
    m_pDS->exec("UPDATE album SET idThumb=NULL");
  }
  if (version < 49)
  {
    m_pDS->exec("CREATE TABLE cue (idPath integer, strFileName text, strCuesheet text)");
  }
  if (version < 50)
  {
    // add a new column strReleaseType for albums
    m_pDS->exec("ALTER TABLE album ADD strReleaseType text\n");

    // set strReleaseType based on album name
    m_pDS->exec(PrepareSQL(
        "UPDATE album SET strReleaseType = '%s' WHERE strAlbum IS NOT NULL AND strAlbum <> ''",
        CAlbum::ReleaseTypeToString(ReleaseType::Album).c_str()));
    m_pDS->exec(
        PrepareSQL("UPDATE album SET strReleaseType = '%s' WHERE strAlbum IS NULL OR strAlbum = ''",
                   CAlbum::ReleaseTypeToString(ReleaseType::Single).c_str()));
  }
  if (version < 51)
  {
    m_pDS->exec("ALTER TABLE song ADD mood text\n");
  }
  if (version < 53)
  {
    m_pDS->exec("ALTER TABLE song ADD dateAdded text");
  }
  if (version < 54)
  {
    //Remove dateAdded from artist table
    m_pDS->exec("CREATE TABLE artist_new ( idArtist integer primary key, "
                " strArtist varchar(256), strMusicBrainzArtistID text, "
                " strBorn text, strFormed text, strGenres text, strMoods text, "
                " strStyles text, strInstruments text, strBiography text, "
                " strDied text, strDisbanded text, strYearsActive text, "
                " strImage text, strFanart text, "
                " lastScraped varchar(20) default NULL)");
    m_pDS->exec("INSERT INTO artist_new "
                "(idArtist, strArtist, strMusicBrainzArtistID, "
                " strBorn, strFormed, strGenres, strMoods, "
                " strStyles , strInstruments , strBiography , "
                " strDied, strDisbanded, strYearsActive, "
                " strImage, strFanart, lastScraped) "
                " SELECT "
                " idArtist, "
                " strArtist, strMusicBrainzArtistID, "
                " strBorn, strFormed, strGenres, strMoods, "
                " strStyles, strInstruments, strBiography, "
                " strDied, strDisbanded, strYearsActive, "
                " strImage, strFanart, lastScraped "
                " FROM artist");
    m_pDS->exec("DROP TABLE artist");
    m_pDS->exec("ALTER TABLE artist_new RENAME TO artist");

    //Remove dateAdded from album table
    m_pDS->exec("CREATE TABLE album_new (idAlbum integer primary key, "
                " strAlbum varchar(256), strMusicBrainzAlbumID text, "
                " strArtists text, strGenres text, "
                " iYear integer, idThumb integer, "
                " bCompilation integer not null default '0', "
                " strMoods text, strStyles text, strThemes text, "
                " strReview text, strImage text, strLabel text, "
                " strType text, "
                " iRating integer, "
                " lastScraped varchar(20) default NULL, "
                " strReleaseType text)");
    m_pDS->exec("INSERT INTO album_new "
                "(idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, iRating, lastScraped, "
                " strReleaseType) "
                " SELECT "
                " album.idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, iRating, lastScraped, "
                " strReleaseType"
                " FROM album");
    m_pDS->exec("DROP TABLE album");
    m_pDS->exec("ALTER TABLE album_new RENAME TO album");
  }
  if (version < 55)
  {
    m_pDS->exec("DROP TABLE karaokedata");
  }
  if (version < 57)
  {
    m_pDS->exec("ALTER TABLE song ADD userrating INTEGER NOT NULL DEFAULT 0");
    m_pDS->exec("UPDATE song SET rating = 0 WHERE rating < 0 or rating IS NULL");
    m_pDS->exec("UPDATE song SET userrating = rating * 2");
    m_pDS->exec("UPDATE song SET rating = 0");
    m_pDS->exec("CREATE TABLE song_new (idSong INTEGER PRIMARY KEY, "
                " idAlbum INTEGER, idPath INTEGER, "
                " strArtists TEXT, strGenres TEXT, strTitle VARCHAR(512), "
                " iTrack INTEGER, iDuration INTEGER, iYear INTEGER, "
                " dwFileNameCRC TEXT, "
                " strFileName TEXT, strMusicBrainzTrackID TEXT, "
                " iTimesPlayed INTEGER, iStartOffset INTEGER, iEndOffset INTEGER, "
                " idThumb INTEGER, "
                " lastplayed VARCHAR(20) DEFAULT NULL, "
                " rating FLOAT DEFAULT 0, "
                " userrating INTEGER DEFAULT 0, "
                " comment TEXT, mood TEXT, dateAdded TEXT)");
    m_pDS->exec("INSERT INTO song_new "
                "(idSong, "
                " idAlbum, idPath, "
                " strArtists, strGenres, strTitle, "
                " iTrack, iDuration, iYear, "
                " dwFileNameCRC, "
                " strFileName, strMusicBrainzTrackID, "
                " iTimesPlayed, iStartOffset, iEndOffset, "
                " idThumb, "
                " lastplayed,"
                " rating, userrating, "
                " comment, mood, dateAdded)"
                " SELECT "
                " idSong, "
                " idAlbum, idPath, "
                " strArtists, strGenres, strTitle, "
                " iTrack, iDuration, iYear, "
                " dwFileNameCRC, "
                " strFileName, strMusicBrainzTrackID, "
                " iTimesPlayed, iStartOffset, iEndOffset, "
                " idThumb, "
                " lastplayed,"
                " rating, "
                " userrating, "
                " comment, mood, dateAdded"
                " FROM song");
    m_pDS->exec("DROP TABLE song");
    m_pDS->exec("ALTER TABLE song_new RENAME TO song");

    m_pDS->exec("ALTER TABLE album ADD iUserrating INTEGER NOT NULL DEFAULT 0");
    m_pDS->exec("UPDATE album SET iRating = 0 WHERE iRating < 0 or iRating IS NULL");
    m_pDS->exec("CREATE TABLE album_new (idAlbum INTEGER PRIMARY KEY, "
                " strAlbum VARCHAR(256), strMusicBrainzAlbumID TEXT, "
                " strArtists TEXT, strGenres TEXT, "
                " iYear INTEGER, idThumb INTEGER, "
                " bCompilation INTEGER NOT NULL DEFAULT '0', "
                " strMoods TEXT, strStyles TEXT, strThemes TEXT, "
                " strReview TEXT, strImage TEXT, strLabel TEXT, "
                " strType TEXT, "
                " fRating FLOAT NOT NULL DEFAULT 0, "
                " iUserrating INTEGER NOT NULL DEFAULT 0, "
                " lastScraped VARCHAR(20) DEFAULT NULL, "
                " strReleaseType TEXT)");
    m_pDS->exec("INSERT INTO album_new "
                "(idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, "
                " fRating, "
                " iUserrating, "
                " lastScraped, "
                " strReleaseType)"
                " SELECT "
                " idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, strGenres, "
                " iYear, idThumb, "
                " bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, "
                " iRating, "
                " iUserrating, "
                " lastScraped, "
                " strReleaseType"
                " FROM album");
    m_pDS->exec("DROP TABLE album");
    m_pDS->exec("ALTER TABLE album_new RENAME TO album");

    m_pDS->exec("ALTER TABLE album ADD iVotes INTEGER NOT NULL DEFAULT 0");
    m_pDS->exec("ALTER TABLE song ADD votes INTEGER NOT NULL DEFAULT 0");
  }
  if (version < 58)
  {
    m_pDS->exec("UPDATE album SET fRating = fRating * 2");
  }
  if (version < 59)
  {
    m_pDS->exec("CREATE TABLE role (idRole integer primary key, strRole text)");
    m_pDS->exec("INSERT INTO role(idRole, strRole) VALUES (1, 'Artist')"); //Default Role

    //Remove strJoinPhrase, boolFeatured from song_artist table and add idRole
    m_pDS->exec("CREATE TABLE song_artist_new (idArtist integer, idSong integer, idRole integer, "
                "iOrder integer, strArtist text)");
    m_pDS->exec("INSERT INTO song_artist_new (idArtist, idSong, idRole, iOrder, strArtist) "
                "SELECT idArtist, idSong, 1 as idRole, iOrder, strArtist FROM song_artist");
    m_pDS->exec("DROP TABLE song_artist");
    m_pDS->exec("ALTER TABLE song_artist_new RENAME TO song_artist");

    //Remove strJoinPhrase, boolFeatured from album_artist table
    m_pDS->exec("CREATE TABLE album_artist_new (idArtist integer, idAlbum integer, iOrder integer, "
                "strArtist text)");
    m_pDS->exec("INSERT INTO album_artist_new (idArtist, idAlbum, iOrder, strArtist) "
                "SELECT idArtist, idAlbum, iOrder, strArtist FROM album_artist");
    m_pDS->exec("DROP TABLE album_artist");
    m_pDS->exec("ALTER TABLE album_artist_new RENAME TO album_artist");
  }
  if (version < 60)
  {
    // From now on artist ID = 1 will be an artificial artist [Missing] use for songs that
    // do not have an artist tag to ensure all songs in the library have at least one artist.
    std::string strSQL;
    if (GetArtistExists(BLANKARTIST_ID))
    {
      // When BLANKARTIST_ID (=1) is already in use, move the record
      try
      { //No mbid index yet, so can have record for artist twice even with mbid
        strSQL = PrepareSQL("INSERT INTO artist SELECT null, "
                            "strArtist, strMusicBrainzArtistID, "
                            "strBorn, strFormed, strGenres, strMoods, "
                            "strStyles, strInstruments, strBiography, "
                            "strDied, strDisbanded, strYearsActive, "
                            "strImage, strFanart, lastScraped "
                            "FROM artist WHERE artist.idArtist = %i",
                            BLANKARTIST_ID);
        m_pDS->exec(strSQL);
        const auto idArtist = static_cast<int>(m_pDS->lastinsertid());
        //No triggers, so can delete artist without effecting other tables.
        strSQL = PrepareSQL("DELETE FROM artist WHERE artist.idArtist = %i", BLANKARTIST_ID);
        m_pDS->exec(strSQL);

        // Update related tables with the new artist ID
        // Indices have been dropped making transactions very slow, so create appropriate temp indices
        m_pDS->exec("CREATE INDEX idxSongArtist2 ON song_artist ( idArtist )");
        m_pDS->exec("CREATE INDEX idxAlbumArtist2 ON album_artist ( idArtist )");
        m_pDS->exec("CREATE INDEX idxDiscography ON discography ( idArtist )");
        m_pDS->exec("CREATE INDEX ix_art ON art ( media_id, media_type(20) )");
        strSQL = PrepareSQL("UPDATE song_artist SET idArtist = %i WHERE idArtist = %i", idArtist,
                            BLANKARTIST_ID);
        m_pDS->exec(strSQL);
        strSQL = PrepareSQL("UPDATE album_artist SET idArtist = %i WHERE idArtist = %i", idArtist,
                            BLANKARTIST_ID);
        m_pDS->exec(strSQL);
        strSQL =
            PrepareSQL("UPDATE art SET media_id = %i WHERE media_id = %i AND media_type='artist'",
                       idArtist, BLANKARTIST_ID);
        m_pDS->exec(strSQL);
        strSQL = PrepareSQL("UPDATE discography SET idArtist = %i WHERE idArtist = %i", idArtist,
                            BLANKARTIST_ID);
        m_pDS->exec(strSQL);
        // Drop temp indices
        m_pDS->exec("DROP INDEX idxSongArtist2 ON song_artist");
        m_pDS->exec("DROP INDEX idxAlbumArtist2 ON album_artist");
        m_pDS->exec("DROP INDEX idxDiscography ON discography");
        m_pDS->exec("DROP INDEX ix_art ON art");
      }
      catch (...)
      {
        CLog::Log(LOGERROR, "Moving existing artist to add missing tag artist has failed");
      }
    }

    // Create missing artist tag artist [Missing].
    // Fake MusicbrainzId assures uniqueness and avoids updates from scanned songs
    strSQL = PrepareSQL(
        "INSERT INTO artist (idArtist, strArtist, strMusicBrainzArtistID) VALUES( %i, '%s', '%s' )",
        BLANKARTIST_ID, BLANKARTIST_NAME.data(), BLANKARTIST_FAKEMUSICBRAINZID.data());
    m_pDS->exec(strSQL);

    // Indices have been dropped making transactions very slow, so create temp index
    m_pDS->exec("CREATE INDEX idxSongArtist1 ON song_artist ( idSong, idRole )");
    m_pDS->exec("CREATE INDEX idxAlbumArtist1 ON album_artist ( idAlbum )");

    // Ensure all songs have at least one artist, set those without to [Missing]
    strSQL = "SELECT count(idSong) FROM song "
             "WHERE NOT EXISTS(SELECT idSong FROM song_artist "
             "WHERE song_artist.idsong = song.idsong AND song_artist.idRole = 1)";
    int numsongs = GetSingleValueInt(strSQL);
    if (numsongs > 0)
    {
      CLog::Log(LOGDEBUG, "{} songs have no artist, setting artist to [Missing]", numsongs);
      // Insert song_artist records for songs that don't have any
      try
      {
        strSQL = PrepareSQL("INSERT INTO song_artist(idArtist, idSong, idRole, strArtist, iOrder) "
                            "SELECT %i, idSong, %i, '%s', 0 FROM song "
                            "WHERE NOT EXISTS(SELECT idSong FROM song_artist "
                            "WHERE song_artist.idsong = song.idsong AND song_artist.idRole = %i)",
                            BLANKARTIST_ID, ROLE_ARTIST, BLANKARTIST_NAME.data(), ROLE_ARTIST);
        ExecuteQuery(strSQL);
      }
      catch (...)
      {
        CLog::Log(LOGERROR, "Setting missing artist for songs without an artist has failed");
      }
    }

    // Ensure all albums have at least one artist, set those without to [Missing]
    strSQL = "SELECT count(idAlbum) FROM album "
             "WHERE NOT EXISTS(SELECT idAlbum FROM album_artist "
             "WHERE album_artist.idAlbum = album.idAlbum)";
    int numalbums = GetSingleValueInt(strSQL);
    if (numalbums > 0)
    {
      CLog::Log(LOGDEBUG, "{} albums have no artist, setting artist to [Missing]", numalbums);
      // Insert album_artist records for albums that don't have any
      try
      {
        strSQL = PrepareSQL("INSERT INTO album_artist(idArtist, idAlbum, strArtist, iOrder) "
                            "SELECT %i, idAlbum, '%s', 0 FROM album "
                            "WHERE NOT EXISTS(SELECT idAlbum FROM album_artist "
                            "WHERE album_artist.idAlbum = album.idAlbum)",
                            BLANKARTIST_ID, BLANKARTIST_NAME.data());
        ExecuteQuery(strSQL);
      }
      catch (...)
      {
        CLog::Log(LOGERROR, "Setting artist missing for albums without an artist has failed");
      }
    }
    //Remove temp indices, full analytics for database created later
    m_pDS->exec("DROP INDEX idxSongArtist1 ON song_artist");
    m_pDS->exec("DROP INDEX idxAlbumArtist1 ON album_artist");
  }
  if (version < 61)
  {
    // Create versiontagscan table
    m_pDS->exec("CREATE TABLE versiontagscan (idVersion integer, iNeedsScan integer)");
    m_pDS->exec("INSERT INTO versiontagscan (idVersion, iNeedsScan) values(0, 0)");
  }
  if (version < 62)
  {
    CLog::Log(LOGINFO, "create audiobook table");
    m_pDS->exec("CREATE TABLE audiobook (idBook integer primary key, "
                " strBook varchar(256), strAuthor text,"
                " bookmark integer, file text,"
                " dateAdded varchar (20) default NULL)");
  }
  if (version < 63)
  {
    // Add strSortName to Artist table
    m_pDS->exec("ALTER TABLE artist ADD strSortName text\n");

    //Remove idThumb (column unused since v47), rename strArtists and add strArtistSort to album table
    m_pDS->exec("CREATE TABLE album_new (idAlbum integer primary key, "
                " strAlbum varchar(256), strMusicBrainzAlbumID text, "
                " strArtistDisp text, strArtistSort text, strGenres text, "
                " iYear integer, bCompilation integer not null default '0', "
                " strMoods text, strStyles text, strThemes text, "
                " strReview text, strImage text, strLabel text, "
                " strType text, "
                " fRating FLOAT NOT NULL DEFAULT 0, "
                " iUserrating INTEGER NOT NULL DEFAULT 0, "
                " lastScraped varchar(20) default NULL, "
                " strReleaseType text, "
                " iVotes INTEGER NOT NULL DEFAULT 0)");
    m_pDS->exec("INSERT INTO album_new "
                "(idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtistDisp, strArtistSort, strGenres, "
                " iYear, bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, "
                " fRating, iUserrating, iVotes, "
                " lastScraped, "
                " strReleaseType)"
                " SELECT "
                " idAlbum, "
                " strAlbum, strMusicBrainzAlbumID, "
                " strArtists, NULL, strGenres, "
                " iYear, bCompilation, "
                " strMoods, strStyles, strThemes, "
                " strReview, strImage, strLabel, "
                " strType, "
                " fRating, iUserrating, iVotes, "
                " lastScraped, "
                " strReleaseType"
                " FROM album");
    m_pDS->exec("DROP TABLE album");
    m_pDS->exec("ALTER TABLE album_new RENAME TO album");

    //Remove dwFileNameCRC, idThumb (columns unused since v47), rename strArtists and add strArtistSort to song table
    m_pDS->exec("CREATE TABLE song_new (idSong INTEGER PRIMARY KEY, "
                " idAlbum INTEGER, idPath INTEGER, "
                " strArtistDisp TEXT, strArtistSort TEXT, strGenres TEXT, strTitle VARCHAR(512), "
                " iTrack INTEGER, iDuration INTEGER, iYear INTEGER, "
                " strFileName TEXT, strMusicBrainzTrackID TEXT, "
                " iTimesPlayed INTEGER, iStartOffset INTEGER, iEndOffset INTEGER, "
                " lastplayed VARCHAR(20) DEFAULT NULL, "
                " rating FLOAT NOT NULL DEFAULT 0, votes INTEGER NOT NULL DEFAULT 0, "
                " userrating INTEGER NOT NULL DEFAULT 0, "
                " comment TEXT, mood TEXT, dateAdded TEXT)");
    m_pDS->exec("INSERT INTO song_new "
                "(idSong, "
                " idAlbum, idPath, "
                " strArtistDisp, strArtistSort, strGenres, strTitle, "
                " iTrack, iDuration, iYear, "
                " strFileName, strMusicBrainzTrackID, "
                " iTimesPlayed, iStartOffset, iEndOffset, "
                " lastplayed,"
                " rating, userrating, votes, "
                " comment, mood, dateAdded)"
                " SELECT "
                " idSong, "
                " idAlbum, idPath, "
                " strArtists, NULL, strGenres, strTitle, "
                " iTrack, iDuration, iYear, "
                " strFileName, strMusicBrainzTrackID, "
                " iTimesPlayed, iStartOffset, iEndOffset, "
                " lastplayed,"
                " rating, userrating, votes, "
                " comment, mood, dateAdded"
                " FROM song");
    m_pDS->exec("DROP TABLE song");
    m_pDS->exec("ALTER TABLE song_new RENAME TO song");
  }
  if (version < 65)
  {
    // Remove cue table
    m_pDS->exec("DROP TABLE cue");
    // Add strReplayGain to song table
    m_pDS->exec("ALTER TABLE song ADD strReplayGain TEXT\n");
  }
  if (version < 66)
  {
    // Add a new columns strReleaseGroupMBID, bScrapedMBID for albums
    m_pDS->exec("ALTER TABLE album ADD bScrapedMBID INTEGER NOT NULL DEFAULT 0\n");
    m_pDS->exec("ALTER TABLE album ADD strReleaseGroupMBID TEXT \n");
    // Add a new column bScrapedMBID for artists
    m_pDS->exec("ALTER TABLE artist ADD bScrapedMBID INTEGER NOT NULL DEFAULT 0\n");
  }
  if (version < 67)
  {
    // Add infosetting table
    m_pDS->exec("CREATE TABLE infosetting (idSetting INTEGER PRIMARY KEY, strScraperPath TEXT, "
                "strSettings TEXT)");
    // Add a new column for setting to album and artist tables
    m_pDS->exec("ALTER TABLE artist ADD idInfoSetting INTEGER NOT NULL DEFAULT 0\n");
    m_pDS->exec("ALTER TABLE album ADD idInfoSetting INTEGER NOT NULL DEFAULT 0\n");

    // Attempt to get album and artist specific scraper settings from the content table, extracting ids from path
    m_pDS->exec(
        "CREATE TABLE content_temp(id INTEGER PRIMARY KEY, idItem INTEGER, strContent text, "
        "strScraperPath text, strSettings text)");
    try
    {
      m_pDS->exec("INSERT INTO content_temp(idItem, strContent, strScraperPath, strSettings) "
                  "SELECT SUBSTR(strPath, 19, LENGTH(strPath) - 19) + 0 AS idItem, strContent, "
                  "strScraperPath, strSettings "
                  "FROM content WHERE strContent = 'artists' AND strPath LIKE "
                  "'musicdb://artists/_%/' ORDER BY idItem");
    }
    catch (...)
    {
      CLog::Log(LOGERROR,
                "Migrating specific artist scraper settings has failed, settings not transferred");
    }
    try
    {
      m_pDS->exec("INSERT INTO content_temp (idItem, strContent, strScraperPath, strSettings ) "
                  "SELECT SUBSTR(strPath, 18, LENGTH(strPath) - 18) + 0 AS idItem, strContent, "
                  "strScraperPath, strSettings "
                  "FROM content WHERE strContent = 'albums' AND strPath LIKE "
                  "'musicdb://albums/_%/' ORDER BY idItem");
    }
    catch (...)
    {
      CLog::Log(LOGERROR,
                "Migrating specific album scraper settings has failed, settings not transferred");
    }
    try
    {
      m_pDS->exec("INSERT INTO infosetting(idSetting, strScraperPath, strSettings) "
                  "SELECT id, strScraperPath, strSettings FROM content_temp");
      m_pDS->exec(
          "UPDATE artist SET idInfoSetting = "
          "(SELECT id FROM content_temp WHERE strContent = 'artists' AND idItem = idArtist) "
          "WHERE EXISTS(SELECT 1 FROM content_temp WHERE strContent = 'artists' AND idItem = "
          "idArtist) ");
      m_pDS->exec("UPDATE album SET idInfoSetting = "
                  "(SELECT id FROM content_temp WHERE strContent = 'albums' AND idItem = idAlbum) "
                  "WHERE EXISTS(SELECT 1 FROM content_temp WHERE strContent = 'albums' AND idItem "
                  "= idAlbum) ");
    }
    catch (...)
    {
      CLog::Log(LOGERROR,
                "Migrating album and artist scraper settings has failed, settings not transferred");
    }
    m_pDS->exec("DROP TABLE content_temp");

    // Remove content table
    m_pDS->exec("DROP TABLE content");
    // Remove albuminfosong table
    m_pDS->exec("DROP TABLE albuminfosong");
  }
  if (version < 68)
  {
    // Add a new columns strType, strGender, strDisambiguation for artists
    m_pDS->exec("ALTER TABLE artist ADD strType TEXT \n");
    m_pDS->exec("ALTER TABLE artist ADD strGender TEXT \n");
    m_pDS->exec("ALTER TABLE artist ADD strDisambiguation TEXT \n");
  }
  if (version < 69)
  {
    // Remove album_genre table
    m_pDS->exec("DROP TABLE album_genre");
  }
  if (version < 70)
  {
    // Update all songs iStartOffset and iEndOffset to milliseconds instead of frames (* 1000 / 75)
    m_pDS->exec("UPDATE song SET iStartOffset = iStartOffset * 40 / 3, iEndOffset = iEndOffset * "
                "40 / 3 \n");
  }
  if (version < 71)
  {
    // Add lastscanned to versiontagscan table
    m_pDS->exec("ALTER TABLE versiontagscan ADD lastscanned VARCHAR(20)\n");
    CDateTime dateAdded = CDateTime::GetCurrentDateTime();
    m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET lastscanned = '%s'",
                           dateAdded.GetAsDBDateTime().c_str()));
  }
  if (version < 72)
  {
    // Create source table
    m_pDS->exec(
        "CREATE TABLE source (idSource INTEGER PRIMARY KEY, strName TEXT, strMultipath TEXT)");
    // Create source_path table
    m_pDS->exec(
        "CREATE TABLE source_path (idSource INTEGER, idPath INTEGER, strPath varchar(512))");
    // Create album_source table
    m_pDS->exec("CREATE TABLE album_source (idSource INTEGER, idAlbum INTEGER)");
    // Populate source and source_path tables from sources.xml
    // Filling album_source needs to be done after indexes are created or it is
    // very slow. It could be populated during CreateAnalytics but it is checked
    // and filled as part of scanning anyway so simply force full rescan.
    MigrateSources();
  }
  if (version < 73)
  {
    // add bBoxedSet to album table
    m_pDS->exec("ALTER TABLE album ADD bBoxedSet INTEGER NOT NULL DEFAULT 0 \n");
    //  add iDiscTotal to album table
    m_pDS->exec("ALTER TABLE album ADD iDiscTotal INTEGER NOT NULL DEFAULT 0 \n");
    // populate iDiscTotal from the data already in the song table
    m_pDS->exec("UPDATE album SET iDisctotal = (SELECT COUNT(DISTINCT (iTrack >> 16)) "
                "FROM song WHERE song.idAlbum = album.idAlbum GROUP BY idAlbum ) "
                "WHERE EXISTS (SELECT 1 FROM song WHERE song.idAlbum = album.idAlbum)");
    // add strDiscSubtitles to song table
    m_pDS->exec("ALTER TABLE song ADD strDiscSubtitle TEXT \n");
  }
  if (version < 74)
  {
    //Remove iYear, add stReleaseDate and strOrigReleaseDate columns to album table
    m_pDS->exec("CREATE TABLE album_new (idAlbum INTEGER PRIMARY KEY, "
                "strAlbum VARCHAR(256), strMusicBrainzAlbumID TEXT, "
                "strReleaseGroupMBID TEXT, "
                "strArtistDisp TEXT, strArtistSort TEXT, strGenres TEXT, "
                "strReleaseDate TEXT, strOrigReleaseDate TEXT, "
                "bBoxedSet INTEGER NOT NULL DEFAULT 0, "
                "bCompilation INTEGER NOT NULL DEFAULT '0', "
                "strMoods TEXT, strStyles TEXT, strThemes TEXT, "
                "strReview TEXT, strImage TEXT, strLabel TEXT, "
                "strType TEXT, "
                "fRating FLOAT NOT NULL DEFAULT 0, "
                "iVotes INTEGER NOT NULL DEFAULT 0, "
                "iUserrating INTEGER NOT NULL DEFAULT 0, "
                "lastScraped VARCHAR(20) DEFAULT NULL, "
                "bScrapedMBID INTEGER NOT NULL DEFAULT 0, "
                "strReleaseType TEXT, "
                "iDiscTotal INTEGER NOT NULL DEFAULT 0, "
                "idInfoSetting INTEGER NOT NULL DEFAULT 0)");
    // Prepare as MySQL has different CAST datatypes
    m_pDS->exec(
        PrepareSQL("INSERT INTO album_new "
                   "(idalbum, strAlbum, "
                   "strMusicBrainzAlbumID, strReleaseGroupMBID, "
                   "strArtistDisp, strArtistSort, strGenres, "
                   "strReleaseDate, strOrigReleaseDate, "
                   "bBoxedSet, bCompilation, strMoods, strStyles, strThemes, "
                   "strReview, strImage, strLabel, strType, "
                   "fRating, iVotes, iUserrating, "
                   "lastScraped, bScrapedMBID, strReleaseType, "
                   "iDiscTotal, idInfoSetting) "
                   "SELECT "
                   "idAlbum, strAlbum, "
                   "strMusicBrainzAlbumID, strReleaseGroupMBID, "
                   "strArtistDisp, strArtistSort, strGenres, "
                   "CASE WHEN iYear > 0 THEN CAST(iYear AS TEXT) ELSE NULL END, "
                   "CASE WHEN iYear > 0 THEN CAST(iYear AS TEXT) ELSE NULL END, "
                   // bBoxedSet could be null if v72 not rescanned and that is invalid, tidy up now
                   "CASE WHEN bBoxedSet IS NULL THEN 0 ELSE bBoxedSet END, "
                   "bCompilation, strMoods, strStyles, strThemes, "
                   "strReview, strImage, strLabel, strType, "
                   "fRating, iVotes, iUserrating, "
                   "lastScraped, bScrapedMBID, strReleaseType, "
                   "iDiscTotal, idInfoSetting "
                   "FROM album"));
    m_pDS->exec("DROP TABLE album");
    m_pDS->exec("ALTER TABLE album_new RENAME TO album");

    //Remove iYear and add stReleaseDate, strOrigReleaseDate and iBPM columns to song table
    m_pDS->exec("CREATE TABLE song_new (idSong INTEGER PRIMARY KEY, "
                "idAlbum INTEGER, idPath INTEGER, "
                "strArtistDisp TEXT, strArtistSort TEXT, strGenres TEXT, strTitle VARCHAR(512), "
                "iTrack INTEGER, iDuration INTEGER, "
                "strReleaseDate TEXT, strOrigReleaseDate TEXT, "
                "strDiscSubtitle TEXT, strFileName TEXT, strMusicBrainzTrackID TEXT, "
                "iTimesPlayed INTEGER, iStartOffset INTEGER, iEndOffset INTEGER, "
                "lastplayed VARCHAR(20) DEFAULT NULL, "
                "rating FLOAT NOT NULL DEFAULT 0, votes INTEGER NOT NULL DEFAULT 0, "
                "userrating INTEGER NOT NULL DEFAULT 0, "
                "comment TEXT, mood TEXT, iBPM INTEGER NOT NULL DEFAULT 0, strReplayGain TEXT, "
                "dateAdded TEXT)");
    // Prepare as MySQL has different CAST datatypes
    m_pDS->exec(PrepareSQL("INSERT INTO song_new "
                           "(idSong, "
                           "idAlbum, idPath, "
                           "strArtistDisp, strArtistSort, strGenres, strTitle, "
                           "iTrack, iDuration, "
                           "strReleaseDate, strOrigReleaseDate, "
                           "strDiscSubtitle, strFileName, strMusicBrainzTrackID, "
                           "iTimesPlayed, iStartOffset, iEndOffset, "
                           "lastplayed, "
                           "rating, userrating, votes, "
                           "comment, mood, strReplayGain, dateAdded) "
                           "SELECT "
                           "idSong, "
                           "idAlbum, idPath, "
                           "strArtistDisp, strArtistSort, strGenres, strTitle, "
                           "iTrack, iDuration, "
                           "CASE WHEN iYear > 0 THEN CAST(iYear AS TEXT) ELSE NULL END, "
                           "CASE WHEN iYear > 0 THEN CAST(iYear AS TEXT) ELSE NULL END, "
                           "strDiscSubtitle, strFileName, strMusicBrainzTrackID, "
                           "iTimesPlayed, iStartOffset, iEndOffset, "
                           "lastplayed, "
                           "rating, userrating, votes, "
                           "comment, mood, strReplayGain, dateAdded "
                           "FROM song"));
    m_pDS->exec("DROP TABLE song");
    m_pDS->exec("ALTER TABLE song_new RENAME TO song");
  }
  if (version < 75)
  {
    m_pDS->exec("ALTER TABLE song ADD iBitRate INTEGER NOT NULL DEFAULT 0");
    m_pDS->exec("ALTER TABLE song ADD iSampleRate INTEGER NOT NULL DEFAULT 0");
    m_pDS->exec("ALTER TABLE song ADD iChannels INTEGER NOT NULL DEFAULT 0");
  }
  if (version < 77)
  {
    m_pDS->exec("ALTER TABLE album ADD strReleaseStatus TEXT");
  }
  if (version < 78)
  {
    std::string strUTCNow = CDateTime::GetUTCDateTime().GetAsDBDateTime();

    // Add removed_link table
    m_pDS->exec("CREATE TABLE removed_link(idArtist INTEGER, idMedia INTEGER, idRole INTEGER)");
    // Add lastcleaned and artistlinksupdated to versiontagscan table
    m_pDS->exec("ALTER TABLE versiontagscan ADD lastcleaned VARCHAR(20)");
    m_pDS->exec("ALTER TABLE versiontagscan ADD artistlinksupdated VARCHAR(20)");
    m_pDS->exec("ALTER TABLE versiontagscan ADD genresupdated VARCHAR(20)");
    // Adjust lastscanned if original local time value is after current UTC
    if (GetLibraryLastUpdated() > strUTCNow)
      SetLibraryLastUpdated();
    m_pDS->exec("UPDATE versiontagscan SET lastcleaned = lastscanned, "
                "genresupdated = lastscanned, "
                "artistlinksupdated = lastscanned");

    // Add dateNew, dateModified to song table
    m_pDS->exec("ALTER TABLE song ADD dateNew TEXT");
    m_pDS->exec("ALTER TABLE song ADD dateModified TEXT");
    // Set new to dateAdded and modified to lastplayed as estimates
    // Limit those local time values to now UTC, and modified is after new
    m_pDS->exec("UPDATE song SET dateNew = dateAdded, dateModified = lastplayed");
    m_pDS->exec(PrepareSQL("UPDATE song SET dateNew = '%s' WHERE dateNew > '%s'", strUTCNow.c_str(),
                           strUTCNow.c_str()));
    m_pDS->exec("UPDATE song SET dateModified = dateNew WHERE dateModified IS NULL");
    m_pDS->exec(PrepareSQL("UPDATE song SET dateModified = '%s' WHERE dateModified > '%s'",
                           strUTCNow.c_str(), strUTCNow.c_str()));
    m_pDS->exec("UPDATE song SET dateAdded = dateModified WHERE dateAdded > dateModified");

    // Add dateAdded, dateNew, dateModified to album table
    m_pDS->exec("ALTER TABLE album ADD dateAdded TEXT");
    m_pDS->exec("ALTER TABLE album ADD dateNew TEXT");
    m_pDS->exec("ALTER TABLE album ADD dateModified TEXT");
    // Set dateAdded and new values from song dates, and modified to lastscraped as estimates
    // Limit modified value to now UTC and after new
    // Indices have been dropped making subquery very slow, so create temp index
    m_pDS->exec("CREATE INDEX idxSong3 ON song(idAlbum)");
    m_pDS->exec("UPDATE album SET dateAdded = "
                "(SELECT MAX(song.dateAdded) FROM song WHERE song.idAlbum = album.idAlbum)");
    m_pDS->exec("UPDATE album SET dateNew = "
                "(SELECT MIN(song.dateNew) FROM song WHERE song.idAlbum = album.idAlbum)");
    m_pDS->exec("UPDATE album SET dateModified = dateNew");
    m_pDS->exec("UPDATE album SET dateModified = lastscraped WHERE lastscraped > dateModified");
    m_pDS->exec(PrepareSQL("UPDATE album SET dateModified = '%s' WHERE dateModified > '%s'",
                           strUTCNow.c_str(), strUTCNow.c_str()));
    //Remove temp index, full analytics for database created later
    m_pDS->exec("DROP INDEX idxSong3 ON song");

    // Add dateAdded, dateNew, dateModified to artist table
    m_pDS->exec("ALTER TABLE artist ADD dateAdded TEXT");
    m_pDS->exec("ALTER TABLE artist ADD dateNew TEXT");
    m_pDS->exec("ALTER TABLE artist ADD dateModified TEXT");
    // dateAdded has NULL values until files rescanned by user
    // Set new and modified to now UTC as not worth complexity of estimating from song dates
    m_pDS->exec(PrepareSQL("UPDATE artist SET dateNew = '%s'", strUTCNow.c_str()));
    m_pDS->exec("UPDATE artist SET dateModified = dateNew");
  }
  if (version < 79)
  {
    m_pDS->exec("ALTER TABLE discography ADD strReleaseGroupMBID TEXT");
  }
  if (version < 80)
  {
    m_pDS->exec("ALTER TABLE album ADD iAlbumDuration INTEGER NOT NULL DEFAULT 0");
    // update duration for all current albums
    m_pDS->exec("UPDATE album SET iAlbumDuration = (SELECT SUM(song.iDuration) FROM song "
                "WHERE song.idAlbum = album.idAlbum) "
                "WHERE EXISTS (SELECT 1 FROM song WHERE song.idAlbum = album.idAlbum)");
  }
  if (version < 82)
  {
    // Update artist table combining fanart URL data into strImage field
    // Clear empty URL data <fanart /> and <thumb />
    m_pDS->exec("UPDATE artist SET strFanart = '' WHERE strFanart = '<fanart />'");
    m_pDS->exec("UPDATE artist SET strImage = '' WHERE strImage = '<thumb />'");
    //Prepare strFanart - strip <fanart>...</fanart>, add aspect to the URLs
    m_pDS->exec("UPDATE artist SET strFanart = REPLACE(strFanart, '<fanart>', '')");
    m_pDS->exec("UPDATE artist SET strFanart = REPLACE(strFanart, '</fanart>', '')");
    m_pDS->exec("UPDATE artist SET strFanart = REPLACE(strFanart, 'thumb preview', 'thumb "
                "aspect=\"fanart\" preview')");
    // Art URLs limited on MySQL databases to 65535 characters (TEXT field)
    // Truncate the fanart when total URLs exceeds this
    bool bisMySQL = StringUtils::EqualsNoCase(
        CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseMusic.type,
        "mysql");
    if (bisMySQL)
    {
      std::string strSQL = "SELECT idArtist, strFanart, strImage FROM artist "
                           "WHERE LENGTH(strImage) + LENGTH(strFanart) > 65535";
      if (m_pDS->query(strSQL))
      {
        while (!m_pDS->eof())
        {
          int idArtist = m_pDS->fv("idArtist").get_asInt();
          std::string strFanart = m_pDS->fv("strFanart").get_asString();
          std::string strImage = m_pDS->fv("strImage").get_asString();
          size_t space = 65535;
          // Trim strImage to allow arbitrary half space for fanart
          if (!TrimImageURLs(strImage, space / 2))
            strImage.clear(); // </thumb> not found, empty field
          space = space - strImage.length();
          // Trim fanart to fit remaining space
          if (!TrimImageURLs(strFanart, space))
            strFanart.clear(); // </thumb> not found, empty field

          strSQL = PrepareSQL("UPDATE artist SET strFanart = '%s', strImage = '%s' "
                              "WHERE idArtist = %i",
                              strFanart.c_str(), strImage.c_str(), idArtist);
          m_pDS2->exec(strSQL); // Use other dataset to update while looping result set

          m_pDS->next();
        }
        m_pDS->close();
      }
    }

    // Remove strFanart column from artist table
    m_pDS->exec("CREATE TABLE artist_new (idArtist INTEGER PRIMARY KEY, "
                "strArtist varchar(256), strMusicBrainzArtistID text, "
                "strSortName text, "
                "strType text, strGender text, strDisambiguation text, "
                "strBorn text, strFormed text, strGenres text, strMoods text, "
                "strStyles text, strInstruments text, strBiography text, "
                "strDied text, strDisbanded text, strYearsActive text, "
                "strImage text, "
                "lastScraped varchar(20) default NULL, "
                "bScrapedMBID INTEGER NOT NULL DEFAULT 0, "
                "idInfoSetting INTEGER NOT NULL DEFAULT 0, "
                "dateAdded TEXT, dateNew TEXT, dateModified TEXT)");
    // Concatenate fanart URLs into strImage field
    // Prepare SQL to convert CONCAT to || in SQLite
    m_pDS->exec(PrepareSQL("INSERT INTO artist_new "
                           "(idArtist, strArtist, strMusicBrainzArtistID, "
                           "strSortName, strType, strGender, strDisambiguation, "
                           "strBorn, strFormed, strGenres, strMoods, "
                           "strStyles , strInstruments , strBiography , "
                           "strDied, strDisbanded, strYearsActive, "
                           "strImage, "
                           "lastScraped, bScrapedMBID, idInfoSetting, "
                           "dateAdded, dateNew, dateModified) "
                           "SELECT "
                           "artist.idArtist, "
                           "strArtist, strMusicBrainzArtistID, "
                           "strSortName, strType, strGender, strDisambiguation, "
                           "strBorn, strFormed, strGenres, strMoods, "
                           "strStyles, strInstruments, strBiography, "
                           "strDied, strDisbanded, strYearsActive, "
                           "CONCAT(strImage, strFanart), "
                           "lastScraped, bScrapedMBID, idInfoSetting, "
                           "dateAdded, dateNew, dateModified "
                           "FROM artist"));
    m_pDS->exec("DROP TABLE artist");
    m_pDS->exec("ALTER TABLE artist_new RENAME TO artist");
  }

  if (version < 83)
    m_pDS->exec("ALTER TABLE song ADD strVideoURL TEXT");

  // Set the version of tag scanning required.
  // Not every schema change requires the tags to be rescanned, set to the highest schema version
  // that needs this. Forced rescanning (of music files that have not changed since they were
  // previously scanned) also accommodates any changes to the way tags are processed
  // e.g. read tags that were not processed by previous versions.
  // The original db version when the tags were scanned, and the minimal db version needed are
  // later used to determine if a forced rescan should be prompted

  // The last schema change needing forced rescanning was 73.
  // This is because Kodi can now read and process extra tags involved in the creation of box sets

  SetMusicNeedsTagScan(73);

  // After all updates, store the original db version.
  // This indicates the version of tag processing that was used to populate db
  SetMusicTagScanVersion(version);
}

int CMusicDatabase::GetSchemaVersion() const
{
  return 84;
}

int CMusicDatabase::GetMusicNeedsTagScan()
{
  try
  {
    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    std::string sql = "SELECT * FROM versiontagscan";
    if (!m_pDS->query(sql))
      return -1;

    if (m_pDS->num_rows() != 1)
    {
      m_pDS->close();
      return -1;
    }

    int idVersion = m_pDS->fv("idVersion").get_asInt();
    int iNeedsScan = m_pDS->fv("iNeedsScan").get_asInt();
    m_pDS->close();
    if (idVersion < iNeedsScan)
      return idVersion;
    else
      return 0;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

void CMusicDatabase::SetMusicNeedsTagScan(int version)
{
  m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET iNeedsScan=%i", version));
}

void CMusicDatabase::SetMusicTagScanVersion(int version /* = 0 */)
{
  if (version == 0)
    m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET idVersion=%i", GetSchemaVersion()));
  else
    m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET idVersion=%i", version));
}

std::string CMusicDatabase::GetLibraryLastUpdated() const
{
  return GetSingleValue("SELECT lastscanned FROM versiontagscan LIMIT 1");
}

void CMusicDatabase::SetLibraryLastUpdated()
{
  CDateTime dateUpdated = CDateTime::GetUTCDateTime();
  m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET lastscanned = '%s'",
                         dateUpdated.GetAsDBDateTime().c_str()));
}

std::string CMusicDatabase::GetLibraryLastCleaned() const
{
  return GetSingleValue("SELECT lastcleaned FROM versiontagscan LIMIT 1");
}

void CMusicDatabase::SetLibraryLastCleaned()
{
  std::string strUpdated = CDateTime::GetUTCDateTime().GetAsDBDateTime();
  m_pDS->exec(PrepareSQL("UPDATE versiontagscan SET lastcleaned  = '%s'", strUpdated.c_str()));
}

std::string CMusicDatabase::GetArtistLinksUpdated() const
{
  return GetSingleValue("SELECT artistlinksupdated FROM versiontagscan LIMIT 1");
}

void CMusicDatabase::SetArtistLinksUpdated()
{
  std::string strUpdated = CDateTime::GetUTCDateTime().GetAsDBDateTime();
  m_pDS->exec(
      PrepareSQL("UPDATE versiontagscan SET artistlinksupdated = '%s'", strUpdated.c_str()));
}

std::string CMusicDatabase::GetGenresLastAdded() const
{
  return GetSingleValue("SELECT genresupdated FROM versiontagscan LIMIT 1");
}

std::string CMusicDatabase::GetSongsLastAdded() const
{
  return GetSingleValue("SELECT MAX(dateNew) FROM song");
}

std::string CMusicDatabase::GetAlbumsLastAdded() const
{
  return GetSingleValue("SELECT MAX(dateNew) FROM album");
}

std::string CMusicDatabase::GetArtistsLastAdded() const
{
  return GetSingleValue("SELECT MAX(dateNew) FROM artist");
}

std::string CMusicDatabase::GetSongsLastModified() const
{
  return GetSingleValue("SELECT MAX(dateModified) FROM song");
}

std::string CMusicDatabase::GetAlbumsLastModified() const
{
  return GetSingleValue("SELECT MAX(dateModified) FROM album");
}

std::string CMusicDatabase::GetArtistsLastModified() const
{
  return GetSingleValue("SELECT MAX(dateModified) FROM artist");
}

unsigned int CMusicDatabase::GetRandomSongIDs(const Filter& filter,
                                              std::vector<std::pair<int, int>>& songIDs)
{
  try
  {
    if (nullptr == m_pDB)
      return 0;
    if (nullptr == m_pDS)
      return 0;

    std::string strSQL = "SELECT idSong FROM songview ";
    if (!CDatabase::BuildSQL(strSQL, filter, strSQL))
      return false;
    strSQL += PrepareSQL(" ORDER BY RANDOM()");

    if (!m_pDS->query(strSQL))
      return 0;
    songIDs.clear();
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      return 0;
    }
    songIDs.reserve(m_pDS->num_rows());
    while (!m_pDS->eof())
    {
      songIDs.push_back(std::make_pair<int, int>(1, m_pDS->fv(song_idSong).get_asInt()));
      m_pDS->next();
    } // cleanup
    m_pDS->close();
    return static_cast<unsigned int>(songIDs.size());
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", filter.where);
  }
  return 0;
}

int CMusicDatabase::GetSongsCount(const Filter& filter)
{
  return m_navRepo.GetSongsCount(filter);
}


bool CMusicDatabase::GetAlbumPath(int idAlbum, std::string& basePath)
{
  basePath.clear();
  std::vector<std::pair<std::string, int>> paths;
  if (!GetAlbumPaths(idAlbum, paths))
    return false;

  for (const auto& [songPath, _] : paths)
  {
    if (basePath.empty())
      basePath = songPath;
    else
      URIUtils::GetCommonPath(basePath, songPath);
  }
  return true;
}

bool CMusicDatabase::GetAlbumPaths(int idAlbum, std::vector<std::pair<std::string, int>>& paths)
{
  paths.clear();
  std::string strSQL;
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS2)
      return false;

    // Get the unique paths of songs on the album, providing there are no songs from
    // other albums with the same path. This returns
    // a) <album> if is contains all the songs and no others, or
    // b) <album>/cd1, <album>/cd2 etc. for disc sets
    // but does *not* return any path when albums are mixed together. That could be because of
    // deliberate file organisation, or (more likely) because of a tagging error in album name
    // or Musicbrainzalbumid. Thus it avoids finding some generic music path.
    strSQL = PrepareSQL("SELECT DISTINCT strPath, song.idPath FROM song "
                        "JOIN path ON song.idPath = path.idPath "
                        "WHERE song.idAlbum = %ld "
                        "AND (SELECT COUNT(DISTINCT(idAlbum)) FROM song AS song2 "
                        "WHERE idPath = song.idPath) = 1",
                        idAlbum);

    if (!m_pDS2->query(strSQL))
      return false;
    if (m_pDS2->num_rows() == 0)
    {
      // Album does not have a unique path, files are mixed
      m_pDS2->close();
      return false;
    }

    while (!m_pDS2->eof())
    {
      paths.emplace_back(m_pDS2->fv("strPath").get_asString(),
                         m_pDS2->fv("song.idPath").get_asInt());
      m_pDS2->next();
    }
    // Cleanup recordset data
    m_pDS2->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed to execute {}", strSQL);
  }

  return false;
}

int CMusicDatabase::GetDiscnumberForPathID(int idPath)
{
  std::string strSQL;
  int result = -1;
  try
  {
    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS2)
      return -1;

    strSQL = PrepareSQL("SELECT DISTINCT(song.iTrack >> 16) AS discnum FROM song "
                        "WHERE idPath = %i",
                        idPath);

    if (!m_pDS2->query(strSQL))
      return -1;
    if (m_pDS2->num_rows() == 1)
    { // Songs with this path have a unique disc number
      result = m_pDS2->fv("discnum").get_asInt();
    }
    // Cleanup recordset data
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed to execute {}", strSQL);
  }
  return result;
}

// Get old "artist path" - where artist.nfo and art was located v17 and below.
// It is the path common to all albums by an (album) artist, but ensure it is unique
// to that artist and not shared with other artists. Previously this caused incorrect nfo
// and art to be applied to multiple artists.
bool CMusicDatabase::GetOldArtistPath(int idArtist, std::string& basePath)
{
  basePath.clear();
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS2)
      return false;

    // find all albums from this artist, and all the paths to the songs from those albums
    std::string strSQL = PrepareSQL("SELECT strPath FROM album_artist "
                                    "JOIN song ON album_artist.idAlbum = song.idAlbum "
                                    "JOIN path ON song.idPath = path.idPath "
                                    "WHERE album_artist.idArtist = %ld "
                                    "GROUP BY song.idPath",
                                    idArtist);

    // run query
    if (!m_pDS2->query(strSQL))
      return false;
    int iRowsFound = m_pDS2->num_rows();
    if (iRowsFound == 0)
    {
      // Artist is not an album artist, no path to find
      m_pDS2->close();
      return false;
    }
    else if (iRowsFound == 1)
    {
      // Special case for single path - assume that we're in an artist/album/songs filesystem
      URIUtils::GetParentPath(m_pDS2->fv("strPath").get_asString(), basePath);
      m_pDS2->close();
    }
    else
    {
      // find the common path (if any) to these albums
      while (!m_pDS2->eof())
      {
        std::string path = m_pDS2->fv("strPath").get_asString();
        if (basePath.empty())
          basePath = path;
        else
          URIUtils::GetCommonPath(basePath, path);

        m_pDS2->next();
      }
      m_pDS2->close();
    }

    // Check any path found is unique to that album artist, and do *not* return any path
    // that is shared with other album artists. That could be because of collaborations
    // i.e. albums with more than one album artist, or because there are albums by the
    // artist on multiple music sources, or elsewhere in the folder hierarchy.
    // Avoid returning some generic music path.
    if (!basePath.empty())
    {
      strSQL = PrepareSQL("SELECT COUNT(album_artist.idArtist) FROM album_artist "
                          "JOIN song ON album_artist.idAlbum = song.idAlbum "
                          "JOIN path ON song.idPath = path.idPath "
                          "WHERE album_artist.idArtist <> %ld "
                          "AND strPath LIKE '%s%%'",
                          idArtist, basePath.c_str());
      const std::string strValue = GetSingleValue(strSQL, *m_pDS2);
      if (!strValue.empty())
      {
        const auto countartists = static_cast<int>(std::strtol(strValue.c_str(), nullptr, 10));
        if (countartists == 0)
          return true;
      }
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  basePath.clear();
  return false;
}

bool CMusicDatabase::GetArtistPath(const CArtist& artist, std::string& path)
{
  // Get path for artist in the artists folder
  path = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
      CSettings::SETTING_MUSICLIBRARY_ARTISTSFOLDER);
  if (path.empty())
    return false; // No Artists folder not set;
  // Get unique artist folder name
  std::string strFolder;
  if (GetArtistFolderName(artist, strFolder))
  {
    path = URIUtils::AddFileToFolder(path, strFolder);
    return true;
  }
  path.clear();
  return false;
}

bool CMusicDatabase::GetAlbumFolder(const CAlbum& album,
                                    const std::string& strAlbumPath,
                                    std::string& strFolder)
{
  if (!m_pDS2)
    return false;

  strFolder.clear();
  // Get a name for the album folder that is unique for the artist to use when
  // exporting albums to separate nfo files in a folder under an artist folder

  // When given an album path (common to all the music files containing *only*
  // that album) check if that folder name is *unique* looking at folders on
  // all levels of the music file paths for the artist
  if (!strAlbumPath.empty())
  {
    // Get last folder from full path
    std::vector<std::string> folders = URIUtils::SplitPath(strAlbumPath);
    if (!folders.empty())
    {
      strFolder = folders.back();
      // The same folder name could be used on different paths for albums by the
      // same first artist. The albums could be totally different or also have
      // the same name (but different mbid). Be over cautious and look for the
      // name any where in the music file paths
      std::string strSQL = PrepareSQL("SELECT DISTINCT album_artist.idAlbum FROM album_artist "
                                      "JOIN song ON album_artist.idAlbum = song.idAlbum "
                                      "JOIN path on path.idPath = song.idPath "
                                      "WHERE album_artist.iOrder = 0 "
                                      "AND album_artist.idArtist = %ld "
                                      "AND path.strPath LIKE '%%\\%s\\%%'",
                                      album.artistCredits[0].GetArtistId(), strFolder.c_str());

      if (!m_pDS2->query(strSQL))
        return false;
      int iRowsFound = m_pDS2->num_rows();
      m_pDS2->close();
      if (iRowsFound == 1)
        return true;
    }
  }
  // Create a valid unique folder name from album title
  // @todo: Does UFT8 matter or need normalizing?
  // @todo: Simplify punctuation removing unicode appostraphes, "..." etc.?
  strFolder = CUtil::MakeLegalFileName(album.strAlbum, LegalPath::WIN32_COMPAT);
  StringUtils::Replace(strFolder, " _ ", "_");

  // Check <first albumartist name>/<albumname> is unique e.g. 2 x Bruckner Symphony No. 3
  // To have duplicate albumartist/album names at least one will have mbid, so append start of mbid to folder.
  // This will not handle names that only differ by reserved chars e.g. "a>album" and "a?name"
  // will be unique in db, but produce same folder name "a_name", but that kind of album and artist naming is very unlikely
  std::string strSQL = PrepareSQL("SELECT COUNT(album_artist.idAlbum) FROM album_artist "
                                  "JOIN album ON album_artist.idAlbum = album.idAlbum "
                                  "WHERE album_artist.iOrder = 0 "
                                  "AND album_artist.idArtist = %ld "
                                  "AND album.strAlbum LIKE '%s'  ",
                                  album.artistCredits[0].GetArtistId(), album.strAlbum.c_str());
  const std::string strValue = GetSingleValue(strSQL, *m_pDS2);
  if (strValue.empty())
    return false;
  const auto countalbum = static_cast<int>(std::strtol(strValue.c_str(), nullptr, 10));
  if (countalbum > 1 && !album.strMusicBrainzAlbumID.empty())
  { // Only one of the duplicate albums can be without mbid
    strFolder += "_" + album.strMusicBrainzAlbumID.substr(0, 4);
  }
  return !strFolder.empty();
}

bool CMusicDatabase::GetArtistFolderName(const CArtist& artist, std::string& strFolder)
{
  return GetArtistFolderName(artist.strArtist, artist.strMusicBrainzArtistID, strFolder);
}

bool CMusicDatabase::GetArtistFolderName(const std::string& strArtist,
                                         const std::string& strMusicBrainzArtistID,
                                         std::string& strFolder)
{
  if (!m_pDS2)
    return false;

  // Create a valid unique folder name for artist
  // @todo: Does UFT8 matter or need normalizing?
  // @todo: Simplify punctuation removing unicode appostraphes, "..." etc.?
  strFolder = CUtil::MakeLegalFileName(strArtist, LegalPath::WIN32_COMPAT);
  StringUtils::Replace(strFolder, " _ ", "_");

  // Ensure <artist name> is unique e.g. 2 x John Williams.
  // To have duplicate artist names there must both have mbids, so append start of mbid to folder.
  // This will not handle names that only differ by reserved chars e.g. "a>name" and "a?name"
  // will be unique in db, but produce same folder name "a_name", but that kind of artist naming is very unlikely
  std::string strSQL =
      PrepareSQL("SELECT COUNT(1) FROM artist WHERE strArtist LIKE '%s'", strArtist.c_str());
  const std::string strValue = GetSingleValue(strSQL, *m_pDS2);
  if (strValue.empty())
    return false;
  const auto countartist = static_cast<int>(std::strtol(strValue.c_str(), nullptr, 10));
  if (countartist > 1)
    strFolder += "_" + strMusicBrainzArtistID.substr(0, 4);
  return !strFolder.empty();
}

int CMusicDatabase::AddSource(const std::string& strName,
                              const std::string& strMultipath,
                              const std::vector<std::string>& vecPaths,
                              int id /*= -1*/)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    // Check if source name already exists
    int idSource = GetSourceByName(strName);
    if (idSource < 0)
    {
      BeginTransaction();
      // Add new source and source paths
      if (id > 0)
        strSQL = PrepareSQL("INSERT INTO source (idSource, strName, strMultipath) "
                            "VALUES(%i, '%s', '%s')",
                            id, strName.c_str(), strMultipath.c_str());
      else
        strSQL = PrepareSQL("INSERT INTO source (idSource, strName, strMultipath) "
                            "VALUES(NULL, '%s', '%s')",
                            strName.c_str(), strMultipath.c_str());
      m_pDS->exec(strSQL);

      idSource = static_cast<int>(m_pDS->lastinsertid());

      int idPath = 1;
      for (const auto& path : vecPaths)
      {
        strSQL = PrepareSQL("INSERT INTO source_path (idSource, idPath, strPath) "
                            "VALUES(%i,%i,'%s')",
                            idSource, idPath, path.c_str());
        m_pDS->exec(strSQL);
        ++idPath;
      }

      // Find albums by song path, building WHERE for multiple source paths
      // (providing source has a path)
      if (!vecPaths.empty())
      {
        std::vector<int> albumIds;
        Filter extFilter;
        strSQL = "SELECT DISTINCT idAlbum FROM song ";
        extFilter.AppendJoin("JOIN path ON song.idPath = path.idPath");
        for (const auto& path : vecPaths)
          extFilter.AppendWhere(PrepareSQL("path.strPath LIKE '%s%%%%'", path.c_str()), false);
        if (!BuildSQL(strSQL, extFilter, strSQL))
          return -1;

        if (!m_pDS->query(strSQL))
          return -1;

        while (!m_pDS->eof())
        {
          albumIds.push_back(m_pDS->fv("idAlbum").get_asInt());
          m_pDS->next();
        }
        m_pDS->close();

        // Add album_source for related albums
        for (auto idAlbum : albumIds)
        {
          strSQL = PrepareSQL("INSERT INTO album_source (idSource, idAlbum) "
                              "VALUES('%i', '%i')",
                              idSource, idAlbum);
          m_pDS->exec(strSQL);
        }
      }
      CommitTransaction();
    }
    return idSource;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed with query ({})", strSQL);
    RollbackTransaction();
  }

  return -1;
}

int CMusicDatabase::UpdateSource(const std::string& strOldName,
                                 const std::string& strName,
                                 const std::string& strMultipath,
                                 const std::vector<std::string>& vecPaths)
{
  int idSource = -1;
  std::string strSourceMultipath;
  std::string strSQL;
  try
  {
    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    // Get details of named old source
    if (!strOldName.empty())
    {
      strSQL = PrepareSQL("SELECT idSource, strMultipath FROM source WHERE strName LIKE '%s'",
                          strOldName.c_str());
      if (!m_pDS->query(strSQL))
        return -1;
      if (m_pDS->num_rows() > 0)
      {
        idSource = m_pDS->fv("idSource").get_asInt();
        strSourceMultipath = m_pDS->fv("strMultipath").get_asString();
      }
      m_pDS->close();
    }
    if (idSource < 0)
    {
      // Source not found, add new one
      return AddSource(strName, strMultipath, vecPaths);
    }

    // Nothing changed? (that we hold in db, other source details could be modified)
    bool pathschanged = strMultipath.compare(strSourceMultipath) != 0;
    if (!pathschanged && strOldName.compare(strName) == 0)
      return idSource;

    if (!pathschanged)
    {
      // Name changed? Could be that none of the values held in db changed
      if (strOldName.compare(strName) != 0)
      {
        strSQL = PrepareSQL("UPDATE source SET strName = '%s' "
                            "WHERE idSource = %i",
                            strName.c_str(), idSource);
        m_pDS->exec(strSQL);
      }
      return idSource;
    }
    else
    {
      // Change paths (and name) by deleting and re-adding, but keep same ID
      strSQL = PrepareSQL("DELETE FROM source WHERE idSource = %i", idSource);
      m_pDS->exec(strSQL);
      return AddSource(strName, strMultipath, vecPaths, idSource);
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed with query ({})", strSQL);
    RollbackTransaction();
  }

  return -1;
}

bool CMusicDatabase::RemoveSource(const std::string& strName)
{
  // Related album_source and source_path rows removed by trigger
  SetLibraryLastCleaned();
  return ExecuteQuery(PrepareSQL("DELETE FROM source WHERE strName ='%s'", strName.c_str()));
}

int CMusicDatabase::GetSourceFromPath(const std::string& strPath1)
{
  std::string strSQL;
  int idSource = -1;
  try
  {
    std::string strPath(strPath1);
    if (!URIUtils::HasSlashAtEnd(strPath))
      URIUtils::AddSlashAtEnd(strPath);

    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    // Check if path is a source matching on multipath
    strSQL = PrepareSQL("SELECT idSource FROM source WHERE strMultipath = '%s'", strPath.c_str());
    if (!m_pDS->query(strSQL))
      return -1;
    if (m_pDS->num_rows() > 0)
      idSource = m_pDS->fv("idSource").get_asInt();
    m_pDS->close();
    if (idSource > 0)
      return idSource;

    // Check if path is a source path (of many) or a subfolder of a single source
    strSQL = PrepareSQL("SELECT DISTINCT idSource FROM source_path "
                        "WHERE SUBSTR('%s', 1, LENGTH(strPath)) = strPath",
                        strPath.c_str());
    if (!m_pDS->query(strSQL))
      return -1;
    if (m_pDS->num_rows() == 1)
      idSource = m_pDS->fv("idSource").get_asInt();
    m_pDS->close();
    return idSource;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "path: {} ({}) failed", strSQL, strPath1);
  }

  return -1;
}

bool CMusicDatabase::AddAlbumSource(int idAlbum, int idSource)
{
  std::string strSQL;
  strSQL = PrepareSQL("INSERT INTO album_source (idAlbum, idSource) "
                      "VALUES(%i, %i)",
                      idAlbum, idSource);
  return ExecuteQuery(strSQL);
}

bool CMusicDatabase::AddAlbumSources(int idAlbum, const std::string& strPath)
{
  std::string strSQL;
  std::vector<int> sourceIds;
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    if (!strPath.empty())
    {
      // Find sources related to album using album path
      strSQL = PrepareSQL("SELECT DISTINCT idSource FROM source_path "
                          "WHERE SUBSTR('%s', 1, LENGTH(strPath)) = strPath",
                          strPath.c_str());
      if (!m_pDS->query(strSQL))
        return false;
      while (!m_pDS->eof())
      {
        sourceIds.push_back(m_pDS->fv("idSource").get_asInt());
        m_pDS->next();
      }
      m_pDS->close();
    }
    else
    {
      // Find sources using song paths, check each source path individually
      if (nullptr == m_pDS2)
        return false;
      strSQL = "SELECT idSource, strPath FROM source_path";
      if (!m_pDS->query(strSQL))
        return false;
      while (!m_pDS->eof())
      {
        std::string sourcepath = m_pDS->fv("strPath").get_asString();
        strSQL = PrepareSQL("SELECT 1 FROM song "
                            "JOIN path ON song.idPath = path.idPath "
                            "WHERE song.idAlbum = %i AND path.strPath LIKE '%s%%%%'",
                            sourcepath.c_str());
        if (!m_pDS2->query(strSQL))
          return false;
        if (m_pDS2->num_rows() > 0)
          sourceIds.push_back(m_pDS->fv("idSource").get_asInt());
        m_pDS2->close();

        m_pDS->next();
      }
      m_pDS->close();
    }

    //Add album sources
    for (auto idSource : sourceIds)
    {
      AddAlbumSource(idAlbum, idSource);
    }

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "path: {} ({}) failed", strSQL, strPath);
  }

  return false;
}

bool CMusicDatabase::DeleteAlbumSources(int idAlbum)
{
  return ExecuteQuery(PrepareSQL("DELETE FROM album_source WHERE idAlbum = %i", idAlbum));
}

bool CMusicDatabase::CheckSources(const std::vector<CMediaSource>& sources)
{
  if (sources.empty())
  {
    // Source table empty too?
    return GetSingleValue("SELECT 1 FROM source LIMIT 1").empty();
  }

  // Check number of entries matches
  const auto total = static_cast<size_t>(GetSingleValueInt("SELECT COUNT(1) FROM source"));
  if (total != sources.size())
    return false;

  // Check individual sources match
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL;
    for (const auto& source : sources)
    {
      // Check each source by name
      strSQL = PrepareSQL("SELECT idSource, strMultipath FROM source "
                          "WHERE strName LIKE '%s'",
                          source.strName.c_str());
      m_pDS->query(strSQL);
      if (!m_pDS->query(strSQL))
        return false;
      if (m_pDS->num_rows() != 1)
      {
        // Missing source, or name duplication
        m_pDS->close();
        return false;
      }
      else
      {
        // Check details. Encoded URLs of source.strPath matched to strMultipath
        // field, no need to look at individual paths of source_path table
        if (source.strPath.compare(m_pDS->fv("strMultipath").get_asString()) != 0)
        {
          // Paths not match
          m_pDS->close();
          return false;
        }
        m_pDS->close();
      }
    }
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicDatabase::MigrateSources()
{
  //Fetch music sources from xml
  std::vector<CMediaSource> sources(*CMediaSourceSettings::GetInstance().GetSources("music"));

  std::string strSQL;
  try
  {
    // Fill source and source paths tables
    for (const auto& source : sources)
    {
      // AddSource(source.strName, source.strPath, source.vecPaths);
      // Add new source
      strSQL = PrepareSQL("INSERT INTO source (idSource, strName, strMultipath) "
                          "VALUES(NULL, '%s', '%s')",
                          source.strName.c_str(), source.strPath.c_str());
      m_pDS->exec(strSQL);
      const auto idSource = static_cast<int>(m_pDS->lastinsertid());

      // Add new source paths
      int idPath = 1;
      for (const auto& path : source.vecPaths)
      {
        strSQL = PrepareSQL("INSERT INTO source_path (idSource, idPath, strPath) "
                            "VALUES(%i,%i,'%s')",
                            idSource, idPath, path.c_str());
        m_pDS->exec(strSQL);
        ++idPath;
      }
    }

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", strSQL);
  }
  return false;
}

bool CMusicDatabase::UpdateSources()
{
  //Check library and xml sources match
  std::vector<CMediaSource> sources(*CMediaSourceSettings::GetInstance().GetSources("music"));
  if (CheckSources(sources))
    return true;

  try
  {
    // Empty sources table (related link tables removed by trigger);
    ExecuteQuery("DELETE FROM source");

    // Fill source table, and album sources
    for (const auto& source : sources)
      AddSource(source.strName, source.strPath, source.vecPaths);

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicDatabase::GetSources(CFileItemList& items)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    // Get music sources and individual source paths (may not be scanned or have albums etc.)
    std::string strSQL =
        "SELECT source.idSource, source.strName, source.strMultipath, source_path.strPath "
        "FROM source JOIN source_path ON source.idSource = source_path.idSource "
        "ORDER BY source.idSource, source_path.idPath";

    CLog::LogF(LOGDEBUG, "query: {}", strSQL);
    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_pDS->close();
      return true;
    }

    // Get data from returned rows
    // Item has source ID in MusicInfotag, multipath in path, and individual paths in property
    CVariant sourcePaths(CVariant::VariantTypeArray);
    int idSource = -1;
    while (!m_pDS->eof())
    {
      if (idSource != m_pDS->fv("source.idSource").get_asInt())
      { // New source
        if (idSource > 0 && !sourcePaths.empty())
        {
          //Store paths for previous source in item list
          items[items.Size() - 1].get()->SetProperty("paths", sourcePaths);
          sourcePaths.clear();
        }
        idSource = m_pDS->fv("source.idSource").get_asInt();
        auto pItem{std::make_shared<CFileItem>(m_pDS->fv("source.strName").get_asString())};
        pItem->GetMusicInfoTag()->SetDatabaseId(idSource, "source");
        // Set tag URL for "file" property in AudioLibary processing
        pItem->GetMusicInfoTag()->SetURL(m_pDS->fv("source.strMultipath").get_asString());
        // Set item path as source URL encoded multipath too
        pItem->SetPath(m_pDS->fv("source.strMultiPath").get_asString());

        pItem->SetFolder(true);
        items.Add(std::move(pItem));
      }
      // Get path data
      sourcePaths.push_back(m_pDS->fv("source_path.strPath").get_asString());

      m_pDS->next();
    }
    if (!sourcePaths.empty())
    {
      //Store paths for final source
      items[items.Size() - 1].get()->SetProperty("paths", sourcePaths);
      sourcePaths.clear();
    }

    // cleanup
    m_pDS->close();

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicDatabase::GetSourcesByArtist(int idArtist, CFileItem* item)
{
  if (nullptr == m_pDB)
    return false;
  if (nullptr == m_pDS)
    return false;

  try
  {
    std::string strSQL;
    strSQL = PrepareSQL("SELECT DISTINCT album_source.idSource FROM artist "
                        "JOIN album_artist ON album_artist.idArtist = artist.idArtist "
                        "JOIN album_source ON album_source.idAlbum = album_artist.idAlbum "
                        "WHERE artist.idArtist = %i "
                        "ORDER BY album_source.idSource",
                        idArtist);
    if (!m_pDS->query(strSQL))
      return false;
    if (m_pDS->num_rows() == 0)
    {
      // Artist does have any source via albums may not be an album artist.
      // Check via songs fetch sources from compilations or where they are guest artist
      m_pDS->close();
      strSQL = PrepareSQL("SELECT DISTINCT album_source.idSource, FROM song_artist "
                          "JOIN song ON song_artist.idSong = song.idSong "
                          "JOIN album_source ON album_source.idAlbum = song.idAlbum "
                          "WHERE song_artist.idArtist = %i AND song_artist.idRole = 1 "
                          "ORDER BY album_source.idSource",
                          idArtist);
      if (!m_pDS->query(strSQL))
        return false;
      if (m_pDS->num_rows() == 0)
      {
        //No sources, but query successful
        m_pDS->close();
        return true;
      }
    }

    CVariant artistSources(CVariant::VariantTypeArray);
    while (!m_pDS->eof())
    {
      artistSources.push_back(m_pDS->fv("idSource").get_asInt());
      m_pDS->next();
    }
    m_pDS->close();

    item->SetProperty("sourceid", artistSources);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idArtist);
  }
  return false;
}

bool CMusicDatabase::GetSourcesByAlbum(int idAlbum, CFileItem* item)
{
  if (nullptr == m_pDB)
    return false;
  if (nullptr == m_pDS)
    return false;

  try
  {
    std::string strSQL;
    strSQL = PrepareSQL("SELECT idSource FROM album_source "
                        "WHERE album_source.idAlbum = %i "
                        "ORDER BY idSource",
                        idAlbum);
    if (!m_pDS->query(strSQL))
      return false;
    CVariant albumSources(CVariant::VariantTypeArray);
    if (m_pDS->num_rows() > 0)
    {
      while (!m_pDS->eof())
      {
        albumSources.push_back(m_pDS->fv("idSource").get_asInt());
        m_pDS->next();
      }
      m_pDS->close();
    }
    else
    {
      //! @todo: handle singles, or don't waste time checking songs
      // Album does have any sources, may be a single??
      // Check via song paths, check each source path individually
      // usually fewer source paths than songs
      m_pDS->close();

      if (nullptr == m_pDS2)
        return false;
      strSQL = "SELECT idSource, strPath FROM source_path";
      if (!m_pDS->query(strSQL))
        return false;
      while (!m_pDS->eof())
      {
        std::string sourcepath = m_pDS->fv("strPath").get_asString();
        strSQL = PrepareSQL("SELECT 1 FROM song "
                            "JOIN path ON song.idPath = path.idPath "
                            "WHERE song.idAlbum = %i AND path.strPath LIKE '%s%%%%'",
                            idAlbum, sourcepath.c_str());
        if (!m_pDS2->query(strSQL))
          return false;
        if (m_pDS2->num_rows() > 0)
          albumSources.push_back(m_pDS->fv("idSource").get_asInt());
        m_pDS2->close();

        m_pDS->next();
      }
      m_pDS->close();
    }


    item->SetProperty("sourceid", albumSources);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idAlbum);
  }
  return false;
}

bool CMusicDatabase::GetSourcesBySong(int idSong, const std::string& strPath1, CFileItem* item)
{
  if (nullptr == m_pDB)
    return false;
  if (nullptr == m_pDS)
    return false;

  try
  {
    std::string strSQL;
    strSQL = PrepareSQL("SELECT idSource FROM song "
                        "JOIN album_source ON album_source.idAlbum = song.idAlbum "
                        "WHERE song.idSong = %i "
                        "ORDER BY idSource",
                        idSong);
    if (!m_pDS->query(strSQL))
      return false;
    if (m_pDS->num_rows() == 0 && !strPath1.empty())
    {
      // Check via song path instead
      m_pDS->close();
      std::string strPath(strPath1);
      if (!URIUtils::HasSlashAtEnd(strPath))
        URIUtils::AddSlashAtEnd(strPath);

      strSQL = PrepareSQL("SELECT DISTINCT idSource FROM source_path "
                          "WHERE SUBSTR('%s', 1, LENGTH(strPath)) = strPath",
                          strPath.c_str());
      if (!m_pDS->query(strSQL))
        return false;
    }
    CVariant songSources(CVariant::VariantTypeArray);
    while (!m_pDS->eof())
    {
      songSources.push_back(m_pDS->fv("idSource").get_asInt());
      m_pDS->next();
    }
    m_pDS->close();

    item->SetProperty("sourceid", songSources);
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", idSong);
  }
  return false;
}

int CMusicDatabase::GetSourceByName(const std::string& strSource)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL;
    strSQL = PrepareSQL("SELECT idSource FROM source WHERE strName LIKE '%s'", strSource.c_str());
    // run query
    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_pDS->close();
      return -1;
    }
    return m_pDS->fv("idSource").get_asInt();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

std::string CMusicDatabase::GetSourceById(int id) const
{
  return GetSingleValue("source", "strName", PrepareSQL("idSource = %i", id));
}

int CMusicDatabase::GetArtistByName(const std::string& strArtist)
{
  return m_crudRepo.GetArtistByName(strArtist);
}


int CMusicDatabase::GetArtistByMatch(const CArtist& artist)
{
  return m_crudRepo.GetArtistByMatch(artist);
}


bool CMusicDatabase::GetArtistFromSong(int idSong, CArtist& artist)
{
  return m_crudRepo.GetArtistFromSong(idSong, artist);
}


bool CMusicDatabase::IsSongArtist(int idSong, int idArtist) const
{
  return m_crudRepo.IsSongArtist(idSong, idArtist);
}


bool CMusicDatabase::IsSongAlbumArtist(int idSong, int idArtist) const
{
  return m_crudRepo.IsSongAlbumArtist(idSong, idArtist);
}


bool CMusicDatabase::IsAlbumBoxset(int idAlbum) const
{
  std::string strSQL = PrepareSQL("SELECT bBoxedSet FROM album WHERE idAlbum = %i", idAlbum);
  int isBoxSet = GetSingleValueInt(strSQL);
  return (isBoxSet == 1 ? true : false);
}

int CMusicDatabase::GetAlbumByName(const std::string& strAlbum, const std::string& strArtist)
{
  return m_crudRepo.GetAlbumByName(strAlbum, strArtist);
}


bool CMusicDatabase::GetMatchingMusicVideoAlbum(const std::string& strAlbum,
                                             const std::string& strArtist,
                                             int& idAlbum,
                                             std::string& strReview)
{
  return m_crudRepo.GetMatchingMusicVideoAlbum(strAlbum, strArtist, idAlbum, strReview);
}


bool CMusicDatabase::SearchAlbumsByArtistName(const std::string& strArtist, CFileItemList& items)
{
  return m_crudRepo.SearchAlbumsByArtistName(strArtist, items);
}


int CMusicDatabase::GetAlbumByName(const std::string& strAlbum,
                                   const std::vector<std::string>& artist)
{
  return m_crudRepo.GetAlbumByName(strAlbum, artist);
}

int CMusicDatabase::GetAlbumByMatch(const CAlbum& album)
{
  return m_crudRepo.GetAlbumByMatch(album);
}


std::string CMusicDatabase::GetGenreById(int id) const
{
  return GetSingleValue("genre", "strGenre", PrepareSQL("idGenre=%i", id));
}

std::string CMusicDatabase::GetArtistById(int id) const
{
  return m_crudRepo.GetArtistById(id);
}


std::string CMusicDatabase::GetRoleById(int id) const
{
  return m_crudRepo.GetRoleById(id);
}


bool CMusicDatabase::UpdateArtistSortNames(int idArtist /*=-1*/)
{
  return m_crudRepo.UpdateArtistSortNames(idArtist);
}


std::string CMusicDatabase::GetAlbumById(int id) const
{
  return m_crudRepo.GetAlbumById(id);
}


int CMusicDatabase::GetGenreByName(const std::string& strGenre)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL;
    strSQL = PrepareSQL("SELECT idGenre FROM genre "
                        "WHERE genre.strGenre LIKE '%s'",
                        strGenre.c_str());
    // run query
    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_pDS->close();
      return -1;
    }
    return m_pDS->fv("genre.idGenre").get_asInt();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return -1;
}

bool CMusicDatabase::GetGenresJSON(CFileItemList& items, bool bSources)
{
  std::string strSQL;
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    strSQL = "SELECT %s FROM genre ";
    Filter extFilter;
    extFilter.AppendField("genre.idGenre");
    extFilter.AppendField("genre.strGenre");
    if (bSources)
    {
      strSQL = "SELECT DISTINCT %s FROM genre ";
      extFilter.AppendField("album_source.idSource");
      extFilter.AppendJoin("JOIN song_genre ON song_genre.idGenre = genre.idGenre");
      extFilter.AppendJoin("JOIN song ON song.idSong = song_genre.idSong");
      extFilter.AppendJoin("JOIN album ON album.idAlbum = song.idAlbum");
      extFilter.AppendJoin("LEFT JOIN album_source on album_source.idAlbum = album.idAlbum");
      extFilter.AppendOrder("genre.strGenre");
      extFilter.AppendOrder("album_source.idSource");
    }
    extFilter.AppendWhere("genre.strGenre != ''");

    std::string strSQLExtra;
    if (!BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    strSQL = PrepareSQL(strSQL, extFilter.fields.c_str()) + strSQLExtra;

    // run query
    CLog::LogF(LOGDEBUG, "query: {}", strSQL);

    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_pDS->close();
      return true;
    }

    if (!bSources)
      items.Reserve(iRowsFound);

    // Get data from returned rows
    // Item has genre name and ID in MusicInfotag, VFS path, and sources in property
    CVariant genreSources(CVariant::VariantTypeArray);
    int idGenre = -1;
    while (!m_pDS->eof())
    {
      if (idGenre != m_pDS->fv("genre.idGenre").get_asInt())
      { // New genre
        if (idGenre > 0 && bSources)
        {
          //Store sources for previous genre in item list
          items[items.Size() - 1].get()->SetProperty("sourceid", genreSources);
          genreSources.clear();
        }
        idGenre = m_pDS->fv("genre.idGenre").get_asInt();
        std::string strGenre = m_pDS->fv("genre.strGenre").get_asString();
        auto pItem{std::make_shared<CFileItem>(strGenre)};
        pItem->GetMusicInfoTag()->SetTitle(strGenre);
        pItem->GetMusicInfoTag()->SetGenre(strGenre);
        pItem->GetMusicInfoTag()->SetDatabaseId(idGenre, "genre");
        pItem->SetPath(StringUtils::Format("musicdb://genres/{}/", idGenre));
        pItem->SetFolder(true);
        items.Add(std::move(pItem));
      }
      // Get source data
      if (bSources)
      {
        int sourceid = m_pDS->fv("album_source.idSource").get_asInt();
        if (sourceid > 0)
          genreSources.push_back(sourceid);
      }
      m_pDS->next();
    }
    if (bSources)
    {
      //Store sources for final genre
      items[items.Size() - 1].get()->SetProperty("sourceid", genreSources);
    }

    // cleanup
    m_pDS->close();

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", strSQL);
  }
  return false;
}

std::string CMusicDatabase::GetAlbumDiscTitle(int idAlbum, int idDisc) const
{
  return m_crudRepo.GetAlbumDiscTitle(idAlbum, idDisc);
}


int CMusicDatabase::GetBoxsetsCount() const
{
  return GetSingleValueInt("album", "count(idAlbum)", "bBoxedSet = 1");
}

int CMusicDatabase::GetAlbumDiscsCount(int idAlbum) const
{
  return m_crudRepo.GetAlbumDiscsCount(idAlbum);
}


int CMusicDatabase::GetCompilationAlbumsCount() const
{
  return GetSingleValueInt("album", "count(idAlbum)", "bCompilation = 1");
}

int CMusicDatabase::GetSinglesCount()
{
  CDatabase::Filter filter(
      PrepareSQL("songview.idAlbum IN (SELECT idAlbum FROM album WHERE strReleaseType = '%s')",
                 CAlbum::ReleaseTypeToString(ReleaseType::Single).c_str()));
  return GetSongsCount(filter);
}

int CMusicDatabase::GetArtistCountForRole(int role) const
{
  std::string strSQL = PrepareSQL(
      "SELECT COUNT(DISTINCT idartist) FROM song_artist WHERE song_artist.idRole = %i", role);
  return GetSingleValueInt(strSQL);
}

int CMusicDatabase::GetArtistCountForRole(const std::string& strRole) const
{
  std::string strSQL = PrepareSQL("SELECT COUNT(DISTINCT idartist) FROM song_artist "
                                  "JOIN role ON song_artist.idRole = role.idRole "
                                  "WHERE role.strRole LIKE '%s'",
                                  strRole.c_str());
  return GetSingleValueInt(strSQL);
}

bool CMusicDatabase::SetPathHash(const std::string& path, const std::string& hash)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    if (hash.empty())
    { // this is an empty folder - we need only add it to the path table
      // if the path actually exists
      if (!CDirectory::Exists(path))
        return false;
    }
    int idPath = AddPath(path);
    if (idPath < 0)
      return false;

    std::string strSQL =
        PrepareSQL("UPDATE path SET strHash='%s' WHERE idPath=%ld", hash.c_str(), idPath);
    m_pDS->exec(strSQL);

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}, {}) failed", path, hash);
  }

  return false;
}

bool CMusicDatabase::GetPathHash(const std::string& path, std::string& hash)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL = PrepareSQL("select strHash from path where strPath='%s'", path.c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
      return false;
    hash = m_pDS->fv("strHash").get_asString();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", path);
  }

  return false;
}

bool CMusicDatabase::RemoveSongsFromPath(const std::string& path,
                                       std::map<std::string, std::vector<CSong>>& songmap,
                                       bool exact)
{
  return m_crudRepo.RemoveSongsFromPath(path, songmap, exact);
}


void CMusicDatabase::CheckArtistLinksChanged()
{
  m_crudRepo.CheckArtistLinksChanged();
}


bool CMusicDatabase::GetPaths(std::set<std::string, std::less<>>& paths)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    paths.clear();

    // find all paths
    if (!m_pDS->query("SELECT strPath FROM path"))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_pDS->close();
      return true;
    }
    while (!m_pDS->eof())
    {
      paths.insert(m_pDS->fv("strPath").get_asString());
      m_pDS->next();
    }
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

bool CMusicDatabase::SetSongUserrating(const std::string& filePath, int userrating)
{
  return m_crudRepo.SetSongUserrating(filePath, userrating);
}


bool CMusicDatabase::SetSongUserrating(int idSong, int userrating)
{
  return m_crudRepo.SetSongUserrating(idSong, userrating);
}


bool CMusicDatabase::SetAlbumUserrating(const int idAlbum, int userrating)
{
  return m_crudRepo.SetAlbumUserrating(idAlbum, userrating);
}


bool CMusicDatabase::SetSongVotes(const std::string& filePath, int votes)
{
  return m_crudRepo.SetSongVotes(filePath, votes);
}


int CMusicDatabase::GetSongIDFromPath(const std::string& filePath)
{
  // grab the where string to identify the song id
  CURL url(filePath);
  if (url.IsProtocol("musicdb"))
  {
    std::string strFile = URIUtils::GetFileName(filePath);
    URIUtils::RemoveExtension(strFile);
    return atoi(strFile.c_str());
  }
  // hit the db
  try
  {
    if (nullptr == m_pDB)
      return -1;
    if (nullptr == m_pDS)
      return -1;

    std::string strPath;
    std::string strFileName;
    SplitPath(filePath, strPath, strFileName);
    URIUtils::AddSlashAtEnd(strPath);

    std::string sql = PrepareSQL("SELECT idSong FROM song JOIN path ON song.idPath = path.idPath "
                                 "WHERE song.strFileName='%s' AND path.strPath='%s'",
                                 strFileName.c_str(), strPath.c_str());
    if (!m_pDS->query(sql))
      return -1;

    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      return -1;
    }

    int songID = m_pDS->fv("idSong").get_asInt();
    m_pDS->close();
    return songID;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", filePath);
  }
  return -1;
}

bool CMusicDatabase::CommitTransaction()
{
  if (CDatabase::CommitTransaction())
  { // number of items in the db has likely changed, so reset the infomanager cache
    CGUIComponent* gui = CServiceBroker::GetGUI();
    if (gui)
    {
      gui->GetInfoManager().GetInfoProviders().GetLibraryInfoProvider().SetLibraryBool(
          LIBRARY_HAS_MUSIC, GetSongsCount() > 0);
      return true;
    }
  }
  return false;
}

bool CMusicDatabase::SetScraperAll(const std::string& strBaseDir, const ADDON::ScraperPtr& scraper)
{
  if (nullptr == m_pDB)
    return false;
  if (nullptr == m_pDS)
    return false;
  std::string strSQL;
  int idSetting = -1;
  try
  {
    ADDON::ContentType content = ADDON::ContentType::NONE;

    // Build where clause from virtual path
    Filter extFilter;
    CMusicDbUrl musicUrl;
    SortDescription sorting;
    if (!musicUrl.FromString(strBaseDir) || !GetFilter(musicUrl, extFilter, sorting))
      return false;

    std::string itemType = musicUrl.GetType();
    if (StringUtils::EqualsNoCase(itemType, "artists"))
    {
      content = ADDON::ContentType::ARTISTS;
    }
    else if (StringUtils::EqualsNoCase(itemType, "albums"))
    {
      content = ADDON::ContentType::ALBUMS;
    }
    else
      return false; //Only artists and albums have info settings

    std::string strSQLWhere;
    if (!BuildSQL(strSQLWhere, extFilter, strSQLWhere))
      return false;

    // Replace view names with table names
    StringUtils::Replace(strSQLWhere, "artistview", "artist");
    StringUtils::Replace(strSQLWhere, "albumview", "album");

    BeginTransaction();
    // Clear current scraper settings (0 => default scraper used)
    if (content == ADDON::ContentType::ARTISTS)
      strSQL = "UPDATE artist SET idInfoSetting = %i ";
    else
      strSQL = "UPDATE album SET idInfoSetting = %i ";
    strSQL = PrepareSQL(strSQL, 0) + strSQLWhere;
    m_pDS->exec(strSQL);

    //Remove orphaned settings
    CleanupInfoSettings();

    if (scraper)
    {
      // Add new info setting
      strSQL = "INSERT INTO infosetting (strScraperPath, strSettings) values ('%s','%s')";
      strSQL = PrepareSQL(strSQL, scraper->ID().c_str(), scraper->GetPathSettings().c_str());
      m_pDS->exec(strSQL);
      idSetting = static_cast<int>(m_pDS->lastinsertid());

      if (content == ADDON::ContentType::ARTISTS)
        strSQL = "UPDATE artist SET idInfoSetting = %i ";
      else
        strSQL = "UPDATE album SET idInfoSetting = %i ";
      strSQL = PrepareSQL(strSQL, idSetting) + strSQLWhere;
      m_pDS->exec(strSQL);
    }
    CommitTransaction();
    return true;
  }
  catch (...)
  {
    RollbackTransaction();
    CLog::LogF(LOGERROR, "({}, {}) failed", strBaseDir, strSQL);
  }
  return false;
}

bool CMusicDatabase::SetScraper(int id,
                                ADDON::ContentType content,
                                const ADDON::ScraperPtr& scraper)
{
  if (nullptr == m_pDB)
    return false;
  if (nullptr == m_pDS)
    return false;
  std::string strSQL;
  int idSetting = -1;
  try
  {
    BeginTransaction();
    // Fetch current info settings for item, 0 => default is used
    if (content == ADDON::ContentType::ARTISTS)
      strSQL = "SELECT idInfoSetting FROM artist WHERE idArtist = %i";
    else
      strSQL = "SELECT idInfoSetting FROM album WHERE idAlbum = %i";
    strSQL = PrepareSQL(strSQL, id);
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() > 0)
      idSetting = m_pDS->fv("idInfoSetting").get_asInt();
    m_pDS->close();

    if (idSetting < 1)
    { // Add new info setting
      strSQL = "INSERT INTO infosetting (strScraperPath, strSettings) values ('%s','%s')";
      strSQL = PrepareSQL(strSQL, scraper->ID().c_str(), scraper->GetPathSettings().c_str());
      m_pDS->exec(strSQL);
      idSetting = static_cast<int>(m_pDS->lastinsertid());

      if (content == ADDON::ContentType::ARTISTS)
        strSQL = "UPDATE artist SET idInfoSetting = %i WHERE idArtist = %i";
      else
        strSQL = "UPDATE album SET idInfoSetting = %i WHERE idAlbum = %i";
      strSQL = PrepareSQL(strSQL, idSetting, id);
      m_pDS->exec(strSQL);
    }
    else
    { // Update info setting
      strSQL = "UPDATE infosetting SET strScraperPath = '%s', strSettings = '%s' "
               "WHERE idSetting = %i";
      strSQL =
          PrepareSQL(strSQL, scraper->ID().c_str(), scraper->GetPathSettings().c_str(), idSetting);
      m_pDS->exec(strSQL);
    }
    CommitTransaction();
    return true;
  }
  catch (...)
  {
    RollbackTransaction();
    CLog::LogF(LOGERROR, "({}, {}) failed", id, strSQL);
  }
  return false;
}

bool CMusicDatabase::GetScraper(int id, ADDON::ContentType content, ADDON::ScraperPtr& scraper)
{
  std::string scraperUUID;
  std::string strSettings;
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL;
    strSQL = "SELECT strScraperPath, strSettings FROM infosetting JOIN ";
    if (content == ADDON::ContentType::ARTISTS)
      strSQL = strSQL + "artist ON artist.idInfoSetting = infosetting.idSetting "
                        "WHERE artist.idArtist = %i";
    else
      strSQL = strSQL + "album ON album.idInfoSetting = infosetting.idSetting "
                        "WHERE album.idAlbum = %i";
    strSQL = PrepareSQL(strSQL, id);
    m_pDS->query(strSQL);
    if (!m_pDS->eof())
    { // try and ascertain scraper
      scraperUUID = m_pDS->fv("strScraperPath").get_asString();
      strSettings = m_pDS->fv("strSettings").get_asString();

      // Use pre configured or default scraper
      ADDON::AddonPtr addon;
      if (!scraperUUID.empty() &&
          CServiceBroker::GetAddonMgr().GetAddon(scraperUUID, addon,
                                                 ADDON::OnlyEnabled::CHOICE_YES) &&
          addon)
      {
        scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon);
        if (scraper)
          // Set settings
          scraper->SetPathSettings(content, strSettings);
      }
    }
    m_pDS->close();

    if (!scraper)
    { // use default music scraper instead
      ADDON::AddonPtr addon;
      if (ADDON::CAddonSystemSettings::GetInstance().GetActive(
              ADDON::ScraperTypeFromContent(content), addon))
      {
        scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon);
        return scraper != nullptr;
      }
      else
        return false;
    }

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}, {} {}) failed", id, scraperUUID, strSettings);
  }
  return false;
}

bool CMusicDatabase::ScraperInUse(const std::string& scraperID) const
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string sql =
        PrepareSQL("SELECT COUNT(1) FROM infosetting WHERE strScraperPath='%s'", scraperID.c_str());
    if (!m_pDS->query(sql) || m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      return false;
    }
    bool found = m_pDS->fv(0).get_asInt() > 0;
    m_pDS->close();
    return found;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", scraperID);
  }
  return false;
}

bool CMusicDatabase::GetItems(const std::string& strBaseDir,
                              CFileItemList& items,
                              const SortDescription& sortDescription,
                              const Filter& filter /* = Filter() */)
{
  CMusicDbUrl musicUrl;
  if (!musicUrl.FromString(strBaseDir))
    return false;

  return GetItems(strBaseDir, musicUrl.GetType(), items, sortDescription, filter);
}

bool CMusicDatabase::GetItems(const std::string& strBaseDir,
                              const std::string& itemType,
                              CFileItemList& items,
                              const SortDescription& sortDescription,
                              const Filter& filter /* = Filter() */)
{
  if (StringUtils::EqualsNoCase(itemType, "genres"))
    return GetGenresNav(strBaseDir, items, filter);
  else if (StringUtils::EqualsNoCase(itemType, "sources"))
    return GetSourcesNav(strBaseDir, items, filter);
  else if (StringUtils::EqualsNoCase(itemType, "years"))
    return GetYearsNav(strBaseDir, items, filter);
  else if (StringUtils::EqualsNoCase(itemType, "roles"))
    return GetRolesNav(strBaseDir, items, filter);
  else if (StringUtils::EqualsNoCase(itemType, "artists"))
    return GetArtistsNav(strBaseDir, items, sortDescription,
                         !CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
                             CSettings::SETTING_MUSICLIBRARY_SHOWCOMPILATIONARTISTS),
                         -1, -1, -1, filter, false);
  else if (StringUtils::EqualsNoCase(itemType, "albums"))
    return GetAlbumsByWhere(strBaseDir, items, sortDescription, filter);
  else if (StringUtils::EqualsNoCase(itemType, "discs"))
    return GetDiscsByWhere(strBaseDir, items, sortDescription, filter);
  else if (StringUtils::EqualsNoCase(itemType, "songs"))
    return GetSongsFullByWhere(strBaseDir, items, sortDescription, filter, true);

  return false;
}

std::string CMusicDatabase::GetItemById(const std::string& itemType, int id) const
{
  if (StringUtils::EqualsNoCase(itemType, "genres"))
    return GetGenreById(id);
  else if (StringUtils::EqualsNoCase(itemType, "sources"))
    return GetSourceById(id);
  else if (StringUtils::EqualsNoCase(itemType, "years"))
    return std::to_string(id);
  else if (StringUtils::EqualsNoCase(itemType, "artists"))
    return GetArtistById(id);
  else if (StringUtils::EqualsNoCase(itemType, "albums"))
    return GetAlbumById(id);
  else if (StringUtils::EqualsNoCase(itemType, "roles"))
    return GetRoleById(id);

  return "";
}

void CMusicDatabase::ExportToXML(const CLibExportSettings& settings,
                                 CGUIDialogProgress* progressDialog /*= nullptr*/)
{
  if (!settings.IsItemExported(ELIBEXPORT_ALBUMARTISTS) &&
      !settings.IsItemExported(ELIBEXPORT_SONGARTISTS) &&
      !settings.IsItemExported(ELIBEXPORT_OTHERARTISTS) &&
      !settings.IsItemExported(ELIBEXPORT_ALBUMS) && !settings.IsItemExported(ELIBEXPORT_SONGS))
    return;

  // Exporting albums either art or NFO (or both) selected
  if ((settings.IsToLibFolders() || settings.IsSeparateFiles()) && settings.IsSkipNfo() &&
      !settings.IsArtwork() && settings.IsItemExported(ELIBEXPORT_ALBUMS))
    return;

  std::string strFolder;
  if (settings.IsSingleFile() || settings.IsSeparateFiles())
  {
    // Exporting to single file or separate files in a specified location
    if (settings.GetPath().empty())
      return;

    strFolder = settings.GetPath();
    if (!URIUtils::HasSlashAtEnd(strFolder))
      URIUtils::AddSlashAtEnd(strFolder);
    strFolder = URIUtils::GetDirectory(strFolder);
    if (strFolder.empty())
      return;
  }
  else if (settings.IsArtistFoldersOnly() || (settings.IsToLibFolders() && settings.IsArtists()))
  {
    // Exporting artist folders only, or artist NFO or art to library folders
    // need Artist Information Folder defined.
    // (Album NFO and art goes to music folders)
    strFolder = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
        CSettings::SETTING_MUSICLIBRARY_ARTISTSFOLDER);
    if (strFolder.empty())
      return;
  }

  //
  bool artistfoldersonly;
  artistfoldersonly = settings.IsArtistFoldersOnly() ||
                      ((settings.IsToLibFolders() || settings.IsSeparateFiles()) &&
                       settings.IsSkipNfo() && !settings.IsArtwork());

  int iFailCount = 0;
  try
  {
    if (nullptr == m_pDB)
      return;
    if (nullptr == m_pDS)
      return;
    if (nullptr == m_pDS2)
      return;

    // Create our xml document
    CXBMCTinyXML xmlDoc;
    TiXmlDeclaration decl("1.0", "UTF-8", "yes");
    xmlDoc.InsertEndChild(decl);
    TiXmlNode* pMain = nullptr;
    if ((settings.IsToLibFolders() || settings.IsSeparateFiles()) && !artistfoldersonly)
      pMain = &xmlDoc;
    else if (settings.IsSingleFile())
    {
      TiXmlElement xmlMainElement("musicdb");
      pMain = xmlDoc.InsertEndChild(xmlMainElement);
    }

    if (settings.IsItemExported(ELIBEXPORT_ALBUMS) && !artistfoldersonly)
    {
      // Find albums to export
      std::vector<int> albumIds;
      std::string strSQL = PrepareSQL("SELECT idAlbum FROM album WHERE strReleaseType = '%s' ",
                                      CAlbum::ReleaseTypeToString(ReleaseType::Album).c_str());
      if (!settings.IsUnscraped())
        strSQL += "AND lastScraped IS NOT NULL";
      CLog::LogF(LOGDEBUG, "{}", strSQL);
      m_pDS->query(strSQL);

      int total = m_pDS->num_rows();
      int current = 0;

      albumIds.reserve(total);
      while (!m_pDS->eof())
      {
        albumIds.push_back(m_pDS->fv("idAlbum").get_asInt());
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto& albumId : albumIds)
      {
        CAlbum album;
        GetAlbum(albumId, album);
        std::string strAlbumPath;
        std::string strPath;
        // Get album path, empty unless all album songs are under a unique folder, and
        // there are no songs from another album in the same folder.
        if (!GetAlbumPath(albumId, strAlbumPath))
          strAlbumPath.clear();
        if (settings.IsSingleFile())
        {
          // Save album to xml, including album path
          album.Save(pMain, "album", strAlbumPath);
        }
        else
        { // Separate files and artwork
          bool pathfound = false;
          if (settings.IsToLibFolders())
          { // Save album.nfo and artwork with music files.
            // Most albums are under a unique folder, but if songs from various albums are mixed then
            // avoid overwriting by not allow NFO and art to be exported
            if (strAlbumPath.empty())
              CLog::LogF(LOGDEBUG, "Not exporting album {} as unique path not found",
                         album.strAlbum);
            else if (!CDirectory::Exists(strAlbumPath))
              CLog::LogF(LOGDEBUG, "Not exporting album {} as found path {} does not exist",
                         album.strAlbum, strAlbumPath);
            else
            {
              strPath = strAlbumPath;
              pathfound = true;
            }
          }
          else
          { // Save album.nfo and artwork to subfolder on export path
            // strPath = strFolder/<albumartist name>/<albumname>
            // where <albumname> is either the same name as the album folder
            // containing the music files (if unique) or is created using the album name
            std::string strAlbumArtist;
            pathfound = GetArtistFolderName(album.GetAlbumArtist()[0],
                                            album.GetMusicBrainzAlbumArtistID()[0], strAlbumArtist);
            if (pathfound)
            {
              strPath = URIUtils::AddFileToFolder(strFolder, strAlbumArtist);
              pathfound = CDirectory::Exists(strPath);
              if (!pathfound)
                pathfound = CDirectory::Create(strPath);
            }
            if (!pathfound)
              CLog::LogF(LOGDEBUG, "Not exporting album {} as could not create {}", album.strAlbum,
                         strPath);
            else
            {
              std::string strAlbumFolder;
              pathfound = GetAlbumFolder(album, strAlbumPath, strAlbumFolder);
              if (pathfound)
              {
                strPath = URIUtils::AddFileToFolder(strPath, strAlbumFolder);
                pathfound = CDirectory::Exists(strPath);
                if (!pathfound)
                  pathfound = CDirectory::Create(strPath);
              }
              if (!pathfound)
                CLog::LogF(LOGDEBUG, "Not exporting album {} as could not create {}",
                           album.strAlbum, strPath);
            }
          }
          if (pathfound)
          {
            if (!settings.IsSkipNfo())
            {
              // Save album to NFO, including album path
              album.Save(pMain, "album", strAlbumPath);
              std::string nfoFile = URIUtils::AddFileToFolder(strPath, "album.nfo");
              if (settings.IsOverwrite() || !CFile::Exists(nfoFile))
              {
                if (!xmlDoc.SaveFile(nfoFile))
                {
                  CLog::LogF(LOGERROR, "Album nfo export failed! ('{}')", nfoFile);
                  CGUIDialogKaiToast::QueueNotification(
                      CGUIDialogKaiToast::Error,
                      CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(20302),
                      CURL::GetRedacted(nfoFile));
                  iFailCount++;
                }
              }
            }
            if (settings.IsArtwork())
            {
              // Save art in album folder
              // Note thumb resolution may be lower than original when overwriting
              KODI::ART::Artwork artwork;
              std::string savedArtfile;
              if (GetArtForItem(album.idAlbum, MediaTypeAlbum, artwork))
              {
                for (const auto& [type, url] : artwork)
                {
                  if (type == "thumb")
                    savedArtfile = URIUtils::AddFileToFolder(strPath, "folder");
                  else
                    savedArtfile = URIUtils::AddFileToFolder(strPath, type);
                  CServiceBroker::GetTextureCache()->Export(url, savedArtfile,
                                                            settings.IsOverwrite());
                }
              }
            }
            xmlDoc.Clear();
            xmlDoc.InsertEndChild(decl); // TiXmlDeclaration ("1.0", "UTF-8", "yes")
          }
        }

        if ((current % 50) == 0 && progressDialog)
        {
          progressDialog->SetLine(1, CVariant{album.strAlbum});
          progressDialog->SetPercentage(current * 100 / total);
          if (progressDialog->IsCanceled())
            return;
        }
        current++;
      }
    }

    // Export song playback history to single file only
    if (settings.IsSingleFile() && settings.IsItemExported(ELIBEXPORT_SONGS))
    {
      if (!ExportSongHistory(pMain, progressDialog))
        return;
    }

    if ((settings.IsArtists() || artistfoldersonly) && !strFolder.empty())
    {
      // Find artists to export
      std::vector<int> artistIds;
      Filter filter;

      if (settings.IsItemExported(ELIBEXPORT_ALBUMARTISTS))
        filter.AppendWhere("EXISTS(SELECT 1 FROM album_artist "
                           "WHERE album_artist.idArtist = artist.idArtist)",
                           false);
      if (settings.IsItemExported(ELIBEXPORT_SONGARTISTS))
      {
        if (settings.IsItemExported(ELIBEXPORT_OTHERARTISTS))
          filter.AppendWhere("EXISTS (SELECT 1 FROM song_artist "
                             "WHERE song_artist.idArtist = artist.idArtist )",
                             false);
        else
          filter.AppendWhere(
              "EXISTS (SELECT 1 FROM song_artist "
              "WHERE song_artist.idArtist = artist.idArtist AND song_artist.idRole = 1)",
              false);
      }
      else if (settings.IsItemExported(ELIBEXPORT_OTHERARTISTS))
        filter.AppendWhere(
            "EXISTS (SELECT 1 FROM song_artist "
            "WHERE song_artist.idArtist = artist.idArtist AND song_artist.idRole > 1)",
            false);

      if (!settings.IsUnscraped() && !artistfoldersonly)
        filter.AppendWhere("lastScraped IS NOT NULL", true);

      std::string strSQL = "SELECT idArtist FROM artist";
      BuildSQL(strSQL, filter, strSQL);
      CLog::LogF(LOGDEBUG, "{}", strSQL);

      m_pDS->query(strSQL);
      int total = m_pDS->num_rows();
      int current = 0;
      artistIds.reserve(total);
      while (!m_pDS->eof())
      {
        artistIds.push_back(m_pDS->fv("idArtist").get_asInt());
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto& artistId : artistIds)
      {
        CArtist artist;
        // Include discography when not folders only
        GetArtist(artistId, artist, !artistfoldersonly);
        std::string strPath;
        KODI::ART::Artwork artwork;
        if (settings.IsSingleFile())
        {
          // Save artist to xml, and old path (common to music files) if it has one
          GetOldArtistPath(artist.idArtist, strPath);
          artist.Save(pMain, "artist", strPath);

          if (GetArtForItem(artist.idArtist, MediaTypeArtist, artwork))
          { // append to the XML
            TiXmlElement additionalNode("art");
            for (const auto& [type, url] : artwork)
              XMLUtils::SetString(&additionalNode, type.c_str(), url);
            pMain->LastChild()->InsertEndChild(additionalNode);
          }
        }
        else
        { // Separate files: artist.nfo and artwork in strFolder/<artist name>
          // Get unique folder allowing for duplicate names e.g. 2 x John Williams
          bool pathfound = GetArtistFolderName(artist, strPath);
          if (pathfound)
          {
            strPath = URIUtils::AddFileToFolder(strFolder, strPath);
            pathfound = CDirectory::Exists(strPath);
            if (!pathfound)
              pathfound = CDirectory::Create(strPath);
          }
          if (!pathfound)
            CLog::LogF(LOGDEBUG, "Not exporting artist {} as could not create {}", artist.strArtist,
                       strPath);
          else
          {
            if (!artistfoldersonly)
            {
              if (!settings.IsSkipNfo())
              {
                artist.Save(pMain, "artist", strPath);
                std::string nfoFile = URIUtils::AddFileToFolder(strPath, "artist.nfo");
                if (settings.IsOverwrite() || !CFile::Exists(nfoFile))
                {
                  if (!xmlDoc.SaveFile(nfoFile))
                  {
                    CLog::LogF(LOGERROR, "Artist nfo export failed! ('{}')", nfoFile);
                    CGUIDialogKaiToast::QueueNotification(
                        CGUIDialogKaiToast::Error,
                        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(20302),
                        CURL::GetRedacted(nfoFile));
                    iFailCount++;
                  }
                }
              }
              if (settings.IsArtwork())
              {
                std::string savedArtfile;
                if (GetArtForItem(artist.idArtist, MediaTypeArtist, artwork))
                {
                  for (const auto& [type, url] : artwork)
                  {
                    if (type == "thumb")
                      savedArtfile = URIUtils::AddFileToFolder(strPath, "folder");
                    else
                      savedArtfile = URIUtils::AddFileToFolder(strPath, type);
                    CServiceBroker::GetTextureCache()->Export(url, savedArtfile,
                                                              settings.IsOverwrite());
                  }
                }
              }
              xmlDoc.Clear();
              xmlDoc.InsertEndChild(decl); // TiXmlDeclaration ("1.0", "UTF-8", "yes")
            }
          }
        }
        if ((current % 50) == 0 && progressDialog)
        {
          progressDialog->SetLine(1, CVariant{artist.strArtist});
          progressDialog->SetPercentage(current * 100 / total);
          if (progressDialog->IsCanceled())
            return;
        }
        current++;
      }
    }

    if (settings.IsSingleFile())
    {
      std::string xmlFile = URIUtils::AddFileToFolder(
          strFolder, "kodi_musicdb" + CDateTime::GetCurrentDateTime().GetAsDBDate() + ".xml");
      if (CFile::Exists(xmlFile))
        xmlFile = URIUtils::AddFileToFolder(
            strFolder, "kodi_musicdb" + CDateTime::GetCurrentDateTime().GetAsSaveString() + ".xml");
      xmlDoc.SaveFile(xmlFile);

      CVariant data;
      data["file"] = xmlFile;
      if (iFailCount > 0)
        data["failcount"] = iFailCount;
      CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::AudioLibrary, "OnExport",
                                                         data);
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
    iFailCount++;
  }

  if (progressDialog)
    progressDialog->Close();

  if (iFailCount > 0 && progressDialog)
    HELPERS::ShowOKDialogLines(
        CVariant{20196},
        CVariant{StringUtils::Format(
            CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(15011), iFailCount)});
}

bool CMusicDatabase::ExportSongHistory(TiXmlNode* pNode, CGUIDialogProgress* progressDialog)
{
  try
  {
    // Export songs with some playback history
    std::string strSQL =
        "SELECT idSong, song.idAlbum, "
        "strAlbum, strMusicBrainzAlbumID, album.strArtistDisp AS strAlbumArtistDisp, "
        "song.strArtistDisp, strTitle, iTrack, strFileName, strMusicBrainzTrackID, "
        "iTimesPlayed, lastplayed, song.rating, song.votes, song.userrating "
        "FROM song JOIN album on album.idAlbum = song.idAlbum "
        "WHERE iTimesPlayed > 0 OR rating > 0 or userrating > 0";

    CLog::LogF(LOGDEBUG, "{}", strSQL);
    m_pDS->query(strSQL);

    int total = m_pDS->num_rows();
    int current = 0;
    while (!m_pDS->eof())
    {
      TiXmlElement songElement("song");
      TiXmlNode* song = pNode->InsertEndChild(songElement);

      XMLUtils::SetInt(song, "idsong", m_pDS->fv("idSong").get_asInt());
      XMLUtils::SetString(song, "artistdesc", m_pDS->fv("strArtistDisp").get_asString());
      XMLUtils::SetString(song, "title", m_pDS->fv("strTitle").get_asString());
      XMLUtils::SetInt(song, "track", m_pDS->fv("iTrack").get_asInt());
      XMLUtils::SetString(song, "filename", m_pDS->fv("strFilename").get_asString());
      XMLUtils::SetString(song, "musicbrainztrackid",
                          m_pDS->fv("strMusicBrainzTrackID").get_asString());
      XMLUtils::SetInt(song, "idalbum", m_pDS->fv("idAlbum").get_asInt());
      XMLUtils::SetString(song, "albumtitle", m_pDS->fv("strAlbum").get_asString());
      XMLUtils::SetString(song, "musicbrainzalbumid",
                          m_pDS->fv("strMusicBrainzAlbumID").get_asString());
      XMLUtils::SetString(song, "albumartistdesc", m_pDS->fv("strAlbumArtistDisp").get_asString());
      XMLUtils::SetInt(song, "timesplayed", m_pDS->fv("iTimesplayed").get_asInt());
      XMLUtils::SetString(song, "lastplayed", m_pDS->fv("lastplayed").get_asString());
      auto* rating = XMLUtils::SetString(
          song, "rating", StringUtils::FormatNumber(m_pDS->fv("rating").get_asFloat()));
      if (rating)
        rating->ToElement()->SetAttribute("max", 10);
      XMLUtils::SetInt(song, "votes", m_pDS->fv("votes").get_asInt());
      auto* userrating = XMLUtils::SetInt(song, "userrating", m_pDS->fv("userrating").get_asInt());
      if (userrating)
        userrating->ToElement()->SetAttribute("max", 10);

      if ((current % 100) == 0 && progressDialog)
      {
        progressDialog->SetLine(1, CVariant{m_pDS->fv("strAlbum").get_asString()});
        progressDialog->SetPercentage(current * 100 / total);
        if (progressDialog->IsCanceled())
        {
          m_pDS->close();
          return false;
        }
      }
      current++;

      m_pDS->next();
    }
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return false;
}

void CMusicDatabase::ImportFromXML(const std::string& xmlFile, CGUIDialogProgress* progressDialog)
{
  try
  {
    if (nullptr == m_pDB)
      return;
    if (nullptr == m_pDS)
      return;

    CXBMCTinyXML xmlDoc;
    if (!xmlDoc.LoadFile(xmlFile) && progressDialog)
    {
      HELPERS::ShowOKDialogLines(CVariant{20197}, CVariant{38354}); //"Unable to read xml file"
      return;
    }

    TiXmlElement* root = xmlDoc.RootElement();
    if (!root)
      return;

    TiXmlElement* entry = root->FirstChildElement();
    int current = 0;
    int total = 0;
    int songtotal = 0;
    // Count the number of artists, albums and songs
    while (entry)
    {
      if (StringUtils::CompareNoCase(entry->Value(), "artist", 6) == 0 ||
          StringUtils::CompareNoCase(entry->Value(), "album", 5) == 0)
        total++;
      else if (StringUtils::CompareNoCase(entry->Value(), "song", 4) == 0)
        songtotal++;

      entry = entry->NextSiblingElement();
    }

    BeginTransaction();
    entry = root->FirstChildElement();
    while (entry)
    {
      std::string strTitle;
      if (StringUtils::CompareNoCase(entry->Value(), "artist", 6) == 0)
      {
        CArtist importedArtist;
        importedArtist.Load(entry);
        strTitle = importedArtist.strArtist;

        // Match by mbid first (that is definatively unique), then name (no mbid), finally by just name
        int idArtist = GetArtistByMatch(importedArtist);
        if (idArtist > -1)
        {
          CArtist artist;
          GetArtist(idArtist, artist, true); // include discography
          artist.MergeScrapedArtist(importedArtist, true);
          UpdateArtist(artist);
        }
        else
          CLog::LogF(LOGDEBUG, "Not import additional artist data as {} not found",
                     importedArtist.strArtist);
        current++;
      }
      else if (StringUtils::CompareNoCase(entry->Value(), "album", 5) == 0)
      {
        CAlbum importedAlbum;
        importedAlbum.Load(entry);
        strTitle = importedAlbum.strAlbum;
        // Match by mbid first (that is definatively unique), then title and artist desc (no mbid), finally by just name and artist
        int idAlbum = GetAlbumByMatch(importedAlbum);
        if (idAlbum > -1)
        {
          CAlbum album;
          GetAlbum(idAlbum, album, true);
          album.MergeScrapedAlbum(importedAlbum, true);
          UpdateAlbum(album); //Will replace song artists if present in xml
        }
        else
          CLog::LogF(LOGDEBUG, "Not import additional album data as {} not found",
                     importedAlbum.strAlbum);

        current++;
      }
      entry = entry->NextSiblingElement();
      if (progressDialog && total)
      {
        progressDialog->SetPercentage(current * 100 / total);
        progressDialog->SetLine(2, CVariant{std::move(strTitle)});
        progressDialog->Progress();
        if (progressDialog->IsCanceled())
        {
          RollbackTransaction();
          return;
        }
      }
    }
    CommitTransaction();

    // Import song playback history <song> entries found
    if (songtotal > 0)
      if (!ImportSongHistory(xmlFile, songtotal, progressDialog))
        return;

    CGUIComponent* gui = CServiceBroker::GetGUI();
    if (gui)
      gui->GetInfoManager().GetInfoProviders().GetLibraryInfoProvider().ResetLibraryBools();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
    RollbackTransaction();
  }
  if (progressDialog)
    progressDialog->Close();
}

bool CMusicDatabase::ImportSongHistory(const std::string& xmlFile,
                                       const int total,
                                       CGUIDialogProgress* progressDialog)
{
  if (!m_pDS)
    return false;

  bool bHistSongExists = false;
  try
  {
    CXBMCTinyXML xmlDoc;
    if (!xmlDoc.LoadFile(xmlFile))
      return false;

    TiXmlElement* root = xmlDoc.RootElement();
    if (!root)
      return false;

    if (progressDialog)
    {
      progressDialog->SetLine(1, CVariant{38350}); //"Importing song playback history"
      progressDialog->SetLine(2, CVariant{""});
    }

    // As can be many songs do in db, not song at a time which would be slow
    // Convert xml entries into a SQL bulk insert statement
    std::string strSQL;
    int current = 0;
    TiXmlElement* entry = root->FirstChildElement();
    while (entry)
    {
      std::string strArtistDisp;
      std::string strTitle;
      int iTrack;
      std::string strFilename;
      std::string strMusicBrainzTrackID;
      std::string strAlbum;
      std::string strMusicBrainzAlbumID;
      std::string strAlbumArtistDisp;
      int iTimesplayed;
      std::string lastplayed;
      int iUserrating = 0;
      float fRating = 0.0;
      int iVotes;
      std::string strSQLSong;
      if (StringUtils::CompareNoCase(entry->Value(), "song", 4) == 0)
      {
        XMLUtils::GetString(entry, "artistdesc", strArtistDisp);
        XMLUtils::GetString(entry, "title", strTitle);
        XMLUtils::GetInt(entry, "track", iTrack);
        XMLUtils::GetString(entry, "filename", strFilename);
        XMLUtils::GetString(entry, "musicbrainztrackid", strMusicBrainzTrackID);
        XMLUtils::GetString(entry, "albumtitle", strAlbum);
        XMLUtils::GetString(entry, "musicbrainzalbumid", strMusicBrainzAlbumID);
        XMLUtils::GetString(entry, "albumartistdesc", strAlbumArtistDisp);
        XMLUtils::GetInt(entry, "timesplayed", iTimesplayed);
        XMLUtils::GetString(entry, "lastplayed", lastplayed);
        const TiXmlElement* rElement = entry->FirstChildElement("rating");
        if (rElement)
        {
          float rating = 0;
          float max_rating = 10;
          XMLUtils::GetFloat(entry, "rating", rating);
          if (rElement->QueryFloatAttribute("max", &max_rating) == TIXML_SUCCESS && max_rating >= 1)
            rating *= (10.f / max_rating); // Normalise the value to between 0 and 10
          if (rating > 10.f)
            rating = 10.f;
          fRating = rating;
        }
        XMLUtils::GetInt(entry, "votes", iVotes);
        const TiXmlElement* userrating = entry->FirstChildElement("userrating");
        if (userrating)
        {
          float rating = 0;
          float max_rating = 10;
          XMLUtils::GetFloat(entry, "userrating", rating);
          if (userrating->QueryFloatAttribute("max", &max_rating) == TIXML_SUCCESS &&
              max_rating >= 1)
            rating *= (10.f / max_rating); // Normalise the value to between 0 and 10
          if (rating > 10.f)
            rating = 10.f;
          iUserrating = MathUtils::round_int(static_cast<double>(rating));
        }

        strSQLSong = PrepareSQL("(%d, %d, ", current + 1, iTrack);
        strSQLSong += PrepareSQL("'%s', '%s', '%s', ", strArtistDisp.c_str(), strTitle.c_str(),
                                 strFilename.c_str());
        if (strMusicBrainzTrackID.empty())
          strSQLSong += PrepareSQL("NULL, ");
        else
          strSQLSong += PrepareSQL("'%s', ", strMusicBrainzTrackID.c_str());
        strSQLSong += PrepareSQL("'%s', '%s', ", strAlbum.c_str(), strAlbumArtistDisp.c_str());
        if (strMusicBrainzAlbumID.empty())
          strSQLSong += PrepareSQL("NULL, ");
        else
          strSQLSong += PrepareSQL("'%s', ", strMusicBrainzAlbumID.c_str());
        strSQLSong += PrepareSQL("%d, ", iTimesplayed);
        if (lastplayed.empty())
          strSQLSong += PrepareSQL("NULL, ");
        else
          strSQLSong += PrepareSQL("'%s', ", lastplayed.c_str());
        strSQLSong +=
            PrepareSQL("%.1f, %d, %d, -1, -1)", static_cast<double>(fRating), iVotes, iUserrating);

        if (current > 0)
          strSQLSong = ", " + strSQLSong;
        strSQL += strSQLSong;
        current++;
      }

      entry = entry->NextSiblingElement();

      if ((current % 100) == 0 && progressDialog)
      {
        progressDialog->SetPercentage(current * 100 / total);
        progressDialog->SetLine(3, CVariant{std::move(strTitle)});
        progressDialog->Progress();
        if (progressDialog->IsCanceled())
          return false;
      }
    }

    CLog::Log(LOGINFO, "Create temporary HistSong table and insert {} records", total);
    /* Can not use CREATE TEMPORARY TABLE as MySQL does not support updates of
       song table using correlated subqueries to a temp table. An updatable join
       to temp table would work in MySQL but SQLite not support updatable joins.
    */
    m_pDS->exec("CREATE TABLE HistSong ("
                "idSongSrc INTEGER primary key, "
                "strAlbum varchar(256), "
                "strMusicBrainzAlbumID text, "
                "strAlbumArtistDisp text, "
                "strArtistDisp text, strTitle varchar(512), "
                "iTrack INTEGER, strFileName text, strMusicBrainzTrackID text, "
                "iTimesPlayed INTEGER, lastplayed varchar(20) default NULL, "
                "rating FLOAT NOT NULL DEFAULT 0, votes INTEGER NOT NULL DEFAULT 0, "
                "userrating INTEGER NOT NULL DEFAULT 0, "
                "idAlbum INTEGER, idSong INTEGER)");
    bHistSongExists = true;

    strSQL = "INSERT INTO HistSong (idSongSrc, iTrack, strArtistDisp, strTitle, "
             "strFileName, strMusicBrainzTrackID, "
             "strAlbum, strAlbumArtistDisp, strMusicBrainzAlbumID, "
             " iTimesPlayed, lastplayed, rating, votes, userrating, idAlbum, idSong) VALUES " +
             strSQL;
    m_pDS->exec(strSQL);

    if (progressDialog)
    {
      progressDialog->SetLine(2, CVariant{38351}); //"Matching data"
      progressDialog->SetLine(3, CVariant{""});
      progressDialog->Progress();
      if (progressDialog->IsCanceled())
      {
        m_pDS->exec("DROP TABLE HistSong");
        return false;
      }
    }

    BeginTransaction();
    // Match albums first on mbid then artist string and album title, setting idAlbum
    // mbid is unique so subquery can only return one result at most
    strSQL = "UPDATE HistSong "
             "SET idAlbum = (SELECT album.idAlbum FROM album "
             "WHERE album.strMusicBrainzAlbumID = HistSong.strMusicBrainzAlbumID) "
             "WHERE EXISTS(SELECT 1 FROM album "
             "WHERE album.strMusicBrainzAlbumID = HistSong.strMusicBrainzAlbumID) AND idAlbum < 0";
    m_pDS->exec(strSQL);

    // Can only be one album with same title and artist(s) and no mbid.
    // But could have 2 releases one with and one without mbid, match up those without mbid
    strSQL = "UPDATE HistSong "
             "SET idAlbum = (SELECT album.idAlbum FROM album "
             "WHERE HistSong.strAlbumArtistDisp = album.strArtistDisp "
             "AND HistSong.strAlbum = album.strAlbum "
             "AND album.strMusicBrainzAlbumID IS NULL "
             "AND HistSong.strMusicBrainzAlbumID IS NULL) "
             "WHERE EXISTS(SELECT 1 FROM album "
             "WHERE HistSong.strAlbumArtistDisp = album.strArtistDisp "
             "AND HistSong.strAlbum = album.strAlbum "
             "AND album.strMusicBrainzAlbumID IS NULL "
             "AND HistSong.strMusicBrainzAlbumID IS NULL) "
             "AND idAlbum < 0";
    m_pDS->exec(strSQL);

    // Try match rest by title and artist(s), prioritise one without mbid
    // Target could have multiple releases - with mbid (non-matching) or one without mbid
    strSQL = "UPDATE HistSong "
             "SET idAlbum = (SELECT album.idAlbum FROM album "
             "WHERE HistSong.strAlbumArtistDisp = album.strArtistDisp "
             "AND HistSong.strAlbum = album.strAlbum "
             "ORDER BY album.strMusicBrainzAlbumID LIMIT 1) "
             "WHERE EXISTS(SELECT 1 FROM album "
             "WHERE HistSong.strAlbumArtistDisp = album.strArtistDisp "
             "AND HistSong.strAlbum = album.strAlbum) "
             "AND idAlbum < 0";
    m_pDS->exec(strSQL);
    if (progressDialog)
    {
      progressDialog->Progress();
      if (progressDialog->IsCanceled())
      {
        RollbackTransaction();
        m_pDS->exec("DROP TABLE HistSong");
        return false;
      }
    }

    // Match songs on first on idAlbum, track and mbid, then idAlbum, track and title, setting idSong
    strSQL = "UPDATE HistSong "
             "SET idSong = (SELECT idsong FROM song "
             "WHERE HistSong.idAlbum = song.idAlbum AND "
             "HistSong.iTrack = song.iTrack AND "
             "HistSong.strMusicBrainzTrackID = song.strMusicBrainzTrackID) "
             "WHERE EXISTS(SELECT 1 FROM song "
             "WHERE HistSong.idAlbum = song.idAlbum AND "
             "HistSong.iTrack = song.iTrack AND "
             "HistSong.strMusicBrainzTrackID = song.strMusicBrainzTrackID) AND idSong < 0";
    m_pDS->exec(strSQL);

    // An album can have more than one song with same track and title (although idAlbum, track and
    // title is often unique), but not using filename as an identifier to allow for import of song
    // history for renamed files. It is about song playback not file playback.
    // Pick the first
    strSQL = "UPDATE HistSong "
             "SET idSong = (SELECT idsong FROM song "
             "WHERE HistSong.idAlbum = song.idAlbum AND "
             "HistSong.iTrack = song.iTrack AND HistSong.strTitle = song.strTitle LIMIT 1) "
             "WHERE EXISTS(SELECT 1 FROM song "
             "WHERE HistSong.idAlbum = song.idAlbum AND "
             "HistSong.iTrack = song.iTrack AND HistSong.strTitle = song.strTitle) AND idSong < 0";
    m_pDS->exec(strSQL);

    CommitTransaction();
    if (progressDialog)
    {
      progressDialog->Progress();
      if (progressDialog->IsCanceled())
      {
        m_pDS->exec("DROP TABLE HistSong");
        return false;
      }
    }

    // Create an index to speed up the updates
    m_pDS->exec("CREATE INDEX idxHistSong ON HistSong(idSong)");

    // Log how many songs matched
    const int unmatched =
        GetSingleValueInt("SELECT COUNT(1) FROM HistSong WHERE idSong < 0", *m_pDS);
    CLog::Log(LOGINFO, "Importing song history {} of {} songs matched", total - unmatched, total);

    if (progressDialog)
    {
      progressDialog->SetLine(2, CVariant{38352}); //"Updating song playback history"
      progressDialog->Progress();
      if (progressDialog->IsCanceled())
      {
        m_pDS->exec("DROP TABLE HistSong"); // Drops index too
        return false;
      }
    }

    /* Update song table using the song ids we have matched.
      Use correlated subqueries as SQLite does not support updatable joins.
      MySQL requires HistSong table not to be defined temporary for this.
    */

    BeginTransaction();
    // Times played and last played date(when count is greater)
    strSQL = "UPDATE song SET iTimesPlayed = "
             "(SELECT iTimesPlayed FROM HistSong WHERE HistSong.idSong = song.idSong), "
             "lastplayed = "
             "(SELECT lastplayed FROM HistSong WHERE HistSong.idSong = song.idSong) "
             "WHERE EXISTS(SELECT 1 FROM HistSong WHERE "
             "HistSong.idSong = song.idSong AND HistSong.iTimesPlayed > song.iTimesPlayed)";
    m_pDS->exec(strSQL);

    // User rating
    strSQL = "UPDATE song SET userrating = "
             "(SELECT userrating FROM HistSong WHERE HistSong.idSong = song.idSong) "
             "WHERE EXISTS(SELECT 1 FROM HistSong WHERE "
             "HistSong.idSong = song.idSong AND HistSong.userrating > 0)";
    m_pDS->exec(strSQL);

    // Rating and votes
    strSQL = "UPDATE song SET rating = "
             "(SELECT rating FROM HistSong WHERE HistSong.idSong = song.idSong), "
             "votes = "
             "(SELECT votes FROM HistSong WHERE HistSong.idSong = song.idSong) "
             "WHERE  EXISTS(SELECT 1 FROM HistSong WHERE "
             "HistSong.idSong = song.idSong AND HistSong.rating > 0)";
    m_pDS->exec(strSQL);

    if (progressDialog)
    {
      progressDialog->Progress();
      if (progressDialog->IsCanceled())
      {
        RollbackTransaction();
        m_pDS->exec("DROP TABLE HistSong");
        return false;
      }
    }
    CommitTransaction();

    // Tidy up temp table (index also removed)
    m_pDS->exec("DROP TABLE HistSong");
    // Compact db to recover space as had to add/drop actual table
    if (progressDialog)
    {
      progressDialog->SetLine(2, CVariant{331});
      progressDialog->Progress();
    }
    Compress(false);

    // Write event log entry
    // "Importing song history {1} of {2} songs matched", total - unmatched, total)
    std::string strLine =
        StringUtils::Format(CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(38353),
                            total - unmatched, total);

    auto eventLog = CServiceBroker::GetEventLog();
    if (eventLog)
      eventLog->Add(EventPtr(new CNotificationEvent(20197, strLine, EventLevel::Information)));

    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
    RollbackTransaction();
    if (bHistSongExists)
      m_pDS->exec("DROP TABLE HistSong");
  }
  return false;
}

void CMusicDatabase::SetPropertiesFromArtist(CFileItem& item, const CArtist& artist)
{
  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  item.SetProperty("artist_sortname", artist.strSortName);
  item.SetProperty("artist_type", artist.strType);
  item.SetProperty("artist_gender", artist.strGender);
  item.SetProperty("artist_disambiguation", artist.strDisambiguation);
  item.SetProperty("artist_instrument", StringUtils::Join(artist.instruments, itemSeparator));
  item.SetProperty("artist_instrument_array", artist.instruments);
  item.SetProperty("artist_style", StringUtils::Join(artist.styles, itemSeparator));
  item.SetProperty("artist_style_array", artist.styles);
  item.SetProperty("artist_mood", StringUtils::Join(artist.moods, itemSeparator));
  item.SetProperty("artist_mood_array", artist.moods);
  item.SetProperty("artist_born", artist.strBorn);
  item.SetProperty("artist_formed", artist.strFormed);
  item.SetProperty("artist_description", artist.strBiography);
  item.SetProperty("artist_genre", StringUtils::Join(artist.genre, itemSeparator));
  item.SetProperty("artist_genre_array", artist.genre);
  item.SetProperty("artist_died", artist.strDied);
  item.SetProperty("artist_disbanded", artist.strDisbanded);
  item.SetProperty("artist_yearsactive", StringUtils::Join(artist.yearsActive, itemSeparator));
  item.SetProperty("artist_yearsactive_array", artist.yearsActive);
}

void CMusicDatabase::SetPropertiesFromAlbum(CFileItem& item, const CAlbum& album)
{
  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  item.SetProperty("album_description", album.strReview);
  item.SetProperty("album_theme", StringUtils::Join(album.themes, itemSeparator));
  item.SetProperty("album_theme_array", album.themes);
  item.SetProperty("album_mood", StringUtils::Join(album.moods, itemSeparator));
  item.SetProperty("album_mood_array", album.moods);
  item.SetProperty("album_style", StringUtils::Join(album.styles, itemSeparator));
  item.SetProperty("album_style_array", album.styles);
  item.SetProperty("album_type", album.strType);
  item.SetProperty("album_label", album.strLabel);
  item.SetProperty("album_artist", album.GetAlbumArtistString());
  item.SetProperty("album_artist_array", album.GetAlbumArtist());
  item.SetProperty("album_genre", StringUtils::Join(album.genre, itemSeparator));
  item.SetProperty("album_genre_array", album.genre);
  item.SetProperty("album_title", album.strAlbum);
  if (album.fRating > 0)
    item.SetProperty("album_rating", StringUtils::FormatNumber(album.fRating));
  if (album.iUserrating > 0)
    item.SetProperty("album_userrating", album.iUserrating);
  if (album.iVotes > 0)
    item.SetProperty("album_votes", album.iVotes);

  item.SetProperty("album_isboxset", album.bBoxedSet);
  item.SetProperty("album_totaldiscs", album.iTotalDiscs);
  item.SetProperty("album_releasetype", CAlbum::ReleaseTypeToString(album.releaseType));
  item.SetProperty("album_duration",
                   StringUtils::SecondsToTimeString(album.iAlbumDuration, TIME_FORMAT_GUESS));
}

void CMusicDatabase::SetPropertiesForFileItem(CFileItem& item)
{
  if (!item.HasMusicInfoTag())
    return;
  // May already have song artist ids as item property set when data read from
  // db, but check property is valid array (scripts could set item properties
  // incorrectly), otherwise try to fetch artist by name.
  int idArtist = -1;
  if (item.HasProperty("artistid") && item.GetProperty("artistid").isArray())
  {
    CVariant::const_iterator_array varid = item.GetProperty("artistid").begin_array();
    idArtist = static_cast<int>(varid->asInteger());
  }
  else
    idArtist = GetArtistByName(item.GetMusicInfoTag()->GetArtistString());
  if (idArtist > -1)
  {
    CArtist artist;
    if (GetArtist(idArtist, artist))
      SetPropertiesFromArtist(item, artist);
  }
  int idAlbum = item.GetMusicInfoTag()->GetAlbumId();
  if (idAlbum <= 0)
    idAlbum = GetAlbumByName(item.GetMusicInfoTag()->GetAlbum(),
                             item.GetMusicInfoTag()->GetArtistString());
  if (idAlbum > -1)
  {
    CAlbum album;
    if (GetAlbum(idAlbum, album, false))
      SetPropertiesFromAlbum(item, album);
  }
}

void CMusicDatabase::SetItemUpdated(int mediaId, const std::string& mediaType)
{
  std::string strSQL;
  try
  {
    if (mediaType != MediaTypeArtist && mediaType != MediaTypeAlbum && mediaType != MediaTypeSong)
      return;
    if (nullptr == m_pDB)
      return;
    if (nullptr == m_pDS)
      return;

    // Fire AFTER UPDATE db trigger on artist, album or song table to set datemodified field
    // e.g. when artwork for item is changed from info dialog but not item details.
    // Use SQL UPDATE that does not change record data.
    if (mediaType == MediaTypeArtist)
      strSQL = PrepareSQL("UPDATE artist SET strArtist = strArtist WHERE idArtist = %i", mediaId);
    else if (mediaType == MediaTypeAlbum)
      strSQL = PrepareSQL("UPDATE album SET strAlbum = strAlbum WHERE idAlbum = %i", mediaId);
    else // MediaTypeSong
      strSQL = PrepareSQL("UPDATE song SET strTitle = strTitle WHERE idSong = %i", mediaId);
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}, {}) - failed to execute {}", mediaId, mediaType, strSQL);
  }
}

void CMusicDatabase::SetArtForItem(int mediaId,
                                   const std::string& mediaType,
                                   const KODI::ART::Artwork& art)
{
  for (const auto& [type, url] : art)
    SetArtForItem(mediaId, mediaType, type, url);
}

void CMusicDatabase::SetArtForItem(int mediaId,
                                   const std::string& mediaType,
                                   const std::string& artType,
                                   const std::string& url)
{
  try
  {
    if (nullptr == m_pDB)
      return;
    if (nullptr == m_pDS)
      return;

    // don't set <foo>.<bar> art types - these are derivative types from parent items
    if (artType.find('.') != std::string::npos)
      return;

    std::string sql = PrepareSQL("SELECT art_id FROM art "
                                 "WHERE media_id=%i AND media_type='%s' AND type='%s'",
                                 mediaId, mediaType.c_str(), artType.c_str());
    m_pDS->query(sql);
    if (!m_pDS->eof())
    { // update
      int artId = m_pDS->fv(0).get_asInt();
      m_pDS->close();
      sql = PrepareSQL("UPDATE art SET url='%s' where art_id=%d", url.c_str(), artId);
      m_pDS->exec(sql);
    }
    else
    { // insert
      m_pDS->close();
      sql = PrepareSQL("INSERT INTO art(media_id, media_type, type, url) "
                       "VALUES (%d, '%s', '%s', '%s')",
                       mediaId, mediaType.c_str(), artType.c_str(), url.c_str());
      m_pDS->exec(sql);
    }
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}, '{}', '{}', '{}') failed", mediaId, mediaType, artType, url);
  }
}

bool CMusicDatabase::GetArtForItem(
    int songId, int albumId, int artistId, bool bPrimaryArtist, std::vector<ArtForThumbLoader>& art)
{
  std::string strSQL;
  try
  {
    if (!(songId > 0 || albumId > 0 || artistId > 0))
      return false;
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS2)
      return false; // using dataset 2 as we're likely called in loops on dataset 1

    Filter filter;
    if (songId > 0)
      filter.AppendWhere(PrepareSQL("media_id = %i AND media_type ='%s'", songId, MediaTypeSong));
    if (albumId > 0)
      filter.AppendWhere(PrepareSQL("media_id = %i AND media_type ='%s'", albumId, MediaTypeAlbum),
                         false);
    if (artistId > 0)
      filter.AppendWhere(
          PrepareSQL("media_id = %i AND media_type ='%s'", artistId, MediaTypeArtist), false);

    strSQL = "SELECT DISTINCT art_id, media_id, media_type, type, '' as prefix, url, 0 as iorder "
             "FROM art";
    if (!BuildSQL(strSQL, filter, strSQL))
      return false;

    if (!(artistId > 0))
    {
      // Artist ID unknown, so lookup album artist for albums and songs
      std::string strSQL2;
      if (albumId > 0)
      {
        //Album ID known, so use it to look up album artist(s)
        strSQL2 = PrepareSQL(
            "SELECT art_id, media_id, media_type, type, 'albumartist' as prefix, "
            "url, album_artist.iOrder as iorder FROM art "
            "JOIN album_artist ON art.media_id = album_artist.idArtist AND art.media_type ='%s' "
            "WHERE album_artist.idAlbum = %i ",
            MediaTypeArtist, albumId);
        if (bPrimaryArtist)
          strSQL2 += "AND album_artist.iOrder = 0";

        strSQL = strSQL + " UNION " + strSQL2;
      }
      if (songId > 0)
      {
        if (albumId < 0)
        {
          //Album ID unknown, so get from song to look up album artist(s)
          strSQL2 = PrepareSQL(
              "SELECT art_id, media_id, media_type, type, 'albumartist' as prefix, "
              "url, album_artist.iOrder as iorder FROM art "
              "JOIN album_artist ON art.media_id = album_artist.idArtist AND art.media_type ='%s' "
              "JOIN song ON song.idAlbum = album_artist.idAlbum  "
              "WHERE song.idSong = %i ",
              MediaTypeArtist, songId);
          if (bPrimaryArtist)
            strSQL2 += "AND album_artist.iOrder = 0";

          strSQL = strSQL + " UNION " + strSQL2;
        }

        // Artist ID unknown, so lookup artist for songs (could be different from album artist)
        strSQL2 = PrepareSQL(
            "SELECT art_id, media_id, media_type, type, 'artist' as prefix, "
            "url, song_artist.iOrder as iorder FROM art "
            "JOIN song_artist on art.media_id = song_artist.idArtist AND art.media_type = '%s' "
            "WHERE song_artist.idsong = %i AND song_artist.idRole = %i ",
            MediaTypeArtist, songId, ROLE_ARTIST);
        if (bPrimaryArtist)
          strSQL2 += "AND song_artist.iOrder = 0";

        strSQL = strSQL + " UNION " + strSQL2;
      }
    }
    if (songId > 0 && albumId < 0)
    {
      //Album ID unknown, so get from song to look up album art
      std::string strSQL2;
      strSQL2 = PrepareSQL("SELECT art_id, media_id, media_type, type, '' as prefix, "
                           "url, 0 as iorder FROM art "
                           "JOIN song ON art.media_id = song.idAlbum AND art.media_type ='%s' "
                           "WHERE song.idSong = %i ",
                           MediaTypeAlbum, songId);
      strSQL = strSQL + " UNION " + strSQL2;
    }

    m_pDS2->query(strSQL);
    while (!m_pDS2->eof())
    {
      ArtForThumbLoader artitem;
      artitem.artType = m_pDS2->fv("type").get_asString();
      artitem.mediaType = m_pDS2->fv("media_type").get_asString();
      artitem.prefix = m_pDS2->fv("prefix").get_asString();
      artitem.url = m_pDS2->fv("url").get_asString();
      int iOrder = m_pDS2->fv("iorder").get_asInt();
      // Add order to prefix for multiple artist art for songs and albums e.g. "albumartist2"
      if (iOrder > 0)
        artitem.prefix += m_pDS2->fv("iorder").get_asString();

      art.emplace_back(artitem);
      m_pDS2->next();
    }
    m_pDS2->close();
    return !art.empty();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", strSQL);
  }
  return false;
}

bool CMusicDatabase::GetArtForItem(int mediaId,
                                   const std::string& mediaType,
                                   KODI::ART::Artwork& art)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS2)
      return false; // using dataset 2 as we're likely called in loops on dataset 1

    std::string sql = PrepareSQL("SELECT type,url FROM art WHERE media_id=%i AND media_type='%s'",
                                 mediaId, mediaType.c_str());
    m_pDS2->query(sql);
    while (!m_pDS2->eof())
    {
      art.try_emplace(m_pDS2->fv(0).get_asString(), m_pDS2->fv(1).get_asString());
      m_pDS2->next();
    }
    m_pDS2->close();
    return !art.empty();
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", mediaId);
  }
  return false;
}

std::string CMusicDatabase::GetArtForItem(int mediaId,
                                          const std::string& mediaType,
                                          const std::string& artType)
{
  if (!m_pDS2)
    return {};

  std::string query = PrepareSQL("SELECT url FROM art "
                                 "WHERE media_id=%i AND media_type='%s' AND type='%s'",
                                 mediaId, mediaType.c_str(), artType.c_str());
  return GetSingleValue(query, *m_pDS2);
}

bool CMusicDatabase::RemoveArtForItem(int mediaId,
                                      const MediaType& mediaType,
                                      const std::string& artType)
{
  return ExecuteQuery(PrepareSQL("DELETE FROM art "
                                 "WHERE media_id=%i AND media_type='%s' AND type='%s'",
                                 mediaId, mediaType.c_str(), artType.c_str()));
}

bool CMusicDatabase::RemoveArtForItem(int mediaId,
                                      const MediaType& mediaType,
                                      const std::set<std::string, std::less<>>& artTypes)
{
  bool result = true;
  for (const auto& i : artTypes)
    result &= RemoveArtForItem(mediaId, mediaType, i);

  return result;
}

bool CMusicDatabase::GetArtTypes(const MediaType& mediaType, std::vector<std::string>& artTypes)
{
  try
  {
    if (nullptr == m_pDB)
      return false;
    if (nullptr == m_pDS)
      return false;

    std::string strSQL =
        PrepareSQL("SELECT DISTINCT type FROM art WHERE media_type='%s'", mediaType.c_str());

    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_pDS->close();
      return false;
    }

    while (!m_pDS->eof())
    {
      artTypes.emplace_back(m_pDS->fv(0).get_asString());
      m_pDS->next();
    }
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "({}) failed", mediaType);
  }
  return false;
}

std::vector<std::string> CMusicDatabase::GetAvailableArtTypesForItem(int mediaId,
                                                                     const MediaType& mediaType)
{
  CScraperUrl thumbURL;
  if (mediaType == MediaTypeArtist)
  {
    CArtist artist;
    if (GetArtist(mediaId, artist))
      thumbURL = artist.thumbURL;
  }
  else if (mediaType == MediaTypeAlbum)
  {
    CAlbum album;
    if (GetAlbum(mediaId, album))
      thumbURL = album.thumbURL;
  }

  std::vector<std::string> result;
  for (const auto& urlEntry : thumbURL.GetUrls())
  {
    std::string artType = urlEntry.m_aspect;
    if (artType.empty())
      artType = "thumb";
    if (std::ranges::find(result, artType) == result.end())
      result.push_back(artType);
  }
  return result;
}

std::vector<CScraperUrl::SUrlEntry> CMusicDatabase::GetAvailableArtForItem(
    int mediaId, const MediaType& mediaType, const std::string& artType)
{
  CScraperUrl thumbURL;
  if (mediaType == MediaTypeArtist)
  {
    CArtist artist;
    if (GetArtist(mediaId, artist))
      thumbURL = artist.thumbURL;
  }
  else if (mediaType == MediaTypeAlbum)
  {
    CAlbum album;
    if (GetAlbum(mediaId, album))
      thumbURL = album.thumbURL;
  }

  std::vector<CScraperUrl::SUrlEntry> result;
  for (auto urlEntry : thumbURL.GetUrls())
  {
    if (urlEntry.m_aspect.empty())
      urlEntry.m_aspect = "thumb";
    if (artType.empty() || urlEntry.m_aspect == artType)
      result.push_back(urlEntry);
  }
  return result;
}

int CMusicDatabase::GetOrderFilter(const std::string& type,
                                 const SortDescription& sorting,
                                 Filter& filter) const
{
  return CMusicQueryBuilder::GetOrderFilter(type, sorting, filter, *this);
}


bool CMusicDatabase::GetFilter(CDbUrl& musicUrl, Filter& filter, SortDescription& sorting)
{
  return CMusicQueryBuilder::GetFilter(musicUrl, filter, sorting, *this);
}


std::string CMusicDatabase::GetMediaDateFromFile(const std::string& strFileNameAndPath) const
{
  if (strFileNameAndPath.empty())
    return std::string();

  CDateTime dateMedia;
  int code;
  code = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_iMusicLibraryDateAdded;
  // 1 using the files mtime (if valid) and only using the ctime if the mtime isn't valid
  if (code == 1)
    dateMedia = CFileUtils::GetModificationDate(0, strFileNameAndPath);
  //2 using the newer datetime of the file's mtime and ctime
  else if (code == 2)
    dateMedia = CFileUtils::GetModificationDate(1, strFileNameAndPath);
  //3 using the older datetime of the file's mtime and ctime
  else if (code == 3)
    dateMedia = CFileUtils::GetModificationDate(2, strFileNameAndPath);
  //0 using the current datetime if none of the above matches or one returns an invalid datetime
  if (!dateMedia.IsValid())
    dateMedia = CDateTime::GetCurrentDateTime();

  return dateMedia.GetAsDBDateTime();
}

bool CMusicDatabase::AddAudioBook(const CFileItem& item)
{
  auto const& artists = item.GetMusicInfoTag()->GetArtist();
  std::string strSQL = PrepareSQL(
      "INSERT INTO audiobook (idBook,strBook,strAuthor,bookmark,file,dateAdded) "
      "VALUES (NULL,'%s','%s',%i,'%s','%s')",
      item.GetMusicInfoTag()->GetAlbum().c_str(), artists.empty() ? "" : artists[0].c_str(), 0,
      item.GetDynPath().c_str(), CDateTime::GetCurrentDateTime().GetAsDBDateTime().c_str());
  return ExecuteQuery(strSQL);
}

bool CMusicDatabase::SetResumeBookmarkForAudioBook(const CFileItem& item, int bookmark)
{
  std::string strSQL = PrepareSQL("SELECT bookmark FROM audiobook "
                                  "WHERE file='%s'",
                                  item.GetDynPath().c_str());
  if (!m_pDS->query(strSQL) || m_pDS->num_rows() == 0)
  {
    if (!AddAudioBook(item))
      return false;
  }

  strSQL = PrepareSQL("UPDATE audiobook SET bookmark=%i "
                      "WHERE file='%s'",
                      bookmark, item.GetDynPath().c_str());

  return ExecuteQuery(strSQL);
}

bool CMusicDatabase::GetResumeBookmarkForAudioBook(const CFileItem& item, int& bookmark)
{
  std::string strSQL =
      PrepareSQL("SELECT bookmark FROM audiobook WHERE file='%s'", item.GetDynPath().c_str());
  if (!m_pDS->query(strSQL) || m_pDS->num_rows() == 0)
    return false;

  bookmark = m_pDS->fv(0).get_asInt();
  return true;
}

std::vector<std::string> CMusicDatabase::GetUsedImages(
    const std::vector<std::string>& imagesToCheck) const
{
  try
  {
    if (!m_pDB || !m_pDS)
      return imagesToCheck;

    if (imagesToCheck.empty())
      return {};

    int artworkLevel = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(
        CSettings::SETTING_MUSICLIBRARY_ARTWORKLEVEL);
    if (artworkLevel == CSettings::MUSICLIBRARY_ARTWORK_LEVEL_NONE)
    {
      return {};
    }

    std::string sql = "SELECT DISTINCT url FROM art WHERE url IN (";
    for (const auto& image : imagesToCheck)
    {
      sql += PrepareSQL("'%s',", image.c_str());
    }
    sql.pop_back(); // remove last ','
    sql += ")";

    // add arttype filters if set to "Basic"
    if (artworkLevel == CSettings::MUSICLIBRARY_ARTWORK_LEVEL_BASIC)
    {
      sql += PrepareSQL(" AND (media_type = 'album' AND type = 'thumb' OR media_type = 'artist' "
                        "AND type IN ('thumb', 'fanart'))");
    }

    if (!m_pDS->query(sql))
      return {};

    std::vector<std::string> result;
    while (!m_pDS->eof())
    {
      result.push_back(m_pDS->fv(0).get_asString());
      m_pDS->next();
    }
    m_pDS->close();

    return result;
  }
  catch (...)
  {
    CLog::LogF(LOGERROR, "failed");
  }
  return {};
}
