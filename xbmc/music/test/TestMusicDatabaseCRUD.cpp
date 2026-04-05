/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "dbwrappers/test/TestMusicDatabaseFixture.h"
#include "music/Album.h"
#include "music/Artist.h"
#include "music/Song.h"
#include "music/tags/ReplayGain.h"

#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using TestMusicDatabaseCRUD = TestMusicDatabaseFixture;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

//! \brief Add an artist with minimal parameters, returning its database ID.
int AddTestArtist(TestMusicDatabaseFixture::TestMusicDatabase& db,
                  const std::string& name,
                  const std::string& mbid = "")
{
  return db.AddArtist(name, mbid, /*bScrapedMBID=*/false);
}

//! \brief Add an album with minimal parameters, returning its database ID.
int AddTestAlbum(TestMusicDatabaseFixture::TestMusicDatabase& db,
                 const std::string& title,
                 const std::string& artistDisp,
                 const std::string& genre = "",
                 const std::string& mbid = "")
{
  return db.AddAlbum(title,
                     mbid,
                     /*strReleaseGroupMBID=*/"",
                     artistDisp,
                     /*strArtistSort=*/"",
                     genre,
                     /*strReleaseDate=*/"2024-01-01",
                     /*strOrigReleaseDate=*/"",
                     /*bBoxedSet=*/false,
                     /*strRecordLabel=*/"",
                     /*strType=*/"",
                     /*strReleaseStatus=*/"",
                     /*bCompilation=*/false,
                     ReleaseType::Album);
}

//! \brief Add a song with minimal parameters, returning its database ID.
//!
//! Also creates the song_artist link so that songartistview is populated and
//! GetSong() (which joins through that view) can find the song.
int AddTestSong(TestMusicDatabaseFixture::TestMusicDatabase& db,
                int idAlbum,
                int idArtist,
                const std::string& title,
                const std::string& artistDisp,
                const std::string& path,
                int track = 1,
                int duration = 200)
{
  std::string discSubtitle;
  ReplayGain rg;
  CDateTime dateNew;
  CDateTime lastPlayed;
  std::vector<std::string> genres;

  int idSong = db.AddSong(/*idSong=*/-1,
                           dateNew,
                           idAlbum,
                           title,
                           /*strMusicBrainzTrackID=*/"",
                           path,
                           /*strComment=*/"",
                           /*strMood=*/"",
                           /*strThumb=*/"",
                           artistDisp,
                           /*artistSort=*/"",
                           genres,
                           track,
                           duration,
                           /*strReleaseDate=*/"2024-01-01",
                           /*strOrigReleaseDate=*/"",
                           discSubtitle,
                           /*iTimesPlayed=*/0,
                           /*iStartOffset=*/0,
                           /*iEndOffset=*/0,
                           lastPlayed,
                           /*rating=*/0.0f,
                           /*userrating=*/0,
                           /*votes=*/0,
                           /*iBPM=*/0,
                           /*iBitRate=*/320,
                           /*iSampleRate=*/44100,
                           /*iChannels=*/2,
                           /*songVideoURL=*/"",
                           rg);

  if (idSong > 0 && idArtist > 0)
  {
    db.AddSongArtist(idArtist, idSong, "Artist", artistDisp, 0);
  }

  return idSong;
}

} // anonymous namespace

// ===========================================================================
// Song CRUD
// ===========================================================================

TEST_F(TestMusicDatabaseCRUD, AddSong_ValidData_ReturnsSongIdGreaterThanZero)
{
  int idArtist = AddTestArtist(*m_musicDb, "The Beatles");
  int idAlbum = AddTestAlbum(*m_musicDb, "Abbey Road", "The Beatles");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "Come Together", "The Beatles",
                           "/music/abbey_road/come_together.mp3");
  EXPECT_GT(idSong, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddSong_EmptyTitle_ReturnsNegativeOne)
{
  int idArtist = AddTestArtist(*m_musicDb, "The Beatles");
  int idAlbum = AddTestAlbum(*m_musicDb, "Abbey Road", "The Beatles");
  std::string discSubtitle;
  ReplayGain rg;
  CDateTime dateNew;
  CDateTime lastPlayed;
  std::vector<std::string> genres;

  int idSong = m_musicDb->AddSong(-1, dateNew, idAlbum,
                                   /*strTitle=*/"",
                                   "", "/music/empty.mp3",
                                   "", "", "", "The Beatles", "",
                                   genres, 1, 200, "", "",
                                   discSubtitle, 0, 0, 0,
                                   lastPlayed, 0.0f, 0, 0,
                                   0, 320, 44100, 2, "", rg);
  EXPECT_EQ(idSong, -1);
}

