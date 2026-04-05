/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "dbwrappers/test/TestMusicDatabaseFixture.h"

#include <string>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Helper: insert a minimal path row and return its idPath
// ---------------------------------------------------------------------------
namespace
{

int InsertPath(TestMusicDatabaseFixture::TestMusicDatabase& db, const std::string& path)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL("INSERT INTO path (strPath, strHash) VALUES ('%s', '')", path.c_str()));
  ds->query(db.PrepareSQL("SELECT idPath FROM path WHERE strPath = '%s'", path.c_str()));
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

int InsertArtist(TestMusicDatabaseFixture::TestMusicDatabase& db,
                 const std::string& name,
                 const std::string& mbid = "")
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL(
      "INSERT INTO artist (strArtist, strMusicBrainzArtistID, strSortName) "
      "VALUES ('%s', '%s', '%s')",
      name.c_str(), mbid.c_str(), name.c_str()));
  ds->query(db.PrepareSQL("SELECT MAX(idArtist) FROM artist"));
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

int InsertAlbum(TestMusicDatabaseFixture::TestMusicDatabase& db,
                const std::string& title,
                const std::string& releaseType = "album")
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL(
      "INSERT INTO album (strAlbum, strArtistDisp, strReleaseType, strGenres) "
      "VALUES ('%s', 'Various', '%s', '')",
      title.c_str(), releaseType.c_str()));
  ds->query("SELECT MAX(idAlbum) FROM album");
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

int InsertSong(TestMusicDatabaseFixture::TestMusicDatabase& db,
               int idAlbum,
               int idPath,
               const std::string& title,
               const std::string& filename,
               int track = 1,
               int timesPlayed = 0,
               const std::string& lastPlayed = "")
{
  auto* ds = db.GetDataset();
  std::string sql = db.PrepareSQL(
      "INSERT INTO song (idAlbum, idPath, strTitle, strFileName, iTrack, "
      "strArtistDisp, strGenres, iDuration, iTimesPlayed, lastplayed, "
      "strMusicBrainzTrackID, iStartOffset, iEndOffset, "
      "rating, userrating, votes, comment, mood, iBPM, iBitRate, "
      "iSampleRate, iChannels, strReplayGain, strVideoURL) "
      "VALUES (%i, %i, '%s', '%s', %i, "
      "'Test Artist', 'Rock', 200, %i, %s, "
      "'', 0, 0, "
      "0.0, 0, 0, '', '', 0, 320, "
      "44100, 2, '', '')",
      idAlbum, idPath, title.c_str(), filename.c_str(), track,
      timesPlayed,
      lastPlayed.empty() ? "NULL" : ("'" + lastPlayed + "'").c_str());
  ds->exec(sql);
  ds->query("SELECT MAX(idSong) FROM song");
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

void InsertAlbumArtist(TestMusicDatabaseFixture::TestMusicDatabase& db,
                       int idArtist,
                       int idAlbum,
                       int iOrder = 0)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL(
      "INSERT INTO album_artist (idArtist, idAlbum, iOrder, strArtist) "
      "VALUES (%i, %i, %i, 'Artist')",
      idArtist, idAlbum, iOrder));
}

void InsertSongArtist(TestMusicDatabaseFixture::TestMusicDatabase& db,
                      int idArtist,
                      int idSong,
                      int idRole = 1,
                      int iOrder = 0)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL(
      "INSERT INTO song_artist (idArtist, idSong, idRole, iOrder, strArtist) "
      "VALUES (%i, %i, %i, %i, 'Artist')",
      idArtist, idSong, idRole, iOrder));
}

void InsertSongGenre(TestMusicDatabaseFixture::TestMusicDatabase& db,
                     int idGenre,
                     int idSong,
                     int iOrder = 0)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL(
      "INSERT INTO song_genre (idGenre, idSong, iOrder) VALUES (%i, %i, %i)",
      idGenre, idSong, iOrder));
}

int InsertGenre(TestMusicDatabaseFixture::TestMusicDatabase& db, const std::string& name)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL("INSERT INTO genre (strGenre) VALUES ('%s')", name.c_str()));
  ds->query("SELECT MAX(idGenre) FROM genre");
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

int InsertRole(TestMusicDatabaseFixture::TestMusicDatabase& db, const std::string& name)
{
  auto* ds = db.GetDataset();
  ds->exec(db.PrepareSQL("INSERT INTO role (strRole) VALUES ('%s')", name.c_str()));
  ds->query("SELECT MAX(idRole) FROM role");
  int id = ds->fv(0).get_asInt();
  ds->close();
  return id;
}

