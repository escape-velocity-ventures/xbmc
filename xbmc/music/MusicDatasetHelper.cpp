/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicDatasetHelper.h"

#include "Album.h"
#include "Artist.h"
#include "FileItem.h"
#include "ServiceBroker.h"
#include "Song.h"
#include "dbwrappers/dataset.h"
#include "media/MediaType.h"
#include "music/MusicDbUrl.h"
#include "music/tags/MusicInfoTag.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

CSong CMusicDatasetHelper::GetSongFromDataset(const dbiplus::sql_record* const record,
                                              int offset /* = 0 */)
{
  CSong song;
  song.idSong = record->at(offset + song_idSong).get_asInt();
  // Note this function does not populate artist credits, this must be done separately.
  // However artist names are held as a descriptive string
  song.strArtistDesc = record->at(offset + song_strArtists).get_asString();
  song.strArtistSort = record->at(offset + song_strArtistSort).get_asString();
  // Get the full genre string
  song.genre = StringUtils::Split(
      record->at(offset + song_strGenres).get_asString(),
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator);
  // and the rest...
  song.strAlbum = record->at(offset + song_strAlbum).get_asString();
  song.idAlbum = record->at(offset + song_idAlbum).get_asInt();
  song.iTrack = record->at(offset + song_iTrack).get_asInt();
  song.iDuration = record->at(offset + song_iDuration).get_asInt();
  song.strReleaseDate = record->at(offset + song_strReleaseDate).get_asString();
  song.strOrigReleaseDate = record->at(offset + song_strOrigReleaseDate).get_asString();
  song.strTitle = record->at(offset + song_strTitle).get_asString();
  song.iTimesPlayed = record->at(offset + song_iTimesPlayed).get_asInt();
  song.lastPlayed.SetFromDBDateTime(record->at(offset + song_lastplayed).get_asString());
  song.dateAdded.SetFromDBDateTime(record->at(offset + song_dateAdded).get_asString());
  song.dateNew.SetFromDBDateTime(record->at(offset + song_dateNew).get_asString());
  song.dateUpdated.SetFromDBDateTime(record->at(offset + song_dateModified).get_asString());
  song.iStartOffset = record->at(offset + song_iStartOffset).get_asInt();
  song.iEndOffset = record->at(offset + song_iEndOffset).get_asInt();
  song.strMusicBrainzTrackID = record->at(offset + song_strMusicBrainzTrackID).get_asString();
  song.rating = record->at(offset + song_rating).get_asFloat();
  song.userrating = record->at(offset + song_userrating).get_asInt();
  song.votes = record->at(offset + song_votes).get_asInt();
  song.strComment = record->at(offset + song_comment).get_asString();
  song.strMood = record->at(offset + song_mood).get_asString();
  song.bCompilation = record->at(offset + song_bCompilation).get_asInt() == 1;
  song.strDiscSubtitle = record->at(offset + song_strDiscSubtitle).get_asString();
  // Replay gain data (needed for songs from cuesheets, both separate .cue files and embedded metadata)
  song.replayGain.Set(record->at(offset + song_strReplayGain).get_asString());
  // Get filename with full path
  song.strFileName =
      URIUtils::AddFileToFolder(record->at(offset + song_strPath).get_asString(),
                                record->at(offset + song_strFileName).get_asString());
  song.iBPM = record->at(offset + song_iBPM).get_asInt();
  song.iBitRate = record->at(offset + song_iBitRate).get_asInt();
  song.iSampleRate = record->at(offset + song_iSampleRate).get_asInt();
  song.iChannels = record->at(offset + song_iChannels).get_asInt();
  song.songVideoURL = record->at(offset + song_songVideoURL).get_asString();
  return song;
}