TEST_F(TestMusicDatabaseCRUD, AddSong_DuplicateSamePath_ReusesExistingId)
{
  int idArtist = AddTestArtist(*m_musicDb, "Artist A");
  int idAlbum = AddTestAlbum(*m_musicDb, "Album A", "Artist A");

  int id1 = AddTestSong(*m_musicDb, idAlbum, idArtist,
                         "Song One", "Artist A",
                         "/music/album_a/song_one.mp3", 1);
  int id2 = AddTestSong(*m_musicDb, idAlbum, idArtist,
                         "Song One", "Artist A",
                         "/music/album_a/song_one.mp3", 1);

  EXPECT_GT(id1, 0);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddSong_DifferentTracks_DifferentIds)
{
  int idArtist = AddTestArtist(*m_musicDb, "Artist B");
  int idAlbum = AddTestAlbum(*m_musicDb, "Album B", "Artist B");

  int id1 = AddTestSong(*m_musicDb, idAlbum, idArtist,
                         "Track One", "Artist B",
                         "/music/album_b/track1.mp3", 1);
  int id2 = AddTestSong(*m_musicDb, idAlbum, idArtist,
                         "Track Two", "Artist B",
                         "/music/album_b/track2.mp3", 2);

  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, GetSong_ById_ReturnsCorrectTitle)
{
  int idArtist = AddTestArtist(*m_musicDb, "Pink Floyd");
  int idAlbum = AddTestAlbum(*m_musicDb, "The Wall", "Pink Floyd");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "Comfortably Numb", "Pink Floyd",
                           "/music/the_wall/comfortably_numb.mp3");
  ASSERT_GT(idSong, 0);

  CSong song;
  bool found = m_musicDb->GetSong(idSong, song);
  ASSERT_TRUE(found);
  EXPECT_EQ(song.strTitle, "Comfortably Numb");
  EXPECT_EQ(song.idAlbum, idAlbum);
}

TEST_F(TestMusicDatabaseCRUD, GetSong_NonExistentId_ReturnsFalse)
{
  CSong song;
  EXPECT_FALSE(m_musicDb->GetSong(99999, song));
}

TEST_F(TestMusicDatabaseCRUD, GetSongByFileName_ValidPath_ReturnsSong)
{
  int idArtist = AddTestArtist(*m_musicDb, "Queen");
  int idAlbum = AddTestAlbum(*m_musicDb, "News of the World", "Queen");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "We Will Rock You", "Queen",
                           "/music/notw/we_will_rock_you.mp3");
  ASSERT_GT(idSong, 0);

  CSong song;
  bool found = m_musicDb->GetSongByFileName("/music/notw/we_will_rock_you.mp3", song);
  ASSERT_TRUE(found);
  EXPECT_EQ(song.strTitle, "We Will Rock You");
}

TEST_F(TestMusicDatabaseCRUD, GetSongByFileName_NonExistentPath_ReturnsFalse)
{
  CSong song;
  EXPECT_FALSE(m_musicDb->GetSongByFileName("/no/such/path.mp3", song));
}

TEST_F(TestMusicDatabaseCRUD, GetSongsByPath_MultipleSongs_ReturnsAll)
{
  int idArtist = AddTestArtist(*m_musicDb, "Led Zeppelin");
  int idAlbum = AddTestAlbum(*m_musicDb, "IV", "Led Zeppelin");

  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Black Dog", "Led Zeppelin",
              "/music/iv/black_dog.mp3", 1);
  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Rock and Roll", "Led Zeppelin",
              "/music/iv/rock_and_roll.mp3", 2);
  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Stairway to Heaven", "Led Zeppelin",
              "/music/iv/stairway.mp3", 4);

  std::map<std::string, std::vector<CSong>> songmap;
  bool result = m_musicDb->GetSongsByPath("/music/iv/", songmap);
  ASSERT_TRUE(result);

  // Three different filenames
  EXPECT_EQ(songmap.size(), 3u);
}

TEST_F(TestMusicDatabaseCRUD, GetSongsByPath_EmptyPath_ReturnsFalse)
{
  std::map<std::string, std::vector<CSong>> songmap;
  EXPECT_FALSE(m_musicDb->GetSongsByPath("/nonexistent/", songmap));
}

