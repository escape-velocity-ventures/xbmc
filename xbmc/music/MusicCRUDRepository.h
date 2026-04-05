/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

/*!
 \file MusicCRUDRepository.h
 \brief Extracted CRUD operations for Song, Album, and Artist entities.

 CMusicCRUDRepository encapsulates the Add/Get/Update/Delete methods that
 were previously inlined inside CMusicDatabase.  It is declared as a friend
 of CMusicDatabase and accesses dataset pointers (m_pDS, m_pDS2, m_pDB)
 plus helper methods (PrepareSQL, ExecuteQuery, ...) through a held reference.

 Hydration of domain objects from dataset records is delegated to
 CMusicDatasetHelper (static methods).
 */

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class CAlbum;
class CArtist;
class CArtistCredit;
class CDateTime;
class CDiscoAlbum;
class CFileItem;
class CFileItemList;
class CMusicDatabase;
class CMusicRole;
class CSong;
enum class ReleaseType;
class ReplayGain;

/*!
 \ingroup music
 \brief Repository class encapsulating Song/Album/Artist CRUD operations.

 All methods were extracted verbatim from CMusicDatabase.  The class is
 constructed with a reference to its owning CMusicDatabase instance and
 accesses internal state through the friend declaration.
 */
class CMusicCRUDRepository
{
public:
  explicit CMusicCRUDRepository(CMusicDatabase& db);
  ~CMusicCRUDRepository() = default;

  // Non-copyable, non-movable (reference member)
  CMusicCRUDRepository(const CMusicCRUDRepository&) = delete;
  CMusicCRUDRepository& operator=(const CMusicCRUDRepository&) = delete;

  // -----------------------------------------------------------------------
  // Song CRUD
  // -----------------------------------------------------------------------
  int AddSong(const int idSong,
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
              const ReplayGain& replayGain);
  bool GetSong(int idSong, CSong& song);

  bool UpdateSong(CSong& song, bool bArtists = true, bool bArtistLinks = true);
  int UpdateSong(int idSong,
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
                 const std::string& songVideoURL);

  //// Misc Song
  bool GetSongByFileName(const std::string& strFileName, CSong& song, int64_t startOffset = 0);
  bool GetSongsByPath(const std::string& strPath,
                      std::map<std::string, std::vector<CSong>>& songmap,
                      bool bAppendToMap = false);
  bool Search(const std::string& search, CFileItemList& items);
  bool RemoveSongsFromPath(const std::string& path,
                           std::map<std::string, std::vector<CSong>>& songmap,
                           bool exact = true);
  void CheckArtistLinksChanged();
  bool SetSongUserrating(const std::string& filePath, int userrating);
  bool SetSongUserrating(int idSong, int userrating);
  bool SetSongVotes(const std::string& filePath, int votes);
  int GetSongByArtistAndAlbumAndTitle(const std::string& strArtist,
                                      const std::string& strAlbum,
                                      const std::string& strTitle);

  // -----------------------------------------------------------------------
  // Album CRUD
  // -----------------------------------------------------------------------
  bool AddAlbum(CAlbum& album, int idSource);
  bool UpdateAlbum(CAlbum& album);

  int AddAlbum(const std::string& strAlbum,
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
               ReleaseType releaseType);

  bool GetAlbum(int idAlbum, CAlbum& album, bool getSongs = true);
  int UpdateAlbum(int idAlbum,
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
                  bool bScrapedMBID);
  bool ClearAlbumLastScrapedTime(int idAlbum);
  bool HasAlbumBeenScraped(int idAlbum) const;

  //// Misc Album
  int GetAlbumIdByPath(const std::string& path);
  bool GetAlbumFromSong(int idSong, CAlbum& album);
  int GetAlbumByName(const std::string& strAlbum, const std::string& strArtist = "");
  int GetAlbumByName(const std::string& strAlbum, const std::vector<std::string>& artist);
  bool GetMatchingMusicVideoAlbum(const std::string& strAlbum,
                                  const std::string& strArtist,
                                  int& idAlbum,
                                  std::string& strReview);
  bool SearchAlbumsByArtistName(const std::string& strArtist, CFileItemList& items);
  int GetAlbumByMatch(const CAlbum& album);
  std::string GetAlbumById(int id) const;
  std::string GetAlbumDiscTitle(int idAlbum, int idDisc) const;
  bool SetAlbumUserrating(const int idAlbum, int userrating);
  int GetAlbumDiscsCount(int idAlbum) const;