void CMusicDatasetHelper::GetFileItemFromDataset(const dbiplus::sql_record* const record,
                                                 CFileItem* item,
                                                 const CMusicDbUrl& baseUrl)
{
  // get the artist string from songview (not the song_artist and artist tables)
  item->GetMusicInfoTag()->SetArtistDesc(record->at(song_strArtists).get_asString());
  // get the artist sort name string from songview (not the song_artist and artist tables)
  item->GetMusicInfoTag()->SetArtistSort(record->at(song_strArtistSort).get_asString());
  // and the full genre string
  item->GetMusicInfoTag()->SetGenre(record->at(song_strGenres).get_asString());
  // and the rest...
  item->GetMusicInfoTag()->SetAlbum(record->at(song_strAlbum).get_asString());
  item->GetMusicInfoTag()->SetAlbumId(record->at(song_idAlbum).get_asInt());
  item->GetMusicInfoTag()->SetTrackAndDiscNumber(record->at(song_iTrack).get_asInt());
  item->GetMusicInfoTag()->SetDuration(record->at(song_iDuration).get_asInt());
  item->GetMusicInfoTag()->SetDatabaseId(record->at(song_idSong).get_asInt(), MediaTypeSong);
  item->GetMusicInfoTag()->SetOriginalDate(record->at(song_strOrigReleaseDate).get_asString());
  item->GetMusicInfoTag()->SetReleaseDate(record->at(song_strReleaseDate).get_asString());
  item->GetMusicInfoTag()->SetTitle(record->at(song_strTitle).get_asString());
  item->GetMusicInfoTag()->SetDiscSubtitle(record->at(song_strDiscSubtitle).get_asString());
  item->SetLabel(record->at(song_strTitle).get_asString());
  item->SetStartOffset(record->at(song_iStartOffset).get_asInt64());
  item->SetProperty("item_start", item->GetStartOffset());
  item->SetEndOffset(record->at(song_iEndOffset).get_asInt64());
  item->GetMusicInfoTag()->SetMusicBrainzTrackID(
      record->at(song_strMusicBrainzTrackID).get_asString());
  item->GetMusicInfoTag()->SetRating(record->at(song_rating).get_asFloat());
  item->GetMusicInfoTag()->SetUserrating(record->at(song_userrating).get_asInt());
  item->GetMusicInfoTag()->SetVotes(record->at(song_votes).get_asInt());
  item->GetMusicInfoTag()->SetComment(record->at(song_comment).get_asString());
  item->GetMusicInfoTag()->SetMood(record->at(song_mood).get_asString());
  item->GetMusicInfoTag()->SetPlayCount(record->at(song_iTimesPlayed).get_asInt());
  item->GetMusicInfoTag()->SetLastPlayed(record->at(song_lastplayed).get_asString());
  item->GetMusicInfoTag()->SetDateAdded(record->at(song_dateAdded).get_asString());
  item->GetMusicInfoTag()->SetDateNew(record->at(song_dateNew).get_asString());
  item->GetMusicInfoTag()->SetDateUpdated(record->at(song_dateModified).get_asString());
  std::string strRealPath = URIUtils::AddFileToFolder(record->at(song_strPath).get_asString(),
                                                      record->at(song_strFileName).get_asString());
  item->GetMusicInfoTag()->SetURL(strRealPath);
  item->GetMusicInfoTag()->SetCompilation(record->at(song_bCompilation).get_asInt() == 1);
  item->GetMusicInfoTag()->SetBoxset(record->at(song_bBoxedSet).get_asInt() == 1);
  // get the album artist string from songview (not the album_artist and artist tables)
  item->GetMusicInfoTag()->SetAlbumArtist(record->at(song_strAlbumArtists).get_asString());
  item->GetMusicInfoTag()->SetAlbumReleaseType(
      CAlbum::ReleaseTypeFromString(record->at(song_strAlbumReleaseType).get_asString()));
  item->GetMusicInfoTag()->SetBPM(record->at(song_iBPM).get_asInt());
  item->GetMusicInfoTag()->SetBitRate(record->at(song_iBitRate).get_asInt());
  item->GetMusicInfoTag()->SetSampleRate(record->at(song_iSampleRate).get_asInt());
  item->GetMusicInfoTag()->SetNoOfChannels(record->at(song_iChannels).get_asInt());
  // Replay gain data (needed for songs from cuesheets, both separate .cue files and embedded metadata)
  ReplayGain replaygain;
  replaygain.Set(record->at(song_strReplayGain).get_asString());
  item->GetMusicInfoTag()->SetReplayGain(replaygain);
  item->GetMusicInfoTag()->SetTotalDiscs(record->at(song_iDiscTotal).get_asInt());
  item->GetMusicInfoTag()->SetSongVideoURL(record->at(song_songVideoURL).get_asString());

  item->GetMusicInfoTag()->SetLoaded(true);
  // Get filename with full path
  if (!baseUrl.IsValid())
    item->SetPath(strRealPath);
  else
  {
    CMusicDbUrl itemUrl = baseUrl;
    std::string strFileName = record->at(song_strFileName).get_asString();
    std::string strExt = URIUtils::GetExtension(strFileName);
    std::string path = StringUtils::Format("{}{}", record->at(song_idSong).get_asInt(), strExt);
    itemUrl.AppendPath(path);
    item->SetPath(itemUrl.ToString());
    item->SetDynPath(strRealPath);
  }
}