TEST_F(TestMusicDatabaseCRUD, UpdateSong_ById_ChangesPersist)
{
  int idArtist = AddTestArtist(*m_musicDb, "Radiohead");
  int idAlbum = AddTestAlbum(*m_musicDb, "OK Computer", "Radiohead");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "Paranoid Android", "Radiohead",
                           "/music/okc/paranoid.mp3", 2, 383);
  ASSERT_GT(idSong, 0);

  ReplayGain rg;
  CDateTime lastPlayed;
  m_musicDb->UpdateSong(idSong,
                         "Paranoid Android (Remaster)",
                         /*strMusicBrainzTrackID=*/"",
                         "/music/okc/paranoid.mp3",
                         "Remastered comment",
                         /*strMood=*/"",
                         /*strThumb=*/"",
                         "Radiohead",
                         /*artistSort=*/"",
                         /*genres=*/{},
                         2,
                         /*iDuration=*/390,
                         /*strReleaseDate=*/"2024-01-01",
                         /*strOrigReleaseDate=*/"",
                         /*strDiscSubtitle=*/"",
                         /*iTimesPlayed=*/5,
                         0, 0,
                         lastPlayed,
                         /*rating=*/4.5f,
                         /*userrating=*/8,
                         /*votes=*/100,
                         rg,
                         /*iBPM=*/0,
                         /*iBitRate=*/320,
                         /*iSampleRate=*/44100,
                         /*iChannels=*/2,
                         /*songVideoURL=*/"");

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.strTitle, "Paranoid Android (Remaster)");
  EXPECT_EQ(song.iDuration, 390);
}

TEST_F(TestMusicDatabaseCRUD, GetSongByArtistAndAlbumAndTitle_Match_ReturnsSongId)
{
  int idArtist = AddTestArtist(*m_musicDb, "Nirvana");
  int idAlbum = AddTestAlbum(*m_musicDb, "Nevermind", "Nirvana");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "Smells Like Teen Spirit", "Nirvana",
                           "/music/nevermind/smells.mp3");
  ASSERT_GT(idSong, 0);

  int found = m_musicDb->GetSongByArtistAndAlbumAndTitle("Nirvana", "Nevermind",
                                                          "Smells Like Teen Spirit");
  EXPECT_EQ(found, idSong);
}

TEST_F(TestMusicDatabaseCRUD, GetSongByArtistAndAlbumAndTitle_NoMatch_ReturnsNegativeOne)
{
  int result = m_musicDb->GetSongByArtistAndAlbumAndTitle("Nobody", "Nothing", "Nowhere");
  EXPECT_EQ(result, -1);
}

TEST_F(TestMusicDatabaseCRUD, SetSongUserrating_ById_PersistsRating)
{
  int idArtist = AddTestArtist(*m_musicDb, "Tool");
  int idAlbum = AddTestAlbum(*m_musicDb, "Lateralus", "Tool");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "Schism", "Tool",
                           "/music/lateralus/schism.mp3");
  ASSERT_GT(idSong, 0);

  EXPECT_TRUE(m_musicDb->SetSongUserrating(idSong, 9));

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.userrating, 9);
}

TEST_F(TestMusicDatabaseCRUD, RemoveSongsFromPath_ExistingPath_RemovesSongs)
{
  int idArtist = AddTestArtist(*m_musicDb, "Metallica");
  int idAlbum = AddTestAlbum(*m_musicDb, "Black Album", "Metallica");

  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Enter Sandman", "Metallica",
              "/music/black/enter_sandman.mp3", 1);
  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Nothing Else Matters", "Metallica",
              "/music/black/nothing.mp3", 8);

  std::map<std::string, std::vector<CSong>> removed;
  bool result = m_musicDb->RemoveSongsFromPath("/music/black/", removed);
  EXPECT_TRUE(result);
  EXPECT_FALSE(removed.empty());

  // Verify songs are gone from the path
  std::map<std::string, std::vector<CSong>> remaining;
  EXPECT_FALSE(m_musicDb->GetSongsByPath("/music/black/", remaining));
}

// ===========================================================================
// Album CRUD
// ===========================================================================

TEST_F(TestMusicDatabaseCRUD, AddAlbum_ValidData_ReturnsAlbumIdGreaterThanZero)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "Dark Side of the Moon", "Pink Floyd", "Rock");
  EXPECT_GT(idAlbum, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddAlbum_DuplicateNameAndArtist_ReusesExistingId)
{
  int id1 = AddTestAlbum(*m_musicDb, "Thriller", "Michael Jackson");
  int id2 = AddTestAlbum(*m_musicDb, "Thriller", "Michael Jackson");
  EXPECT_GT(id1, 0);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddAlbum_DifferentArtistsSameTitle_DifferentIds)
{
  int id1 = AddTestAlbum(*m_musicDb, "Greatest Hits", "Queen");
  int id2 = AddTestAlbum(*m_musicDb, "Greatest Hits", "Elton John");
  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddAlbum_WithMusicBrainzId_ReturnsValidId)
{
  int idAlbum = m_musicDb->AddAlbum("Rumours",
                                     "b1234567-89ab-cdef-0123-456789abcdef",
                                     "",
                                     "Fleetwood Mac",
                                     "",
                                     "Rock",
                                     "1977-02-04",
                                     "",
                                     false,
                                     "Warner Bros.",
                                     "",
                                     "",
                                     false,
                                     ReleaseType::Album);
  EXPECT_GT(idAlbum, 0);
}