int CountRows(TestMusicDatabaseFixture::TestMusicDatabase& db, const std::string& table)
{
  return db.GetSingleValueInt("SELECT COUNT(*) FROM " + table);
}

} // anonymous namespace

// ===========================================================================
// Cleanup Tests
// ===========================================================================

TEST_F(TestMusicDatabaseFixture, CleanupAlbumsRemovesAlbumsWithNoSongs)
{
  // Album with no songs should be removed by CleanupOrphanedItems
  InsertAlbum(*m_musicDb, "Orphan Album");
  EXPECT_EQ(CountRows(*m_musicDb, "album"), 1);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "album"), 0);
}

TEST_F(TestMusicDatabaseFixture, CleanupAlbumsKeepsAlbumsWithSongs)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idAlbum = InsertAlbum(*m_musicDb, "Good Album");
  InsertSong(*m_musicDb, idAlbum, idPath, "Track 1", "track1.mp3");

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "album"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupAlbumsRemovesOnlyOrphanedAlbums)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idAlbumGood = InsertAlbum(*m_musicDb, "Good Album");
  InsertAlbum(*m_musicDb, "Orphan Album");
  InsertSong(*m_musicDb, idAlbumGood, idPath, "Track 1", "track1.mp3");

  EXPECT_EQ(CountRows(*m_musicDb, "album"), 2);
  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "album"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupArtistsRemovesOrphanedArtists)
{
  // Artist not linked to any song_artist or album_artist should be removed
  // Note: BLANKARTIST_ID (1) is always preserved
  InsertArtist(*m_musicDb, "Orphan Artist", "orphan-mbid-001");

  // Should have the [Missing] artist + our orphan
  int artistCount = CountRows(*m_musicDb, "artist");
  EXPECT_GE(artistCount, 2);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // Only [Missing] should remain
  EXPECT_EQ(CountRows(*m_musicDb, "artist"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupArtistsKeepsLinkedArtists)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idArtist = InsertArtist(*m_musicDb, "Linked Artist", "linked-mbid-001");
  int idAlbum = InsertAlbum(*m_musicDb, "Test Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Track 1", "track1.mp3");

  InsertSongArtist(*m_musicDb, idArtist, idSong);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // [Missing] + our linked artist
  EXPECT_EQ(CountRows(*m_musicDb, "artist"), 2);
}

TEST_F(TestMusicDatabaseFixture, CleanupArtistsKeepsAlbumArtists)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idArtist = InsertArtist(*m_musicDb, "Album Artist", "album-artist-mbid");
  int idAlbum = InsertAlbum(*m_musicDb, "Test Album");
  InsertSong(*m_musicDb, idAlbum, idPath, "Track 1", "track1.mp3");
  InsertAlbumArtist(*m_musicDb, idArtist, idAlbum);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // [Missing] + our album artist
  EXPECT_EQ(CountRows(*m_musicDb, "artist"), 2);
}

TEST_F(TestMusicDatabaseFixture, CleanupGenresRemovesOrphanedGenres)
{
  // Genre not in song_genre should be cleaned up
  InsertGenre(*m_musicDb, "Orphan Genre");
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), 1);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), 0);
}

TEST_F(TestMusicDatabaseFixture, CleanupGenresKeepsLinkedGenres)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idAlbum = InsertAlbum(*m_musicDb, "Test Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Track 1", "track1.mp3");
  int idGenre = InsertGenre(*m_musicDb, "Rock");
  InsertSongGenre(*m_musicDb, idGenre, idSong);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupRolesRemovesOrphanedRoles)
{
  // Role not in song_artist (other than default role 1) should be removed
  InsertRole(*m_musicDb, "Orphan Producer");
  // Default role (id=1, 'Artist') + our orphan
  EXPECT_EQ(CountRows(*m_musicDb, "role"), 2);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // Only default role should remain
  EXPECT_EQ(CountRows(*m_musicDb, "role"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupRolesKeepsDefaultRole)
{
  // Default 'Artist' role (id=1) is always preserved even with no songs
  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "role"), 1);
}

TEST_F(TestMusicDatabaseFixture, CleanupRolesKeepsUsedRoles)
{
  int idPath = InsertPath(*m_musicDb, "/music/test/");
  int idArtist = InsertArtist(*m_musicDb, "Test Artist", "test-mbid-001");
  int idAlbum = InsertAlbum(*m_musicDb, "Test Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Track 1", "track1.mp3");
  int idRole = InsertRole(*m_musicDb, "Producer");

  InsertSongArtist(*m_musicDb, idArtist, idSong, idRole);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // Default role + Producer
  EXPECT_EQ(CountRows(*m_musicDb, "role"), 2);
}