void CMusicDatasetHelper::GetFileItemFromArtistCredits(std::vector<CArtistCredit>& artistCredits,
                                                       CFileItem* item)
{
  // Populate fileitem with artists from vector of artist credits
  std::vector<std::string> musicBrainzID;
  std::vector<std::string> songartists;
  CVariant artistidObj(CVariant::VariantTypeArray);

  // When "missing tag" artist, it is the only artist when present.
  if (artistCredits.begin()->GetArtistId() == BLANKARTIST_ID)
  {
    artistidObj.push_back(BLANKARTIST_ID);
    songartists.push_back(StringUtils::Empty);
  }
  else
  {
    for (const auto& artistCredit : artistCredits)
    {
      artistidObj.push_back(artistCredit.GetArtistId());
      songartists.push_back(artistCredit.GetArtist());
      if (!artistCredit.GetMusicBrainzArtistID().empty())
        musicBrainzID.push_back(artistCredit.GetMusicBrainzArtistID());
    }
  }
  // Also sets ArtistDesc if empty from song.strArtist field
  item->GetMusicInfoTag()->SetArtist(songartists);
  item->GetMusicInfoTag()->SetMusicBrainzArtistID(musicBrainzID);
  // Add album artistIds as separate property as not part of CMusicInfoTag
  item->SetProperty("artistid", artistidObj);
}

CAlbum CMusicDatasetHelper::GetAlbumFromDataset(dbiplus::Dataset* pDS,
                                                int offset /* = 0 */,
                                                bool imageURL /* = false*/)
{
  return GetAlbumFromDataset(pDS->get_sql_record(), offset, imageURL);
}

CAlbum CMusicDatasetHelper::GetAlbumFromDataset(const dbiplus::sql_record* const record,
                                                int offset /* = 0 */,
                                                bool imageURL /* = false*/)
{
  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  CAlbum album;
  album.idAlbum = record->at(offset + album_idAlbum).get_asInt();
  album.strAlbum = record->at(offset + album_strAlbum).get_asString();
  if (album.strAlbum.empty())
    album.strAlbum = CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(1050);
  album.strMusicBrainzAlbumID = record->at(offset + album_strMusicBrainzAlbumID).get_asString();
  album.strReleaseGroupMBID = record->at(offset + album_strReleaseGroupMBID).get_asString();
  album.strArtistDesc = record->at(offset + album_strArtists).get_asString();
  album.strArtistSort = record->at(offset + album_strArtistSort).get_asString();
  album.genre =
      StringUtils::Split(record->at(offset + album_strGenres).get_asString(), itemSeparator);
  album.strReleaseDate = record->at(offset + album_strReleaseDate).get_asString();
  album.strOrigReleaseDate = record->at(offset + album_strOrigReleaseDate).get_asString();
  album.bBoxedSet = record->at(offset + album_bBoxedSet).get_asInt() == 1;
  if (imageURL)
    album.thumbURL.ParseFromData(record->at(offset + album_strThumbURL).get_asString());
  album.fRating = record->at(offset + album_fRating).get_asFloat();
  album.iUserrating = record->at(offset + album_iUserrating).get_asInt();
  album.iVotes = record->at(offset + album_iVotes).get_asInt();
  album.strReview = record->at(offset + album_strReview).get_asString();
  album.styles =
      StringUtils::Split(record->at(offset + album_strStyles).get_asString(), itemSeparator);
  album.moods =
      StringUtils::Split(record->at(offset + album_strMoods).get_asString(), itemSeparator);
  album.themes =
      StringUtils::Split(record->at(offset + album_strThemes).get_asString(), itemSeparator);
  album.strLabel = record->at(offset + album_strLabel).get_asString();
  album.strType = record->at(offset + album_strType).get_asString();
  album.strReleaseStatus = record->at(offset + album_strReleaseStatus).get_asString();
  album.bCompilation = record->at(offset + album_bCompilation).get_asInt() == 1;
  album.bScrapedMBID = record->at(offset + album_bScrapedMBID).get_asInt() == 1;
  album.strLastScraped = record->at(offset + album_lastScraped).get_asString();
  album.iTimesPlayed = record->at(offset + album_iTimesPlayed).get_asInt();
  album.SetReleaseType(record->at(offset + album_strReleaseType).get_asString());
  album.iTotalDiscs = record->at(offset + album_iTotalDiscs).get_asInt();
  album.SetDateAdded(record->at(offset + album_dateAdded).get_asString());
  album.SetDateNew(record->at(offset + album_dateNew).get_asString());
  album.SetDateUpdated(record->at(offset + album_dateModified).get_asString());
  album.SetLastPlayed(record->at(offset + album_dtLastPlayed).get_asString());
  album.iAlbumDuration = record->at(offset + album_iAlbumDuration).get_asInt();
  return album;
}