TEST_F(TestMusicDatabaseCRUD, GetAlbum_ById_ReturnsCorrectData)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "The Bends", "Radiohead", "Alternative");
  ASSERT_GT(idAlbum, 0);

  CAlbum album;
  bool found = m_musicDb->GetAlbum(idAlbum, album, /*getSongs=*/false);
  ASSERT_TRUE(found);
  EXPECT_EQ(album.strAlbum, "The Bends");
  EXPECT_EQ(album.strArtistDesc, "Radiohead");
}

TEST_F(TestMusicDatabaseCRUD, GetAlbum_NonExistentId_ReturnsFalse)
{
  CAlbum album;
  EXPECT_FALSE(m_musicDb->GetAlbum(99999, album, false));
}

TEST_F(TestMusicDatabaseCRUD, GetAlbum_WithSongs_IncludesSongs)
{
  int idArtist = AddTestArtist(*m_musicDb, "Daft Punk");
  int idAlbum = AddTestAlbum(*m_musicDb, "Discovery", "Daft Punk");
  m_musicDb->AddAlbumArtist(idArtist, idAlbum, "Daft Punk", 0);

  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "One More Time", "Daft Punk",
              "/music/discovery/one_more_time.mp3", 1);
  AddTestSong(*m_musicDb, idAlbum, idArtist,
              "Aerodynamic", "Daft Punk",
              "/music/discovery/aerodynamic.mp3", 2);

  CAlbum album;
  bool found = m_musicDb->GetAlbum(idAlbum, album, /*getSongs=*/true);
  ASSERT_TRUE(found);
  EXPECT_EQ(album.strAlbum, "Discovery");
  EXPECT_EQ(album.songs.size(), 2u);
}

TEST_F(TestMusicDatabaseCRUD, GetAlbumByName_ExistingAlbum_ReturnsId)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "Wish You Were Here", "Pink Floyd");
  ASSERT_GT(idAlbum, 0);

  int found = m_musicDb->GetAlbumByName("Wish You Were Here", "Pink Floyd");
  EXPECT_EQ(found, idAlbum);
}

TEST_F(TestMusicDatabaseCRUD, GetAlbumByName_NonExistent_ReturnsNegativeOne)
{
  EXPECT_EQ(m_musicDb->GetAlbumByName("No Such Album", "No Such Artist"), -1);
}

TEST_F(TestMusicDatabaseCRUD, UpdateAlbum_ById_ChangesPersist)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "OK Computer", "Radiohead");
  ASSERT_GT(idAlbum, 0);

  m_musicDb->UpdateAlbum(idAlbum,
                          "OK Computer OKNOTOK",
                          /*strMusicBrainzAlbumID=*/"",
                          /*strReleaseGroupMBID=*/"",
                          "Radiohead",
                          /*strArtistSort=*/"",
                          "Alternative Rock",
                          /*strMoods=*/"Melancholy",
                          /*strStyles=*/"Art Rock",
                          /*strThemes=*/"",
                          /*strReview=*/"A landmark album reissue",
                          /*strImage=*/"",
                          "XL Recordings",
                          /*strType=*/"",
                          /*strReleaseStatus=*/"",
                          4.8f,
                          /*iUserrating=*/9,
                          /*iVotes=*/5000,
                          "2017-06-23",
                          "1997-06-16",
                          /*bBoxedSet=*/false,
                          /*bCompilation=*/false,
                          ReleaseType::Album,
                          /*bScrapedMBID=*/false);

  CAlbum album;
  ASSERT_TRUE(m_musicDb->GetAlbum(idAlbum, album, false));
  EXPECT_EQ(album.strAlbum, "OK Computer OKNOTOK");
  EXPECT_EQ(album.strLabel, "XL Recordings");
}

TEST_F(TestMusicDatabaseCRUD, SetAlbumUserrating_ValidAlbum_PersistsRating)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "In Rainbows", "Radiohead");
  ASSERT_GT(idAlbum, 0);

  EXPECT_TRUE(m_musicDb->SetAlbumUserrating(idAlbum, 10));

  CAlbum album;
  ASSERT_TRUE(m_musicDb->GetAlbum(idAlbum, album, false));
  EXPECT_EQ(album.iUserrating, 10);
}

TEST_F(TestMusicDatabaseCRUD, GetAlbumsByArtist_WithLink_ReturnsAlbums)
{
  int idArtist = AddTestArtist(*m_musicDb, "Bjork");
  int idAlbum1 = AddTestAlbum(*m_musicDb, "Debut", "Bjork");
  int idAlbum2 = AddTestAlbum(*m_musicDb, "Post", "Bjork");

  m_musicDb->AddAlbumArtist(idArtist, idAlbum1, "Bjork", 0);
  m_musicDb->AddAlbumArtist(idArtist, idAlbum2, "Bjork", 0);

  std::vector<int> albums;
  bool result = m_musicDb->GetAlbumsByArtist(idArtist, albums);
  ASSERT_TRUE(result);
  EXPECT_EQ(albums.size(), 2u);
}