TEST_F(TestMusicDatabaseFixture, CleanupInfoSettingsRemovesOrphaned)
{
  auto* ds = m_musicDb->GetDataset();
  ds->exec("INSERT INTO infosetting (strScraperPath, strSettings) "
           "VALUES ('test.scraper', '')");
  EXPECT_EQ(CountRows(*m_musicDb, "infosetting"), 1);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "infosetting"), 0);
}

TEST_F(TestMusicDatabaseFixture, CleanupWithActiveDataLeavesEverythingIntact)
{
  // Build a fully connected graph: path -> song -> album, artist links, genre links, role links
  int idPath = InsertPath(*m_musicDb, "/music/complete/");
  int idArtist = InsertArtist(*m_musicDb, "Complete Artist", "complete-mbid");
  int idAlbum = InsertAlbum(*m_musicDb, "Complete Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Complete Track", "complete.mp3");
  int idGenre = InsertGenre(*m_musicDb, "Jazz");
  int idProducerRole = InsertRole(*m_musicDb, "Producer");

  InsertAlbumArtist(*m_musicDb, idArtist, idAlbum);
  InsertSongArtist(*m_musicDb, idArtist, idSong, 1); // default Artist role
  InsertSongArtist(*m_musicDb, idArtist, idSong, idProducerRole);
  InsertSongGenre(*m_musicDb, idGenre, idSong);

  // Snapshot counts before cleanup
  int albumsBefore = CountRows(*m_musicDb, "album");
  int artistsBefore = CountRows(*m_musicDb, "artist");
  int genresBefore = CountRows(*m_musicDb, "genre");
  int rolesBefore = CountRows(*m_musicDb, "role");

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  EXPECT_EQ(CountRows(*m_musicDb, "album"), albumsBefore);
  EXPECT_EQ(CountRows(*m_musicDb, "artist"), artistsBefore);
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), genresBefore);
  EXPECT_EQ(CountRows(*m_musicDb, "role"), rolesBefore);
}

TEST_F(TestMusicDatabaseFixture, CleanupHandlesEmptyDatabase)
{
  // Database with only default data (BLANKARTIST + default role) should not fail
  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
}

TEST_F(TestMusicDatabaseFixture, CleanupAlbumTriggerDeletesCascadedData)
{
  // When an orphan album is deleted, the tgrDeleteAlbum trigger should
  // cascade-delete album_artist and album_source rows
  int idArtist = InsertArtist(*m_musicDb, "Cascade Artist", "cascade-mbid");
  int idAlbum = InsertAlbum(*m_musicDb, "Cascade Album");
  InsertAlbumArtist(*m_musicDb, idArtist, idAlbum);

  EXPECT_EQ(CountRows(*m_musicDb, "album_artist"), 1);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // Album gone -> trigger should have cleaned album_artist
  EXPECT_EQ(CountRows(*m_musicDb, "album"), 0);
  EXPECT_EQ(CountRows(*m_musicDb, "album_artist"), 0);
}

// ===========================================================================
// Play Count / Stats Tests (via direct SQL, since IncrementPlayCount needs
// CServiceBroker path resolution)
// ===========================================================================