  // -----------------------------------------------------------------------
  // Artist CRUD
  // -----------------------------------------------------------------------
  bool UpdateArtist(const CArtist& artist);

  int AddArtist(const std::string& strArtist,
                const std::string& strMusicBrainzArtistID,
                const std::string& strSortName,
                bool bScrapedMBID = false);
  int AddArtist(const std::string& strArtist,
                const std::string& strMusicBrainzArtistID,
                bool bScrapedMBID = false);
  bool GetArtist(int idArtist, CArtist& artist, bool fetchAll = false);
  bool GetArtistExists(int idArtist);
  int GetLastArtist() const;
  int GetArtistFromMBID(const std::string& strMusicBrainzArtistID, std::string& artistname);
  int UpdateArtist(int idArtist,
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
                   const std::string& strImage);
  bool UpdateArtistScrapedMBID(int idArtist, const std::string& strMusicBrainzArtistID);
  bool HasArtistBeenScraped(int idArtist) const;
  bool ClearArtistLastScrapedTime(int idArtist);
  int AddArtistDiscography(int idArtist, const CDiscoAlbum& discoAlbum);
  bool DeleteArtistDiscography(int idArtist);
  bool GetArtistDiscography(int idArtist, CFileItemList& items);
  bool AddArtistVideoLinks(const CArtist& artist);
  bool DeleteArtistVideoLinks(const int idArtist);

  std::string GetArtistById(int id) const;
  int GetArtistByName(const std::string& strArtist);
  int GetArtistByMatch(const CArtist& artist);
  bool GetArtistFromSong(int idSong, CArtist& artist);
  bool IsSongArtist(int idSong, int idArtist) const;
  bool IsSongAlbumArtist(int idSong, int idArtist) const;
  std::string GetRoleById(int id) const;

  bool UpdateArtistSortNames(int idArtist = -1);

  // -----------------------------------------------------------------------
  // Link tables (tightly coupled with CRUD)
  // -----------------------------------------------------------------------
  bool AddAlbumArtist(int idArtist, int idAlbum, std::string_view strArtist, int iOrder);
  bool GetAlbumsByArtist(int idArtist, std::vector<int>& albums);
  bool GetArtistsByAlbum(int idAlbum, CFileItem* item);
  bool GetArtistsByAlbum(int idAlbum, std::vector<std::string>& artistIDs);
  bool DeleteAlbumArtistsByAlbum(int idAlbum);

  int AddRole(std::string_view strRole);
  bool AddSongArtist(
      int idArtist, int idSong, std::string_view strRole, std::string_view strArtist, int iOrder);
  bool AddSongArtist(int idArtist, int idSong, int idRole, std::string_view strArtist, int iOrder);
  int AddSongContributor(int idSong,
                         const std::string& strRole,
                         const std::string& strArtist,
                         const std::string& strSort);
  void AddSongContributors(int idSong,
                           const std::vector<CMusicRole>& contributors,
                           const std::string& strSort);
  int GetRoleByName(const std::string& strRole);
  bool GetRolesByArtist(int idArtist, CFileItem* item);
  bool GetSongsByArtist(int idArtist, std::vector<int>& songs);
  bool GetArtistsBySong(int idSong, std::vector<int>& artists);
  bool DeleteSongArtistsBySong(int idSong);

  bool AddSongGenres(int idSong, const std::vector<std::string>& genres);
  bool GetGenresBySong(int idSong, std::vector<int>& genres);
  bool GetGenresByAlbum(int idAlbum, CFileItem* item);
  bool GetGenresByArtist(int idArtist, CFileItem* item);
  bool GetIsAlbumArtist(int idArtist, CFileItem* item) const;

private:
  CMusicDatabase& m_db;
};