TEST_F(TestMusicDatabaseCRUD, GetAlbumsByArtist_NoLink_ReturnsFalse)
{
  int idArtist = AddTestArtist(*m_musicDb, "Loner McNoAlbums");
  std::vector<int> albums;
  EXPECT_FALSE(m_musicDb->GetAlbumsByArtist(idArtist, albums));
}

TEST_F(TestMusicDatabaseCRUD, DeleteAlbumArtistsByAlbum_RemovesLinks)
{
  int idArtist = AddTestArtist(*m_musicDb, "Portishead");
  int idAlbum = AddTestAlbum(*m_musicDb, "Dummy", "Portishead");
  m_musicDb->AddAlbumArtist(idArtist, idAlbum, "Portishead", 0);

  std::vector<int> albums;
  ASSERT_TRUE(m_musicDb->GetAlbumsByArtist(idArtist, albums));
  ASSERT_EQ(albums.size(), 1u);

  EXPECT_TRUE(m_musicDb->DeleteAlbumArtistsByAlbum(idAlbum));

  albums.clear();
  EXPECT_FALSE(m_musicDb->GetAlbumsByArtist(idArtist, albums));
}

// ===========================================================================
// Artist CRUD
// ===========================================================================

TEST_F(TestMusicDatabaseCRUD, AddArtist_ValidName_ReturnsArtistIdGreaterThanZero)
{
  int idArtist = AddTestArtist(*m_musicDb, "David Bowie");
  EXPECT_GT(idArtist, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddArtist_DuplicateName_ReusesExistingId)
{
  int id1 = AddTestArtist(*m_musicDb, "Tame Impala");
  int id2 = AddTestArtist(*m_musicDb, "Tame Impala");
  EXPECT_GT(id1, 0);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddArtist_WithMusicBrainzId_ReturnsValidId)
{
  int idArtist = m_musicDb->AddArtist("Sigur Ros",
                                       "a1234567-89ab-cdef-0123-456789abcdef",
                                       /*bScrapedMBID=*/false);
  EXPECT_GT(idArtist, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddArtist_SameNameDifferentMBID_ReturnsNewId)
{
  int id1 = m_musicDb->AddArtist("Alias",
                                  "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                                  false);
  int id2 = m_musicDb->AddArtist("Alias",
                                  "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb",
                                  false);
  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddArtist_WithSortName_StoresSortName)
{
  int idArtist = m_musicDb->AddArtist("The Beatles",
                                       "",
                                       "Beatles, The",
                                       /*bScrapedMBID=*/false);
  EXPECT_GT(idArtist, 0);

  CArtist artist;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, artist, false));
  EXPECT_EQ(artist.strSortName, "Beatles, The");
}

TEST_F(TestMusicDatabaseCRUD, GetArtist_ById_ReturnsCorrectName)
{
  int idArtist = AddTestArtist(*m_musicDb, "Massive Attack");
  ASSERT_GT(idArtist, 0);

  CArtist artist;
  bool found = m_musicDb->GetArtist(idArtist, artist, false);
  ASSERT_TRUE(found);
  EXPECT_EQ(artist.strArtist, "Massive Attack");
}

TEST_F(TestMusicDatabaseCRUD, GetArtist_NonExistentId_ReturnsFalse)
{
  CArtist artist;
  EXPECT_FALSE(m_musicDb->GetArtist(99999, artist, false));
}

TEST_F(TestMusicDatabaseCRUD, GetArtistExists_ValidId_ReturnsTrue)
{
  int idArtist = AddTestArtist(*m_musicDb, "Burial");
  ASSERT_GT(idArtist, 0);
  EXPECT_TRUE(m_musicDb->GetArtistExists(idArtist));
}

TEST_F(TestMusicDatabaseCRUD, GetArtistExists_InvalidId_ReturnsFalse)
{
  EXPECT_FALSE(m_musicDb->GetArtistExists(99999));
}

TEST_F(TestMusicDatabaseCRUD, GetArtistByName_ExistingArtist_ReturnsId)
{
  int idArtist = AddTestArtist(*m_musicDb, "Aphex Twin");
  ASSERT_GT(idArtist, 0);

  int found = m_musicDb->GetArtistByName("Aphex Twin");
  EXPECT_EQ(found, idArtist);
}

TEST_F(TestMusicDatabaseCRUD, GetArtistByName_NonExistent_ReturnsNegativeOne)
{
  EXPECT_EQ(m_musicDb->GetArtistByName("Nobody Knows This Artist"), -1);
}

TEST_F(TestMusicDatabaseCRUD, UpdateArtist_ById_ChangesPersist)
{
  int idArtist = AddTestArtist(*m_musicDb, "Boards of Canada");
  ASSERT_GT(idArtist, 0);

  m_musicDb->UpdateArtist(idArtist,
                           "Boards of Canada",
                           "Boards of Canada",
                           /*strMusicBrainzArtistID=*/"",
                           /*bScrapedMBID=*/false,
                           /*strType=*/"Group",
                           /*strGender=*/"",
                           /*strDisambiguation=*/"Scottish electronic duo",
                           /*strBorn=*/"",
                           /*strFormed=*/"1986",
                           /*strGenres=*/"Electronic, Ambient",
                           /*strMoods=*/"Dreamy",
                           /*strStyles=*/"IDM",
                           /*strInstruments=*/"Synthesizer",
                           /*strBiography=*/"Marcus Eoin and Michael Sandison",
                           /*strDied=*/"",
                           /*strDisbanded=*/"",
                           /*strYearsActive=*/"1986-",
                           /*strImage=*/"");

  CArtist artist;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, artist, true));
  EXPECT_EQ(artist.strType, "Group");
  EXPECT_EQ(artist.strDisambiguation, "Scottish electronic duo");
  EXPECT_EQ(artist.strBiography, "Marcus Eoin and Michael Sandison");
}