TEST_F(TestMusicDatabaseFixture, IncrementPlayCountViaSqlUpdatesCountAndDate)
{
  int idPath = InsertPath(*m_musicDb, "/music/stats/");
  int idAlbum = InsertAlbum(*m_musicDb, "Stats Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Play Me", "play.mp3", 1, 0);

  auto* ds = m_musicDb->GetDataset();

  // Simulate what IncrementPlayCount does
  ds->exec(m_musicDb->PrepareSQL(
      "UPDATE song SET iTimesPlayed = iTimesPlayed + 1, "
      "lastplayed = '2025-06-15 12:00:00' WHERE idSong = %i",
      idSong));

  ds->query(m_musicDb->PrepareSQL(
      "SELECT iTimesPlayed, lastplayed FROM song WHERE idSong = %i", idSong));
  EXPECT_EQ(ds->fv("iTimesPlayed").get_asInt(), 1);
  EXPECT_EQ(ds->fv("lastplayed").get_asString(), "2025-06-15 12:00:00");
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, MultiplePlayCountIncrements)
{
  int idPath = InsertPath(*m_musicDb, "/music/stats/");
  int idAlbum = InsertAlbum(*m_musicDb, "Stats Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Play Me", "play.mp3", 1, 5);

  auto* ds = m_musicDb->GetDataset();

  // Increment three more times
  for (int i = 0; i < 3; ++i)
  {
    ds->exec(m_musicDb->PrepareSQL(
        "UPDATE song SET iTimesPlayed = iTimesPlayed + 1 WHERE idSong = %i", idSong));
  }

  ds->query(m_musicDb->PrepareSQL(
      "SELECT iTimesPlayed FROM song WHERE idSong = %i", idSong));
  EXPECT_EQ(ds->fv("iTimesPlayed").get_asInt(), 8); // 5 initial + 3
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, Top100QueryReturnsMostPlayedInOrder)
{
  int idPath = InsertPath(*m_musicDb, "/music/top/");
  int idAlbum = InsertAlbum(*m_musicDb, "Top Album");

  // Insert songs with varying play counts
  InsertSong(*m_musicDb, idAlbum, idPath, "Song A", "a.mp3", 1, 10, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "Song B", "b.mp3", 2, 50, "2025-02-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "Song C", "c.mp3", 3, 25, "2025-03-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "Song D", "d.mp3", 4, 0);

  // Query top played songs (mirrors GetTop100 SQL)
  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strTitle, iTimesPlayed FROM songview "
            "WHERE iTimesPlayed > 0 "
            "ORDER BY iTimesPlayed DESC LIMIT 100");

  // Should have 3 rows (Song D has 0 plays), ordered by play count desc
  EXPECT_EQ(ds->num_rows(), 3);

  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Song B");
  ds->next();
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Song C");
  ds->next();
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Song A");

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, Top100QueryExcludesUnplayedSongs)
{
  int idPath = InsertPath(*m_musicDb, "/music/top/");
  int idAlbum = InsertAlbum(*m_musicDb, "Unplayed Album");

  InsertSong(*m_musicDb, idAlbum, idPath, "Never Played", "never.mp3", 1, 0);

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strTitle FROM songview WHERE iTimesPlayed > 0 "
            "ORDER BY iTimesPlayed DESC LIMIT 100");
  EXPECT_EQ(ds->num_rows(), 0);
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, Top100QueryLimitsTo100Results)
{
  int idPath = InsertPath(*m_musicDb, "/music/top/");
  int idAlbum = InsertAlbum(*m_musicDb, "Big Album");

  // Insert 110 songs with play counts
  for (int i = 1; i <= 110; ++i)
  {
    std::string title = "Song " + std::to_string(i);
    std::string file = "song" + std::to_string(i) + ".mp3";
    InsertSong(*m_musicDb, idAlbum, idPath, title, file, i, i, "2025-01-01 00:00:00");
  }

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strTitle FROM songview WHERE iTimesPlayed > 0 "
            "ORDER BY iTimesPlayed DESC LIMIT 100");
  EXPECT_EQ(ds->num_rows(), 100);
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, RecentlyPlayedQueryReturnsByDate)
{
  int idPath = InsertPath(*m_musicDb, "/music/recent/");
  int idAlbum = InsertAlbum(*m_musicDb, "Recent Album");

  InsertSong(*m_musicDb, idAlbum, idPath, "Old Song", "old.mp3", 1, 5, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "New Song", "new.mp3", 2, 3, "2025-06-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "Mid Song", "mid.mp3", 3, 2, "2025-03-15 00:00:00");

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strTitle, lastplayed FROM songview "
            "WHERE iTimesPlayed > 0 AND lastplayed IS NOT NULL "
            "ORDER BY lastplayed DESC");

  EXPECT_EQ(ds->num_rows(), 3);
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "New Song");
  ds->next();
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Mid Song");
  ds->next();
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Old Song");
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, RecentlyPlayedAlbumsViaSongLastPlayed)
{
  // albumview.lastplayed is MAX(song.lastplayed) for songs in that album
  int idPath = InsertPath(*m_musicDb, "/music/recent/");
  int idAlbum1 = InsertAlbum(*m_musicDb, "Album Old");
  int idAlbum2 = InsertAlbum(*m_musicDb, "Album New");

  InsertSong(*m_musicDb, idAlbum1, idPath, "T1", "t1.mp3", 1, 2, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum2, idPath, "T2", "t2.mp3", 1, 1, "2025-06-01 00:00:00");

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strAlbum, lastplayed FROM albumview "
            "WHERE lastplayed IS NOT NULL "
            "ORDER BY lastplayed DESC");

  EXPECT_EQ(ds->num_rows(), 2);
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Album New");
  ds->next();
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Album Old");
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, RecentlyAddedAlbumsOrderedByDateAdded)
{
  int idPath = InsertPath(*m_musicDb, "/music/added/");
  int idAlbum1 = InsertAlbum(*m_musicDb, "First Added");
  int idAlbum2 = InsertAlbum(*m_musicDb, "Second Added");

  // Insert songs so albums show in songview; dateAdded on songs drives album dateAdded
  InsertSong(*m_musicDb, idAlbum1, idPath, "T1", "t1.mp3");
  InsertSong(*m_musicDb, idAlbum2, idPath, "T2", "t2.mp3");

  auto* ds = m_musicDb->GetDataset();
  // Both albums have dateAdded set by triggers; check ordering
  ds->query("SELECT strAlbum FROM album WHERE strAlbum != '' "
            "ORDER BY dateAdded DESC LIMIT 25");

  EXPECT_GE(ds->num_rows(), 2);
  // Second Added was inserted after First Added, so it should be first
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Second Added");
  ds->next();
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "First Added");
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, AlbumTimesPlayedIsAverageOfSongs)
{
  // albumview.iTimesPlayed = ROUND(AVG(song.iTimesPlayed))
  int idPath = InsertPath(*m_musicDb, "/music/avg/");
  int idAlbum = InsertAlbum(*m_musicDb, "Avg Album");

  InsertSong(*m_musicDb, idAlbum, idPath, "T1", "t1.mp3", 1, 10, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "T2", "t2.mp3", 2, 20, "2025-01-01 00:00:00");

  auto* ds = m_musicDb->GetDataset();
  ds->query(m_musicDb->PrepareSQL(
      "SELECT iTimesPlayed FROM albumview WHERE idAlbum = %i", idAlbum));
  // AVG(10, 20) = 15, ROUND(15) = 15
  EXPECT_EQ(ds->fv("iTimesPlayed").get_asInt(), 15);
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, AlbumLastPlayedIsMaxOfSongs)
{
  int idPath = InsertPath(*m_musicDb, "/music/max/");
  int idAlbum = InsertAlbum(*m_musicDb, "Max Album");

  InsertSong(*m_musicDb, idAlbum, idPath, "T1", "t1.mp3", 1, 1, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum, idPath, "T2", "t2.mp3", 2, 1, "2025-12-31 23:59:59");

  auto* ds = m_musicDb->GetDataset();
  ds->query(m_musicDb->PrepareSQL(
      "SELECT lastplayed FROM albumview WHERE idAlbum = %i", idAlbum));
  EXPECT_EQ(ds->fv("lastplayed").get_asString(), "2025-12-31 23:59:59");
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, UserRatingUpdatePersists)
{
  int idPath = InsertPath(*m_musicDb, "/music/rating/");
  int idAlbum = InsertAlbum(*m_musicDb, "Rating Album");
  int idSong = InsertSong(*m_musicDb, idAlbum, idPath, "Rate Me", "rate.mp3");

  EXPECT_TRUE(m_musicDb->SetSongUserrating(idSong, 8));

  auto* ds = m_musicDb->GetDataset();
  ds->query(m_musicDb->PrepareSQL(
      "SELECT userrating FROM song WHERE idSong = %i", idSong));
  EXPECT_EQ(ds->fv("userrating").get_asInt(), 8);
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, AlbumUserRatingUpdatePersists)
{
  int idAlbum = InsertAlbum(*m_musicDb, "Rated Album");

  EXPECT_TRUE(m_musicDb->SetAlbumUserrating(idAlbum, 7));

  auto* ds = m_musicDb->GetDataset();
  ds->query(m_musicDb->PrepareSQL(
      "SELECT iUserrating FROM album WHERE idAlbum = %i", idAlbum));
  EXPECT_EQ(ds->fv("iUserrating").get_asInt(), 7);
  ds->close();
}