CArtistCredit CMusicDatasetHelper::GetArtistCreditFromDataset(
    const dbiplus::sql_record* const record, int offset /* = 0 */)
{
  CArtistCredit artistCredit;
  artistCredit.idArtist = record->at(offset + artistCredit_idArtist).get_asInt();
  if (artistCredit.idArtist == BLANKARTIST_ID)
    artistCredit.m_strArtist = StringUtils::Empty;
  else
  {
    artistCredit.m_strArtist = record->at(offset + artistCredit_strArtist).get_asString();
    artistCredit.m_strMusicBrainzArtistID =
        record->at(offset + artistCredit_strMusicBrainzArtistID).get_asString();
  }
  return artistCredit;
}

CMusicRole CMusicDatasetHelper::GetArtistRoleFromDataset(const dbiplus::sql_record* const record,
                                                         int offset /* = 0 */)
{
  CMusicRole ArtistRole(record->at(offset + artistCredit_idRole).get_asInt(),
                        record->at(offset + artistCredit_strRole).get_asString(),
                        record->at(offset + artistCredit_strArtist).get_asString(),
                        record->at(offset + artistCredit_idArtist).get_asInt());
  return ArtistRole;
}

CArtist CMusicDatasetHelper::GetArtistFromDataset(dbiplus::Dataset* pDS,
                                                  int offset /* = 0 */,
                                                  bool needThumb /* = true */,
                                                  bool translateBlankArtist /* = true */)
{
  return GetArtistFromDataset(pDS->get_sql_record(), offset, needThumb, translateBlankArtist);
}

CArtist CMusicDatasetHelper::GetArtistFromDataset(const dbiplus::sql_record* const record,
                                                  int offset /* = 0 */,
                                                  bool needThumb /* = true */,
                                                  bool translateBlankArtist /* = true */)
{
  const std::string itemSeparator =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;

  CArtist artist;
  artist.idArtist = record->at(offset + artist_idArtist).get_asInt();
  if (artist.idArtist == BLANKARTIST_ID && translateBlankArtist)
    artist.strArtist = CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(
        38042); //Missing artist tag in current language
  else
    artist.strArtist = record->at(offset + artist_strArtist).get_asString();
  artist.strSortName = record->at(offset + artist_strSortName).get_asString();
  artist.strMusicBrainzArtistID = record->at(offset + artist_strMusicBrainzArtistID).get_asString();
  artist.strType = record->at(offset + artist_strType).get_asString();
  artist.strGender = record->at(offset + artist_strGender).get_asString();
  artist.strDisambiguation = record->at(offset + artist_strDisambiguation).get_asString();
  artist.genre =
      StringUtils::Split(record->at(offset + artist_strGenres).get_asString(), itemSeparator);
  artist.strBiography = record->at(offset + artist_strBiography).get_asString();
  artist.styles =
      StringUtils::Split(record->at(offset + artist_strStyles).get_asString(), itemSeparator);
  artist.moods =
      StringUtils::Split(record->at(offset + artist_strMoods).get_asString(), itemSeparator);
  artist.strBorn = record->at(offset + artist_strBorn).get_asString();
  artist.strFormed = record->at(offset + artist_strFormed).get_asString();
  artist.strDied = record->at(offset + artist_strDied).get_asString();
  artist.strDisbanded = record->at(offset + artist_strDisbanded).get_asString();
  artist.yearsActive =
      StringUtils::Split(record->at(offset + artist_strYearsActive).get_asString(), itemSeparator);
  artist.instruments =
      StringUtils::Split(record->at(offset + artist_strInstruments).get_asString(), itemSeparator);
  artist.bScrapedMBID = record->at(offset + artist_bScrapedMBID).get_asInt() == 1;
  artist.strLastScraped = record->at(offset + artist_lastScraped).get_asString();
  artist.SetDateAdded(record->at(offset + artist_dateAdded).get_asString());
  artist.SetDateNew(record->at(offset + artist_dateNew).get_asString());
  artist.SetDateUpdated(record->at(offset + artist_dateModified).get_asString());

  if (needThumb)
  {
    artist.thumbURL.ParseFromData(record->at(artist_strImage).get_asString());
  }

  return artist;
}