TEST_F(TestMusicDatabaseCRUD, UpdateArtist_Object_ChangesPersist)
{
  int idArtist = AddTestArtist(*m_musicDb, "Autechre");
  ASSERT_GT(idArtist, 0);

  CArtist artist;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, artist, false));
  artist.strType = "Group";
  artist.strBiography = "Rob Brown and Sean Booth";
  EXPECT_TRUE(m_musicDb->UpdateArtist(artist));

  CArtist updated;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, updated, true));
  EXPECT_EQ(updated.strType, "Group");
  EXPECT_EQ(updated.strBiography, "Rob Brown and Sean Booth");
}

TEST_F(TestMusicDatabaseCRUD, DeleteArtistDiscography_RemovesEntries)
{
  int idArtist = AddTestArtist(*m_musicDb, "Depeche Mode");
  ASSERT_GT(idArtist, 0);

  CDiscoAlbum disco;
  disco.strAlbum = "Violator";
  disco.strYear = "1990";
  m_musicDb->AddArtistDiscography(idArtist, disco);

  EXPECT_TRUE(m_musicDb->DeleteArtistDiscography(idArtist));

  CFileItemList items;
  // After deleting discography, GetArtistDiscography should return false
  EXPECT_FALSE(m_musicDb->GetArtistDiscography(idArtist, items));
}

TEST_F(TestMusicDatabaseCRUD, AddArtistDiscography_ValidData_Persists)
{
  int idArtist = AddTestArtist(*m_musicDb, "Joy Division");
  ASSERT_GT(idArtist, 0);

  CDiscoAlbum disco1;
  disco1.strAlbum = "Unknown Pleasures";
  disco1.strYear = "1979";
  CDiscoAlbum disco2;
  disco2.strAlbum = "Closer";
  disco2.strYear = "1980";

  m_musicDb->AddArtistDiscography(idArtist, disco1);
  m_musicDb->AddArtistDiscography(idArtist, disco2);

  CFileItemList items;
  ASSERT_TRUE(m_musicDb->GetArtistDiscography(idArtist, items));
  EXPECT_EQ(items.Size(), 2);
}

// ===========================================================================
// Edge Cases
// ===========================================================================

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetSong_ReturnsFalse)
{
  CSong song;
  EXPECT_FALSE(m_musicDb->GetSong(1, song));
}

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetAlbum_ReturnsFalse)
{
  CAlbum album;
  EXPECT_FALSE(m_musicDb->GetAlbum(1, album, false));
}

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetArtist_ReturnsFalse)
{
  CArtist artist;
  EXPECT_FALSE(m_musicDb->GetArtist(1, artist, false));
}

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetArtistByName_ReturnsNegativeOne)
{
  EXPECT_EQ(m_musicDb->GetArtistByName("Anyone"), -1);
}

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetAlbumByName_ReturnsNegativeOne)
{
  EXPECT_EQ(m_musicDb->GetAlbumByName("Anything"), -1);
}

TEST_F(TestMusicDatabaseCRUD, EmptyDatabase_GetSongsByPath_ReturnsFalse)
{
  std::map<std::string, std::vector<CSong>> songmap;
  EXPECT_FALSE(m_musicDb->GetSongsByPath("/any/path/", songmap));
}

TEST_F(TestMusicDatabaseCRUD, UnicodeArtistName_RoundTrips)
{
  int idArtist = AddTestArtist(*m_musicDb, "\xC3\x96lafur Arnalds");
  ASSERT_GT(idArtist, 0);

  CArtist artist;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, artist, false));
  EXPECT_EQ(artist.strArtist, "\xC3\x96lafur Arnalds");
}