// ===========================================================================
// Data Conversion Tests
//
// GetSongFromDataset, GetAlbumFromDataset, GetArtistFromDataset are private
// in CMusicDatabase.  We verify the view schemas store and return all fields
// correctly by querying via named columns.  This validates that:
//   1. The view joins produce the expected column set
//   2. Data round-trips correctly through INSERT -> view SELECT
//   3. Column names used by the private conversion methods exist and
//      return the correct types
// ===========================================================================

TEST_F(TestMusicDatabaseFixture, SongViewReturnsAllExpectedFields)
{
  int idPath = InsertPath(*m_musicDb, "/music/schema/");
  int idAlbum = InsertAlbum(*m_musicDb, "Schema Album");

  auto* ds = m_musicDb->GetDataset();
  ds->exec(m_musicDb->PrepareSQL(
      "INSERT INTO song (idAlbum, idPath, strTitle, strFileName, iTrack, "
      "strArtistDisp, strArtistSort, strGenres, iDuration, iTimesPlayed, "
      "lastplayed, strMusicBrainzTrackID, iStartOffset, iEndOffset, "
      "rating, userrating, votes, comment, mood, iBPM, iBitRate, "
      "iSampleRate, iChannels, strReplayGain, strVideoURL, "
      "strDiscSubtitle, strReleaseDate, strOrigReleaseDate) "
      "VALUES (%i, %i, 'Schema Title', 'schema.mp3', %i, "
      "'Artist Disp', 'Artist Sort', 'Rock', 300, 42, "
      "'2025-03-15 10:30:00', 'mbid-track-123', 100, 200, "
      "7.5, 8, 100, 'Great song', 'Happy', 120, 320, "
      "44100, 2, 'gain-data', 'http://video', "
      "'Disc A', '2025-01-15', '2024-06-01')",
      idAlbum, idPath, (1 << 16) | 3));

  ds->query("SELECT * FROM songview ORDER BY idSong DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  // Verify all named columns accessible in songview
  EXPECT_GT(ds->fv("idSong").get_asInt(), 0);
  EXPECT_EQ(ds->fv("strArtists").get_asString(), "Artist Disp");
  EXPECT_EQ(ds->fv("strArtistSort").get_asString(), "Artist Sort");
  EXPECT_EQ(ds->fv("strGenres").get_asString(), "Rock");
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Schema Title");
  EXPECT_EQ(ds->fv("iTrack").get_asInt(), (1 << 16) | 3);
  EXPECT_EQ(ds->fv("iDuration").get_asInt(), 300);
  EXPECT_EQ(ds->fv("iTimesPlayed").get_asInt(), 42);
  EXPECT_EQ(ds->fv("lastplayed").get_asString(), "2025-03-15 10:30:00");
  EXPECT_FLOAT_EQ(ds->fv("rating").get_asFloat(), 7.5f);
  EXPECT_EQ(ds->fv("userrating").get_asInt(), 8);
  EXPECT_EQ(ds->fv("votes").get_asInt(), 100);
  EXPECT_EQ(ds->fv("comment").get_asString(), "Great song");
  EXPECT_EQ(ds->fv("mood").get_asString(), "Happy");
  EXPECT_EQ(ds->fv("iBPM").get_asInt(), 120);
  EXPECT_EQ(ds->fv("iBitRate").get_asInt(), 320);
  EXPECT_EQ(ds->fv("iSampleRate").get_asInt(), 44100);
  EXPECT_EQ(ds->fv("iChannels").get_asInt(), 2);
  EXPECT_EQ(ds->fv("strDiscSubtitle").get_asString(), "Disc A");
  EXPECT_EQ(ds->fv("strReleaseDate").get_asString(), "2025-01-15");
  EXPECT_EQ(ds->fv("strOrigReleaseDate").get_asString(), "2024-06-01");
  EXPECT_EQ(ds->fv("strMusicBrainzTrackID").get_asString(), "mbid-track-123");
  EXPECT_EQ(ds->fv("strFileName").get_asString(), "schema.mp3");
  EXPECT_EQ(ds->fv("strPath").get_asString(), "/music/schema/");
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Schema Album");
  EXPECT_EQ(ds->fv("idAlbum").get_asInt(), idAlbum);
  EXPECT_EQ(ds->fv("strVideoURL").get_asString(), "http://video");
  EXPECT_EQ(ds->fv("strReplayGain").get_asString(), "gain-data");

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, AlbumViewReturnsAllExpectedFields)
{
  auto* ds = m_musicDb->GetDataset();
  ds->exec("INSERT INTO album (strAlbum, strMusicBrainzAlbumID, strReleaseGroupMBID, "
           "strArtistDisp, strArtistSort, strGenres, "
           "strReleaseDate, strOrigReleaseDate, bBoxedSet, "
           "strMoods, strStyles, strThemes, strReview, strLabel, strType, "
           "strReleaseStatus, fRating, iUserrating, iVotes, "
           "bCompilation, strReleaseType, iDiscTotal, iAlbumDuration) "
           "VALUES ('Schema Album', 'mbid-album-456', 'rg-mbid-789', "
           "'Album Artist', 'Artist Sort', 'Rock / Blues', "
           "'2025-03-01', '2024-01-01', 1, "
           "'Calm', 'Classic', 'Love', 'Great album', 'Test Label', 'Studio', "
           "'Official', 8.5, 9, 200, "
           "1, 'album', 2, 3600)");

  ds->query("SELECT * FROM albumview ORDER BY idAlbum DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  EXPECT_GT(ds->fv("idAlbum").get_asInt(), 0);
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Schema Album");
  EXPECT_EQ(ds->fv("strMusicBrainzAlbumID").get_asString(), "mbid-album-456");
  EXPECT_EQ(ds->fv("strReleaseGroupMBID").get_asString(), "rg-mbid-789");
  EXPECT_EQ(ds->fv("strArtists").get_asString(), "Album Artist");
  EXPECT_EQ(ds->fv("strArtistSort").get_asString(), "Artist Sort");
  EXPECT_EQ(ds->fv("strGenres").get_asString(), "Rock / Blues");
  EXPECT_EQ(ds->fv("strReleaseDate").get_asString(), "2025-03-01");
  EXPECT_EQ(ds->fv("strOrigReleaseDate").get_asString(), "2024-01-01");
  EXPECT_EQ(ds->fv("bBoxedSet").get_asInt(), 1);
  EXPECT_EQ(ds->fv("strMoods").get_asString(), "Calm");
  EXPECT_EQ(ds->fv("strStyles").get_asString(), "Classic");
  EXPECT_EQ(ds->fv("strThemes").get_asString(), "Love");
  EXPECT_EQ(ds->fv("strReview").get_asString(), "Great album");
  EXPECT_EQ(ds->fv("strLabel").get_asString(), "Test Label");
  EXPECT_EQ(ds->fv("strType").get_asString(), "Studio");
  EXPECT_EQ(ds->fv("strReleaseStatus").get_asString(), "Official");
  EXPECT_FLOAT_EQ(ds->fv("fRating").get_asFloat(), 8.5f);
  EXPECT_EQ(ds->fv("iUserrating").get_asInt(), 9);
  EXPECT_EQ(ds->fv("iVotes").get_asInt(), 200);
  EXPECT_EQ(ds->fv("bCompilation").get_asInt(), 1);
  EXPECT_EQ(ds->fv("strReleaseType").get_asString(), "album");
  EXPECT_EQ(ds->fv("iDiscTotal").get_asInt(), 2);
  EXPECT_EQ(ds->fv("iAlbumDuration").get_asInt(), 3600);

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, ArtistViewReturnsAllExpectedFields)
{
  auto* ds = m_musicDb->GetDataset();
  ds->exec("INSERT INTO artist (strArtist, strMusicBrainzArtistID, strSortName, "
           "strType, strGender, strDisambiguation, "
           "strBorn, strFormed, strGenres, strMoods, strStyles, "
           "strInstruments, strBiography, strDied, strDisbanded, "
           "strYearsActive, strImage, bScrapedMBID) "
           "VALUES ('Schema Artist', 'mbid-artist-abc', 'Artist Schema', "
           "'Person', 'Male', 'The one from London', "
           "'1970-01-01', '1990-05-01', 'Rock / Pop', 'Energetic', 'Classic Rock', "
           "'Guitar / Vocals', 'A legendary artist', '2030-12-31', '', "
           "'1990-2030', 'http://image', 1)");

  ds->query("SELECT * FROM artistview ORDER BY idArtist DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  EXPECT_GT(ds->fv("idArtist").get_asInt(), 0);
  EXPECT_EQ(ds->fv("strArtist").get_asString(), "Schema Artist");
  EXPECT_EQ(ds->fv("strMusicBrainzArtistID").get_asString(), "mbid-artist-abc");
  EXPECT_EQ(ds->fv("strSortName").get_asString(), "Artist Schema");
  EXPECT_EQ(ds->fv("strType").get_asString(), "Person");
  EXPECT_EQ(ds->fv("strGender").get_asString(), "Male");
  EXPECT_EQ(ds->fv("strDisambiguation").get_asString(), "The one from London");
  EXPECT_EQ(ds->fv("strBorn").get_asString(), "1970-01-01");
  EXPECT_EQ(ds->fv("strFormed").get_asString(), "1990-05-01");
  EXPECT_EQ(ds->fv("strGenres").get_asString(), "Rock / Pop");
  EXPECT_EQ(ds->fv("strMoods").get_asString(), "Energetic");
  EXPECT_EQ(ds->fv("strStyles").get_asString(), "Classic Rock");
  EXPECT_EQ(ds->fv("strInstruments").get_asString(), "Guitar / Vocals");
  EXPECT_EQ(ds->fv("strBiography").get_asString(), "A legendary artist");
  EXPECT_EQ(ds->fv("strDied").get_asString(), "2030-12-31");
  EXPECT_EQ(ds->fv("strYearsActive").get_asString(), "1990-2030");
  EXPECT_EQ(ds->fv("strImage").get_asString(), "http://image");
  EXPECT_EQ(ds->fv("bScrapedMBID").get_asInt(), 1);

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, SongViewNullLastPlayedReturnsEmpty)
{
  int idPath = InsertPath(*m_musicDb, "/music/null/");
  int idAlbum = InsertAlbum(*m_musicDb, "Null Album");
  InsertSong(*m_musicDb, idAlbum, idPath, "Null Song", "null.mp3", 1, 0);

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT * FROM songview ORDER BY idSong DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  EXPECT_EQ(ds->fv("iTimesPlayed").get_asInt(), 0);
  // NULL lastplayed comes back as empty string from the dataset
  EXPECT_TRUE(ds->fv("lastplayed").get_asString().empty());

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, AlbumViewEmptyNameIsEmpty)
{
  auto* ds = m_musicDb->GetDataset();
  ds->exec("INSERT INTO album (strAlbum, strArtistDisp, strReleaseType, strGenres) "
           "VALUES ('', 'Nobody', 'album', '')");

  ds->query("SELECT * FROM albumview ORDER BY idAlbum DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  // albumview stores raw value; GetAlbumFromDataset replaces empty with localized string
  EXPECT_TRUE(ds->fv("strAlbum").get_asString().empty());

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, SongViewJoinsPathAndAlbumCorrectly)
{
  int idPath = InsertPath(*m_musicDb, "/music/join/");
  int idAlbum = InsertAlbum(*m_musicDb, "Join Album");
  InsertSong(*m_musicDb, idAlbum, idPath, "Join Song", "join.mp3");

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strPath, strAlbum, strTitle FROM songview "
            "ORDER BY idSong DESC LIMIT 1");
  ASSERT_FALSE(ds->eof());

  EXPECT_EQ(ds->fv("strPath").get_asString(), "/music/join/");
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Join Album");
  EXPECT_EQ(ds->fv("strTitle").get_asString(), "Join Song");

  ds->close();
}

TEST_F(TestMusicDatabaseFixture, CleanupMultipleOrphanedGenresAtOnce)
{
  InsertGenre(*m_musicDb, "Orphan A");
  InsertGenre(*m_musicDb, "Orphan B");
  InsertGenre(*m_musicDb, "Orphan C");
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), 3);

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());
  EXPECT_EQ(CountRows(*m_musicDb, "genre"), 0);
}

