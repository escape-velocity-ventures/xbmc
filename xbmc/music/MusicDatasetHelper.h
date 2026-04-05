/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

class CAlbum;
class CArtist;
class CArtistCredit;
class CFileItem;
class CMusicDbUrl;
class CMusicRole;
class CSong;

namespace dbiplus
{
class Dataset;
class field_value;
using sql_record = std::vector<field_value>;
} // namespace dbiplus

// ---------------------------------------------------------------------------
// Field-index enums (column order must match the SQL views)
//
// These were originally private enums inside CMusicDatabase.  They now live at
// namespace scope so that both CMusicDatabase and CMusicDatasetHelper (and
// future repositories) can use them without qualification.
// ---------------------------------------------------------------------------

// Fields should be ordered as they
// appear in the songview
enum SongFields
{
  song_idSong = 0,
  song_strArtists,
  song_strArtistSort,
  song_strGenres,
  song_strTitle,
  song_iTrack,
  song_iDuration,
  song_strReleaseDate,
  song_strOrigReleaseDate,
  song_strDiscSubtitle,
  song_strFileName,
  song_strMusicBrainzTrackID,
  song_iTimesPlayed,
  song_iStartOffset,
  song_iEndOffset,
  song_lastplayed,
  song_rating,
  song_userrating,
  song_votes,
  song_comment,
  song_idAlbum,
  song_strAlbum,
  song_strPath,
  song_strReleaseStatus,
  song_bCompilation,
  song_bBoxedSet,
  song_strAlbumArtists,
  song_strAlbumArtistSort,
  song_strAlbumReleaseType,
  song_mood,
  song_strReplayGain,
  song_iBPM,
  song_iBitRate,
  song_iSampleRate,
  song_iChannels,
  song_songVideoURL,
  song_iAlbumDuration,
  song_iDiscTotal,
  song_dateAdded,
  song_dateNew,
  song_dateModified,
  song_enumCount // end of the enum, do not add past here
};

// Fields should be ordered as they
// appear in the albumview
enum AlbumFields
{
  album_idAlbum = 0,
  album_strAlbum,
  album_strMusicBrainzAlbumID,
  album_strReleaseGroupMBID,
  album_strArtists,
  album_strArtistSort,
  album_strGenres,
  album_strReleaseDate,
  album_strOrigReleaseDate,
  album_bBoxedSet,
  album_strMoods,
  album_strStyles,
  album_strThemes,
  album_strReview,
  album_strLabel,
  album_strType,
  album_strReleaseStatus,
  album_strThumbURL,
  album_fRating,
  album_iUserrating,
  album_iVotes,
  album_bCompilation,
  album_bScrapedMBID,
  album_lastScraped,
  album_dateAdded,
  album_dateNew,
  album_dateModified,
  album_iTimesPlayed,
  album_strReleaseType,
  album_iTotalDiscs,
  album_dtLastPlayed,
  album_iAlbumDuration,
  album_enumCount // end of the enum, do not add past here
};

// Fields should be ordered as they
// appear in the songartistview/albumartistview
enum ArtistCreditFields
{
  // used for GetAlbum to get the cascaded album/song artist credits
  artistCredit_idEntity = 0, // can be idSong or idAlbum depending on context
  artistCredit_idArtist,
  artistCredit_idRole,
  artistCredit_strRole,
  artistCredit_strArtist,
  artistCredit_strSortName,
  artistCredit_strMusicBrainzArtistID,
  artistCredit_iOrder,
  artistCredit_enumCount
};

// Fields should be ordered as they
// appear in the artistview
enum ArtistFields
{
  artist_idArtist = 0,
  artist_strArtist,
  artist_strSortName,
  artist_strMusicBrainzArtistID,
  artist_strType,
  artist_strGender,
  artist_strDisambiguation,
  artist_strBorn,
  artist_strFormed,
  artist_strGenres,
  artist_strMoods,
  artist_strStyles,
  artist_strInstruments,
  artist_strBiography,
  artist_strDied,
  artist_strDisbanded,
  artist_strYearsActive,
  artist_strImage,
  artist_bScrapedMBID,
  artist_lastScraped,
  artist_dateAdded,
  artist_dateNew,
  artist_dateModified,
  artist_enumCount // end of the enum, do not add past here
};

/*!
 \ingroup music
 \brief Static helper class for hydrating music domain objects from dataset records.

 Extracts the "FromDataset" family of methods from CMusicDatabase so they can
 be reused by other repositories (CRUD, Nav) without coupling to the full
 database class.
 */
class CMusicDatasetHelper
{
public:
  static CSong GetSongFromDataset(const dbiplus::sql_record* const record, int offset = 0);

  static CAlbum GetAlbumFromDataset(dbiplus::Dataset* pDS,
                                    int offset = 0,
                                    bool imageURL = false);
  static CAlbum GetAlbumFromDataset(const dbiplus::sql_record* const record,
                                    int offset = 0,
                                    bool imageURL = false);

  static CArtist GetArtistFromDataset(dbiplus::Dataset* pDS,
                                      int offset = 0,
                                      bool needThumb = true,
                                      bool translateBlankArtist = true);
  static CArtist GetArtistFromDataset(const dbiplus::sql_record* const record,
                                      int offset = 0,
                                      bool needThumb = true,
                                      bool translateBlankArtist = true);

  static CArtistCredit GetArtistCreditFromDataset(const dbiplus::sql_record* const record,
                                                  int offset = 0);

  static CMusicRole GetArtistRoleFromDataset(const dbiplus::sql_record* const record,
                                             int offset = 0);

  static void GetFileItemFromDataset(const dbiplus::sql_record* const record,
                                     CFileItem* item,
                                     const CMusicDbUrl& baseUrl);

  static void GetFileItemFromArtistCredits(std::vector<CArtistCredit>& artistCredits,
                                           CFileItem* item);

private:
  CMusicDatasetHelper() = delete;
};