TEST_F(TestMusicDatabaseCRUD, UnicodeAlbumTitle_RoundTrips)
{
  int idAlbum = AddTestAlbum(*m_musicDb, "\xE4\xB8\x96\xE7\x95\x8C", "Artist");
  ASSERT_GT(idAlbum, 0);

  CAlbum album;
  ASSERT_TRUE(m_musicDb->GetAlbum(idAlbum, album, false));
  EXPECT_EQ(album.strAlbum, "\xE4\xB8\x96\xE7\x95\x8C");
}

TEST_F(TestMusicDatabaseCRUD, UnicodeSongTitle_RoundTrips)
{
  int idArtist = AddTestArtist(*m_musicDb, "Artist");
  int idAlbum = AddTestAlbum(*m_musicDb, "Album", "Artist");
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           "\xC3\xA9\xC3\xA0\xC3\xBC", "Artist",
                           "/music/unicode/song.mp3");
  ASSERT_GT(idSong, 0);

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.strTitle, "\xC3\xA9\xC3\xA0\xC3\xBC");
}

TEST_F(TestMusicDatabaseCRUD, VeryLongArtistName_Survives)
{
  std::string longName(1500, 'A');
  int idArtist = AddTestArtist(*m_musicDb, longName);
  ASSERT_GT(idArtist, 0);

  CArtist artist;
  ASSERT_TRUE(m_musicDb->GetArtist(idArtist, artist, false));
  EXPECT_EQ(artist.strArtist, longName);
}

TEST_F(TestMusicDatabaseCRUD, VeryLongAlbumTitle_Survives)
{
  std::string longTitle(1500, 'B');
  int idAlbum = AddTestAlbum(*m_musicDb, longTitle, "Some Artist");
  ASSERT_GT(idAlbum, 0);

  CAlbum album;
  ASSERT_TRUE(m_musicDb->GetAlbum(idAlbum, album, false));
  EXPECT_EQ(album.strAlbum, longTitle);
}

TEST_F(TestMusicDatabaseCRUD, VeryLongSongTitle_Survives)
{
  int idArtist = AddTestArtist(*m_musicDb, "Long Song Artist");
  int idAlbum = AddTestAlbum(*m_musicDb, "Long Song Album", "Long Song Artist");
  std::string longTitle(1500, 'C');
  int idSong = AddTestSong(*m_musicDb, idAlbum, idArtist,
                           longTitle, "Long Song Artist",
                           "/music/long/song.mp3");
  ASSERT_GT(idSong, 0);

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.strTitle, longTitle);
}