TEST_F(TestMusicDatabaseFixture, CleanupMultipleOrphanedArtistsAtOnce)
{
  InsertArtist(*m_musicDb, "Orphan A", "orphan-a");
  InsertArtist(*m_musicDb, "Orphan B", "orphan-b");
  InsertArtist(*m_musicDb, "Orphan C", "orphan-c");

  EXPECT_TRUE(m_musicDb->CleanupOrphanedItems());

  // Only [Missing] should remain
  EXPECT_EQ(CountRows(*m_musicDb, "artist"), 1);
}

TEST_F(TestMusicDatabaseFixture, Top100AlbumsQueryReturnsMostPlayedAlbums)
{
  int idPath = InsertPath(*m_musicDb, "/music/top-albums/");

  int idAlbum1 = InsertAlbum(*m_musicDb, "Hot Album");
  int idAlbum2 = InsertAlbum(*m_musicDb, "Cold Album");

  // Hot Album: average play count = (20+30)/2 = 25
  InsertSong(*m_musicDb, idAlbum1, idPath, "H1", "h1.mp3", 1, 20, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum1, idPath, "H2", "h2.mp3", 2, 30, "2025-01-01 00:00:00");

  // Cold Album: average play count = (2+4)/2 = 3
  InsertSong(*m_musicDb, idAlbum2, idPath, "C1", "c1.mp3", 1, 2, "2025-01-01 00:00:00");
  InsertSong(*m_musicDb, idAlbum2, idPath, "C2", "c2.mp3", 2, 4, "2025-01-01 00:00:00");

  auto* ds = m_musicDb->GetDataset();
  ds->query("SELECT strAlbum, iTimesPlayed FROM albumview "
            "WHERE strAlbum != '' AND iTimesPlayed > 0 "
            "ORDER BY iTimesPlayed DESC LIMIT 100");

  EXPECT_EQ(ds->num_rows(), 2);
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Hot Album");
  ds->next();
  EXPECT_EQ(ds->fv("strAlbum").get_asString(), "Cold Album");
  ds->close();
}