TEST_F(TestMusicDatabaseCRUD, EmptyArtistName_StillAdds)
{
  // The database allows empty artist names (not empty titles for songs)
  int idArtist = AddTestArtist(*m_musicDb, "");
  // AddArtist with empty name should still work (matches "[Missing Tag]" behavior)
  EXPECT_GT(idArtist, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddPath_ReturnsSameIdForSamePath)
{
  int id1 = m_musicDb->AddPath("/music/test/");
  int id2 = m_musicDb->AddPath("/music/test/");
  EXPECT_GT(id1, 0);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddPath_DifferentPaths_DifferentIds)
{
  int id1 = m_musicDb->AddPath("/music/path_a/");
  int id2 = m_musicDb->AddPath("/music/path_b/");
  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, AddGenre_ReturnsValidId)
{
  std::string genre = "Progressive Rock";
  int idGenre = m_musicDb->AddGenre(genre);
  EXPECT_GT(idGenre, 0);
}

TEST_F(TestMusicDatabaseCRUD, AddGenre_DuplicateName_ReusesId)
{
  std::string genre1 = "Jazz";
  std::string genre2 = "Jazz";
  int id1 = m_musicDb->AddGenre(genre1);
  int id2 = m_musicDb->AddGenre(genre2);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestMusicDatabaseCRUD, GetGenreByName_ExistingGenre_ReturnsId)
{
  std::string genre = "Blues";
  int idGenre = m_musicDb->AddGenre(genre);
  ASSERT_GT(idGenre, 0);

  int found = m_musicDb->GetGenreByName("Blues");
  EXPECT_EQ(found, idGenre);
}

TEST_F(TestMusicDatabaseCRUD, MultipleArtistsMultipleAlbums_CorrectAssociations)
{
  int idArtist1 = AddTestArtist(*m_musicDb, "Artist One");
  int idArtist2 = AddTestArtist(*m_musicDb, "Artist Two");
  int idAlbum1 = AddTestAlbum(*m_musicDb, "Album One", "Artist One");
  int idAlbum2 = AddTestAlbum(*m_musicDb, "Album Two", "Artist Two");
  int idAlbum3 = AddTestAlbum(*m_musicDb, "Collab Album", "Artist One & Artist Two");

  m_musicDb->AddAlbumArtist(idArtist1, idAlbum1, "Artist One", 0);
  m_musicDb->AddAlbumArtist(idArtist2, idAlbum2, "Artist Two", 0);
  m_musicDb->AddAlbumArtist(idArtist1, idAlbum3, "Artist One", 0);
  m_musicDb->AddAlbumArtist(idArtist2, idAlbum3, "Artist Two", 1);

  std::vector<int> artist1Albums;
  ASSERT_TRUE(m_musicDb->GetAlbumsByArtist(idArtist1, artist1Albums));
  EXPECT_EQ(artist1Albums.size(), 2u);

  std::vector<int> artist2Albums;
  ASSERT_TRUE(m_musicDb->GetAlbumsByArtist(idArtist2, artist2Albums));
  EXPECT_EQ(artist2Albums.size(), 2u);
}

TEST_F(TestMusicDatabaseCRUD, Song_WithAllFields_RoundTrips)
{
  int idArtist = AddTestArtist(*m_musicDb, "Full Test Artist");
  int idAlbum = AddTestAlbum(*m_musicDb, "Full Test Album", "Full Test Artist", "Rock");
  m_musicDb->AddAlbumArtist(idArtist, idAlbum, "Full Test Artist", 0);

  std::string discSubtitle;
  ReplayGain rg;
  CDateTime dateNew;
  CDateTime lastPlayed;
  std::vector<std::string> genres = {"Rock", "Alternative"};

  int idSong = m_musicDb->AddSong(-1,
                                   dateNew,
                                   idAlbum,
                                   "Full Song",
                                   "mbid-track-123",
                                   "/music/full/song.flac",
                                   "A great comment",
                                   "Happy",
                                   "",
                                   "Full Test Artist",
                                   "Test Artist, Full",
                                   genres,
                                   (1 << 16) | 3, // disc 1, track 3
                                   301,
                                   "2024-06-15",
                                   "2023-01-01",
                                   discSubtitle,
                                   7,
                                   0, 0,
                                   lastPlayed,
                                   4.2f,
                                   8,
                                   42,
                                   128,
                                   320,
                                   48000,
                                   2,
                                   "https://example.com/video",
                                   rg);
  ASSERT_GT(idSong, 0);

  m_musicDb->AddSongArtist(idArtist, idSong, "Artist", "Full Test Artist", 0);

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.strTitle, "Full Song");
  EXPECT_EQ(song.iDuration, 301);
  EXPECT_EQ(song.strComment, "A great comment");
  EXPECT_EQ(song.iBitRate, 320);
  EXPECT_EQ(song.iSampleRate, 48000);
  EXPECT_EQ(song.iChannels, 2);
}

TEST_F(TestMusicDatabaseCRUD, Song_SpecialCharactersInComment_RoundTrips)
{
  int idArtist = AddTestArtist(*m_musicDb, "Special");
  int idAlbum = AddTestAlbum(*m_musicDb, "Chars", "Special");

  std::string discSubtitle;
  ReplayGain rg;
  CDateTime dateNew;
  CDateTime lastPlayed;
  std::vector<std::string> genres;

  std::string comment = "It's a \"great\" song & <bold> stuff! \xC3\xA9";

  int idSong = m_musicDb->AddSong(-1, dateNew, idAlbum,
                                   "Special Song", "",
                                   "/music/special/song.mp3",
                                   comment, "", "",
                                   "Special", "",
                                   genres, 1, 200, "", "",
                                   discSubtitle, 0, 0, 0,
                                   lastPlayed, 0.0f, 0, 0,
                                   0, 320, 44100, 2, "", rg);
  ASSERT_GT(idSong, 0);
  m_musicDb->AddSongArtist(idArtist, idSong, "Artist", "Special", 0);

  CSong song;
  ASSERT_TRUE(m_musicDb->GetSong(idSong, song));
  EXPECT_EQ(song.strComment, comment);
}

TEST_F(TestMusicDatabaseCRUD, Source_AddAndRetrieve)
{
  std::vector<std::string> paths = {"/music/library/"};
  int idSource = m_musicDb->AddSource("My Music", "/music/library/", paths);
  EXPECT_GT(idSource, 0);

  int found = m_musicDb->GetSourceByName("My Music");
  EXPECT_EQ(found, idSource);
}

TEST_F(TestMusicDatabaseCRUD, Source_RemoveByName)
{
  std::vector<std::string> paths = {"/music/temp/"};
  m_musicDb->AddSource("Temp Music", "/music/temp/", paths);
  EXPECT_TRUE(m_musicDb->RemoveSource("Temp Music"));
  EXPECT_EQ(m_musicDb->GetSourceByName("Temp Music"), -1);
}
