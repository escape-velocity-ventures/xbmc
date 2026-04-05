/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "dbwrappers/test/TestMusicDatabaseFixture.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

//! \brief Test fixture for MusicDatabase navigation, query, and search methods.
//!
//! Extends TestMusicDatabaseFixture with helpers to seed test data via direct
//! SQL, avoiding CServiceBroker dependencies in the CRUD layer. Each test gets
//! a fresh in-memory database with the full music schema.
class TestMusicDatabaseNav : public TestMusicDatabaseFixture
{
protected:
  //! \brief Insert a genre row and return its id.
  int SeedGenre(int id, const std::string& name)
  {
    m_musicDb->ExecuteQuery(
        StringUtils::Format("INSERT INTO genre (idGenre, strGenre) VALUES ({}, '{}')", id, name));
    return id;
  }

  //! \brief Insert an artist row and return its id.
  int SeedArtist(int id, const std::string& name, const std::string& sortName = "")
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO artist (idArtist, strArtist, strSortName, strMusicBrainzArtistID) "
        "VALUES ({}, '{}', '{}', '')",
        id, name, sortName));
    return id;
  }

  //! \brief Insert a path row and return its id.
  int SeedPath(int id, const std::string& path)
  {
    m_musicDb->ExecuteQuery(
        StringUtils::Format("INSERT INTO path (idPath, strPath) VALUES ({}, '{}')", id, path));
    return id;
  }

  //! \brief Insert a source row and return its id.
  int SeedSource(int id, const std::string& name)
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO source (idSource, strName, strMultipath) VALUES ({}, '{}', '{}')", id, name,
        "multipath://" + name));
    return id;
  }

  //! \brief Insert an album row and return its id.
  int SeedAlbum(int id,
                const std::string& title,
                const std::string& artistDisp = "",
                const std::string& releaseDate = "",
                const std::string& origReleaseDate = "",
                const std::string& genres = "",
                const std::string& label = "",
                const std::string& type = "")
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO album (idAlbum, strAlbum, strArtistDisp, strReleaseDate, "
        "strOrigReleaseDate, strGenres, strLabel, strType, strReleaseType) "
        "VALUES ({}, '{}', '{}', '{}', '{}', '{}', '{}', '{}', 'album')",
        id, title, artistDisp, releaseDate, origReleaseDate, genres, label, type));
    return id;
  }

  //! \brief Insert a song row and return its id.
  int SeedSong(int id,
               int idAlbum,
               int idPath,
               const std::string& title,
               const std::string& artistDisp = "",
               const std::string& genres = "",
               int track = 1,
               int duration = 200,
               const std::string& releaseDate = "",
               const std::string& fileName = "")
  {
    std::string fname = fileName.empty() ? StringUtils::Format("song{}.mp3", id) : fileName;
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO song (idSong, idAlbum, idPath, strTitle, strArtistDisp, strGenres, "
        "iTrack, iDuration, strReleaseDate, strOrigReleaseDate, strFileName, "
        "strMusicBrainzTrackID, iTimesPlayed, iStartOffset, iEndOffset, "
        "rating, votes, userrating, comment, mood, iBPM, iBitRate, iSampleRate, iChannels) "
        "VALUES ({}, {}, {}, '{}', '{}', '{}', {}, {}, '{}', '', '{}', '', 0, 0, 0, "
        "0.0, 0, 0, '', '', 0, 0, 0, 0)",
        id, idAlbum, idPath, title, artistDisp, genres, track, duration, releaseDate, fname));
    return id;
  }

  //! \brief Link a song to a genre.
  void SeedSongGenre(int idGenre, int idSong, int order = 0)
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO song_genre (idGenre, idSong, iOrder) VALUES ({}, {}, {})", idGenre, idSong,
        order));
  }

  //! \brief Link an artist to an album.
  void SeedAlbumArtist(int idArtist, int idAlbum, const std::string& name, int order = 0)
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO album_artist (idArtist, idAlbum, iOrder, strArtist) "
        "VALUES ({}, {}, {}, '{}')",
        idArtist, idAlbum, order, name));
  }

  //! \brief Link an artist to a song with a role.
  void SeedSongArtist(int idArtist,
                       int idSong,
                       int idRole,
                       const std::string& name,
                       int order = 0)
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO song_artist (idArtist, idSong, idRole, iOrder, strArtist) "
        "VALUES ({}, {}, {}, {}, '{}')",
        idArtist, idSong, idRole, order, name));
  }

  //! \brief Insert a role row.
  void SeedRole(int id, const std::string& name)
  {
    // Role 1 (Artist) is auto-created by schema. Only insert others.
    if (id != 1)
    {
      m_musicDb->ExecuteQuery(StringUtils::Format(
          "INSERT INTO role (idRole, strRole) VALUES ({}, '{}')", id, name));
    }
  }

  //! \brief Link album to source.
  void SeedAlbumSource(int idAlbum, int idSource)
  {
    m_musicDb->ExecuteQuery(StringUtils::Format(
        "INSERT INTO album_source (idSource, idAlbum) VALUES ({}, {})", idSource, idAlbum));
  }

  //! \brief Seed a complete minimal music library for navigation tests.
  //!
  //! Creates:
  //!   - 3 genres (Rock, Jazz, Pop)
  //!   - 3 artists (Alice, Bob, Charlie)
  //!   - 1 path (/music/)
  //!   - 2 albums (Alpha by Alice, Beta by Bob)
  //!   - 4 songs (2 per album)
  //!   - genre/artist/song links
  //!   - 1 source
  //!   - 2 roles (Artist [built-in], Composer)
  void SeedLibrary()
  {
    // Genres
    SeedGenre(1, "Rock");
    SeedGenre(2, "Jazz");
    SeedGenre(3, "Pop");

    // Artists (ids start at 2 because id 1 is [Missing] artist created by schema)
    SeedArtist(2, "Alice");
    SeedArtist(3, "Bob");
    SeedArtist(4, "Charlie");

    // Path
    SeedPath(1, "/music/");

    // Source
    SeedSource(1, "MyMusic");

    // Albums
    SeedAlbum(1, "Alpha", "Alice", "2020", "2019", "Rock", "Indie Label", "studio");
    SeedAlbum(2, "Beta", "Bob", "2021", "2021", "Jazz", "Jazz Records", "live");

    // Album-artist links
    SeedAlbumArtist(2, 1, "Alice");
    SeedAlbumArtist(3, 2, "Bob");

    // Album-source links
    SeedAlbumSource(1, 1);
    SeedAlbumSource(2, 1);

    // Roles (role 1 = Artist already exists from schema)
    SeedRole(2, "Composer");

    // Songs
    SeedSong(1, 1, 1, "Anthem", "Alice", "Rock", 1, 240, "2020", "anthem.mp3");
    SeedSong(2, 1, 1, "Breeze", "Alice", "Rock", 2, 180, "2020", "breeze.mp3");
    SeedSong(3, 2, 1, "Cascade", "Bob", "Jazz", 1, 300, "2021", "cascade.mp3");
    SeedSong(4, 2, 1, "Dawn", "Bob", "Jazz", 2, 210, "2021", "dawn.mp3");

    // Song-genre links
    SeedSongGenre(1, 1); // Anthem -> Rock
    SeedSongGenre(1, 2); // Breeze -> Rock
    SeedSongGenre(2, 3); // Cascade -> Jazz
    SeedSongGenre(2, 4); // Dawn -> Jazz

    // Song-artist links (role 1 = Artist)
    SeedSongArtist(2, 1, 1, "Alice"); // Alice sings Anthem
    SeedSongArtist(2, 2, 1, "Alice"); // Alice sings Breeze
    SeedSongArtist(3, 3, 1, "Bob");   // Bob sings Cascade
    SeedSongArtist(3, 4, 1, "Bob");   // Bob sings Dawn

    // Charlie is a composer on Anthem
    SeedSongArtist(4, 1, 2, "Charlie");
  }
};

// =========================================================================
// GetGenresNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetGenresNavReturnsAllGenres)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items));
  // 3 genres seeded: Rock, Jazz, Pop. But only Rock and Jazz have songs linked.
  // GetGenresNav returns all genres from the genre table (no join filtering
  // by default), but filters out empty genre names.
  EXPECT_EQ(items.Size(), 3);
}

TEST_F(TestMusicDatabaseNav, GetGenresNavEmptyDatabase)
{
  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetGenresNavSetsGenreOnMusicInfoTag)
{
  SeedGenre(1, "Metal");
  SeedGenre(2, "Blues");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items));
  ASSERT_GE(items.Size(), 1);

  // Each item should have genre set in its MusicInfoTag
  bool foundMetal = false;
  bool foundBlues = false;
  for (int i = 0; i < items.Size(); ++i)
  {
    const auto& genres = items.Get(i)->GetMusicInfoTag()->GetGenre();
    if (!genres.empty())
    {
      if (genres[0] == "Metal")
        foundMetal = true;
      if (genres[0] == "Blues")
        foundBlues = true;
    }
  }
  EXPECT_TRUE(foundMetal);
  EXPECT_TRUE(foundBlues);
}

TEST_F(TestMusicDatabaseNav, GetGenresNavSetsDatabaseId)
{
  SeedGenre(42, "Funk");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetMusicInfoTag()->GetDatabaseId(), 42);
}

TEST_F(TestMusicDatabaseNav, GetGenresNavCountOnly)
{
  SeedGenre(1, "Rock");
  SeedGenre(2, "Pop");
  SeedGenre(3, "Jazz");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items, Filter(), true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetProperty("total").asInteger(), 3);
}

TEST_F(TestMusicDatabaseNav, GetGenresNavInvalidUrlReturnsFalse)
{
  CFileItemList items;
  EXPECT_FALSE(m_musicDb->GetGenresNav("invalid://url/", items));
}

// =========================================================================
// GetSourcesNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSourcesNavReturnsAllSources)
{
  SeedSource(1, "Local");
  SeedSource(2, "Network");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetSourcesNav("musicdb://sources/", items));
  EXPECT_EQ(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetSourcesNavEmptyDatabase)
{
  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetSourcesNav("musicdb://sources/", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetSourcesNavCountOnly)
{
  SeedSource(1, "Local");
  SeedSource(2, "Network");
  SeedSource(3, "NAS");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetSourcesNav("musicdb://sources/", items, Filter(), true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetProperty("total").asInteger(), 3);
}

// =========================================================================
// GetRolesNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetRolesNavReturnsUsedRoles)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetRolesNav("musicdb://roles/", items));
  // Should have 2 roles: Artist (1) and Composer (2),
  // both referenced by song_artist entries.
  EXPECT_EQ(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetRolesNavEmptyDatabase)
{
  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetRolesNav("musicdb://roles/", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetRolesNavSetsDatabaseId)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetRolesNav("musicdb://roles/", items));
  ASSERT_GE(items.Size(), 1);

  bool foundArtistRole = false;
  for (int i = 0; i < items.Size(); ++i)
  {
    if (items.Get(i)->GetMusicInfoTag()->GetDatabaseId() == 1)
    {
      foundArtistRole = true;
      break;
    }
  }
  EXPECT_TRUE(foundArtistRole);
}

TEST_F(TestMusicDatabaseNav, GetRolesNavOnlyReturnsRolesWithSongArtistLinks)
{
  // Add a role with no song_artist references
  SeedRole(10, "Producer");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetRolesNav("musicdb://roles/", items));
  // Producer role has no song_artist links, so it should NOT appear
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// GetArtistsNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetArtistsNavReturnsAllArtists)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetArtistsNav("musicdb://artists/", items, sort, false));

  // Alice, Bob are song artists. Charlie is a composer.
  // By default (albumArtistsOnly=false), all artists with song_artist entries
  // should be returned.
  EXPECT_GE(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetArtistsNavEmptyDatabase)
{
  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetArtistsNav("musicdb://artists/", items, sort));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetArtistsNavFilterByGenre)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by Rock genre (id=1)
  EXPECT_TRUE(m_musicDb->GetArtistsNav("musicdb://artists/", items, sort, false, 1));

  // Only Alice has Rock songs
  bool foundAlice = false;
  for (int i = 0; i < items.Size(); ++i)
  {
    if (items.Get(i)->GetLabel() == "Alice")
      foundAlice = true;
  }
  EXPECT_TRUE(foundAlice);
}

TEST_F(TestMusicDatabaseNav, GetArtistsNavFilterByAlbum)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by album Beta (id=2)
  EXPECT_TRUE(m_musicDb->GetArtistsNav("musicdb://artists/", items, sort, false, -1, 2));
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetArtistsNavCountOnly)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(
      m_musicDb->GetArtistsNav("musicdb://artists/", items, sort, false, -1, -1, -1, Filter(), true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_GE(items.Get(0)->GetProperty("total").asInteger(), 2);
}

// =========================================================================
// GetAlbumsNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetAlbumsNavReturnsAllAlbums)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort));
  EXPECT_EQ(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsNavEmptyDatabase)
{
  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsNavFilterByGenre)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by Jazz genre (id=2)
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort, 2));
  EXPECT_GE(items.Size(), 1);

  bool foundBeta = false;
  for (int i = 0; i < items.Size(); ++i)
  {
    if (items.Get(i)->GetMusicInfoTag()->GetAlbum() == "Beta")
      foundBeta = true;
  }
  EXPECT_TRUE(foundBeta);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsNavFilterByArtist)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by Alice (id=2)
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort, -1, 2));
  EXPECT_GE(items.Size(), 1);

  bool foundAlpha = false;
  for (int i = 0; i < items.Size(); ++i)
  {
    if (items.Get(i)->GetMusicInfoTag()->GetAlbum() == "Alpha")
      foundAlpha = true;
  }
  EXPECT_TRUE(foundAlpha);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsNavCountOnly)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort, -1, -1, Filter(), true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetProperty("total").asInteger(), 2);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsNavInvalidUrlReturnsFalse)
{
  CFileItemList items;
  SortDescription sort;
  EXPECT_FALSE(m_musicDb->GetAlbumsNav("invalid://bad/", items, sort));
}

// =========================================================================
// GetAlbumsByYear
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetAlbumsByYearReturnsMatchingAlbums)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetAlbumsByYear("musicdb://albums/", items, 2020));
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsByYearNoMatch)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetAlbumsByYear("musicdb://albums/", items, 1999));
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// GetSongsNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSongsNavReturnsAllSongs)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, -1, -1, -1));
  EXPECT_EQ(items.Size(), 4);
}

TEST_F(TestMusicDatabaseNav, GetSongsNavEmptyDatabase)
{
  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, -1, -1, -1));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetSongsNavFilterByGenre)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by Rock genre (id=1)
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, 1, -1, -1));
  // Anthem and Breeze are Rock songs
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetSongsNavFilterByArtist)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by Bob (id=3)
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, -1, 3, -1));
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetSongsNavFilterByAlbum)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Filter by album Alpha (id=1)
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, -1, -1, 1));
  EXPECT_EQ(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetSongsNavInvalidUrlReturnsFalse)
{
  CFileItemList items;
  SortDescription sort;
  EXPECT_FALSE(m_musicDb->GetSongsNav("badurl://x/", items, sort, -1, -1, -1));
}

// =========================================================================
// GetSongsByYear
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSongsByYearReturnsMatchingSongs)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetSongsByYear("musicdb://songs/", items, 2020));
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetSongsByYearNoMatch)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetSongsByYear("musicdb://songs/", items, 1900));
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// GetDiscsNav
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetDiscsNavForAlbum)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  // Get discs for album Alpha (id=1)
  EXPECT_TRUE(m_musicDb->GetDiscsNav("musicdb://albums/1/", items, sort, 1));
  // Alpha has songs but disc numbering comes from iTrack encoding.
  // With iTrack=1 and iTrack=2 (no disc encoding), disc defaults to 1.
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, GetDiscsNavEmptyAlbum)
{
  SeedPath(1, "/music/");
  SeedAlbum(99, "EmptyAlbum", "Nobody", "2023");

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetDiscsNav("musicdb://albums/99/", items, sort, 99));
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// GetAlbumTypesNav / GetMusicLabelsNav (common nav)
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetAlbumTypesNavReturnsTypes)
{
  SeedAlbum(1, "A1", "Art1", "2020", "", "", "", "studio");
  SeedAlbum(2, "A2", "Art2", "2021", "", "", "", "live");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetAlbumTypesNav("musicdb://albums/", items));
  EXPECT_GE(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetAlbumTypesNavEmptyDatabase)
{
  CFileItemList items;
  // GetCommonNav returns false when no rows are found
  EXPECT_FALSE(m_musicDb->GetAlbumTypesNav("musicdb://albums/", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, GetMusicLabelsNavReturnsLabels)
{
  SeedAlbum(1, "A1", "Art1", "2020", "", "", "Indie Label");
  SeedAlbum(2, "A2", "Art2", "2021", "", "", "Jazz Records");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetMusicLabelsNav("musicdb://albums/", items));
  EXPECT_GE(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetMusicLabelsNavEmptyDatabase)
{
  CFileItemList items;
  // GetCommonNav returns false when no rows are found
  EXPECT_FALSE(m_musicDb->GetMusicLabelsNav("musicdb://albums/", items));
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// GetArtistsByWhereJSON
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetArtistsByWhereJSONReturnsArtists)
{
  SeedLibrary();

  std::set<std::string, std::less<>> fields;
  fields.insert("artist");
  fields.insert("sortname");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetArtistsByWhereJSON(fields, "musicdb://artists/", result, total, sort));
  EXPECT_GE(total, 2);
}

TEST_F(TestMusicDatabaseNav, GetArtistsByWhereJSONEmptyDatabase)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("artist");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetArtistsByWhereJSON(fields, "musicdb://artists/", result, total, sort));
  // Total should be the number of artists. The [Missing] artist exists
  // from schema creation but typically is excluded by filtering.
  EXPECT_GE(total, 0);
}

TEST_F(TestMusicDatabaseNav, GetArtistsByWhereJSONInvalidUrl)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("artist");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_FALSE(
      m_musicDb->GetArtistsByWhereJSON(fields, "invalid://url/", result, total, sort));
}

// =========================================================================
// GetAlbumsByWhereJSON
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetAlbumsByWhereJSONReturnsAlbums)
{
  SeedLibrary();

  std::set<std::string, std::less<>> fields;
  fields.insert("album");
  fields.insert("year");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetAlbumsByWhereJSON(fields, "musicdb://albums/", result, total, sort));
  EXPECT_EQ(total, 2);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsByWhereJSONEmptyDatabase)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("album");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetAlbumsByWhereJSON(fields, "musicdb://albums/", result, total, sort));
  EXPECT_EQ(total, 0);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsByWhereJSONInvalidUrl)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("album");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_FALSE(
      m_musicDb->GetAlbumsByWhereJSON(fields, "invalid://url/", result, total, sort));
}

// =========================================================================
// GetSongsByWhereJSON
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSongsByWhereJSONReturnsSongs)
{
  SeedLibrary();

  std::set<std::string, std::less<>> fields;
  fields.insert("title");
  fields.insert("track");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetSongsByWhereJSON(fields, "musicdb://songs/", result, total, sort));
  EXPECT_EQ(total, 4);
}

TEST_F(TestMusicDatabaseNav, GetSongsByWhereJSONEmptyDatabase)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("title");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_TRUE(
      m_musicDb->GetSongsByWhereJSON(fields, "musicdb://songs/", result, total, sort));
  EXPECT_EQ(total, 0);
}

TEST_F(TestMusicDatabaseNav, GetSongsByWhereJSONInvalidUrl)
{
  std::set<std::string, std::less<>> fields;
  fields.insert("title");

  CVariant result;
  int total = 0;
  SortDescription sort;

  EXPECT_FALSE(
      m_musicDb->GetSongsByWhereJSON(fields, "invalid://url/", result, total, sort));
}

// =========================================================================
// GetGenresJSON
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetGenresJSONReturnsGenres)
{
  SeedGenre(1, "Rock");
  SeedGenre(2, "Pop");

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresJSON(items));
  EXPECT_GE(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetGenresJSONEmptyDatabase)
{
  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresJSON(items));
  EXPECT_EQ(items.Size(), 0);
}

// =========================================================================
// Search
// =========================================================================

TEST_F(TestMusicDatabaseNav, SearchFindsMatchingArtistAlbumSong)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("A", items));
  // "A" matches "Alice" (artist), "Alpha" (album), "Anthem" (song)
  // Search does prefix matching with LIKE 'A%'
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, SearchPartialMatchWithLongQuery)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("Ali", items));
  // "Ali" is >= MIN_FULL_SEARCH_LENGTH (3), so also does middle-of-word matching.
  // Should find Alice.
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, SearchEmptyStringReturnsTrue)
{
  SeedLibrary();

  CFileItemList items;
  // Empty search still returns true (searches complete but finds nothing).
  EXPECT_TRUE(m_musicDb->Search("", items));
}

TEST_F(TestMusicDatabaseNav, SearchNoMatchReturnsTrue)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("ZZZNONEXISTENT", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, SearchShortQueryPrefixOnly)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("An", items));
  // "An" is < MIN_FULL_SEARCH_LENGTH (3), so only prefix match (LIKE 'An%').
  // Should find "Anthem" as a song title.
  EXPECT_GE(items.Size(), 1);
}

TEST_F(TestMusicDatabaseNav, SearchSpecialCharsDoesNotCrash)
{
  SeedLibrary();

  CFileItemList items;
  // SQL injection attempt should be handled by PrepareSQL
  EXPECT_TRUE(m_musicDb->Search("'; DROP TABLE song; --", items));
}

TEST_F(TestMusicDatabaseNav, SearchEmptyDatabaseReturnsTrue)
{
  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("test", items));
  EXPECT_EQ(items.Size(), 0);
}

TEST_F(TestMusicDatabaseNav, SearchFindsSongByTitle)
{
  SeedLibrary();

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->Search("Cascade", items));
  EXPECT_GE(items.Size(), 1);
}

// =========================================================================
// GetSongsFullByWhere / GetAlbumsByWhere / GetArtistsByWhere
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSongsFullByWhereReturnsAll)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetSongsFullByWhere("musicdb://songs/", items, sort, filter, false));
  EXPECT_EQ(items.Size(), 4);
}

TEST_F(TestMusicDatabaseNav, GetSongsFullByWhereWithArtistData)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetSongsFullByWhere("musicdb://songs/", items, sort, filter, true));
  EXPECT_EQ(items.Size(), 4);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsByWhereReturnsAll)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetAlbumsByWhere("musicdb://albums/", items, sort, filter));
  EXPECT_EQ(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetAlbumsByWhereCountOnly)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetAlbumsByWhere("musicdb://albums/", items, sort, filter, true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetProperty("total").asInteger(), 2);
}

TEST_F(TestMusicDatabaseNav, GetArtistsByWhereReturnsAll)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetArtistsByWhere("musicdb://artists/", items, sort, filter));
  // Should return artists with album_artist or song_artist links
  EXPECT_GE(items.Size(), 2);
}

TEST_F(TestMusicDatabaseNav, GetArtistsByWhereCountOnly)
{
  SeedLibrary();

  CFileItemList items;
  SortDescription sort;
  Filter filter;
  EXPECT_TRUE(m_musicDb->GetArtistsByWhere("musicdb://artists/", items, sort, filter, true));
  ASSERT_EQ(items.Size(), 1);
  EXPECT_GE(items.Get(0)->GetProperty("total").asInteger(), 2);
}

// =========================================================================
// GetSongsCount / GetDiscsCount
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetSongsCountReturnsCorrectCount)
{
  SeedLibrary();

  EXPECT_EQ(m_musicDb->GetSongsCount(Filter()), 4);
}

TEST_F(TestMusicDatabaseNav, GetSongsCountEmptyDatabase)
{
  EXPECT_EQ(m_musicDb->GetSongsCount(Filter()), 0);
}

TEST_F(TestMusicDatabaseNav, GetDiscsCountForAlbum)
{
  SeedLibrary();

  int count = m_musicDb->GetDiscsCount("musicdb://albums/1/");
  EXPECT_GE(count, 1);
}

// =========================================================================
// Multiple genres per song
// =========================================================================

TEST_F(TestMusicDatabaseNav, GenreNavWithMultipleGenresPerSong)
{
  SeedGenre(1, "Rock");
  SeedGenre(2, "Alternative");
  SeedArtist(2, "TestArtist");
  SeedPath(1, "/music/");
  SeedAlbum(1, "MultiGenre", "TestArtist", "2022");
  SeedSong(1, 1, 1, "CrossoverSong", "TestArtist", "Rock / Alternative");
  SeedSongGenre(1, 1, 0); // Rock
  SeedSongGenre(2, 1, 1); // Alternative

  CFileItemList items;
  EXPECT_TRUE(m_musicDb->GetGenresNav("musicdb://genres/", items));
  EXPECT_EQ(items.Size(), 2);
}

// =========================================================================
// Edge cases: single item results
// =========================================================================

TEST_F(TestMusicDatabaseNav, GetAlbumsNavSingleAlbum)
{
  SeedPath(1, "/music/");
  SeedArtist(2, "Solo");
  SeedAlbum(1, "OnlyOne", "Solo", "2023");
  SeedAlbumArtist(2, 1, "Solo");
  SeedSong(1, 1, 1, "Lone Track", "Solo", "Indie");
  SeedSongArtist(2, 1, 1, "Solo");

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetAlbumsNav("musicdb://albums/", items, sort));
  EXPECT_EQ(items.Size(), 1);
  EXPECT_EQ(items.Get(0)->GetMusicInfoTag()->GetAlbum(), "OnlyOne");
}

TEST_F(TestMusicDatabaseNav, GetSongsNavSingleSong)
{
  SeedPath(1, "/music/");
  SeedArtist(2, "Solo");
  SeedAlbum(1, "OnlyOne", "Solo", "2023");
  SeedSong(1, 1, 1, "The One", "Solo");
  SeedSongArtist(2, 1, 1, "Solo");

  CFileItemList items;
  SortDescription sort;
  EXPECT_TRUE(m_musicDb->GetSongsNav("musicdb://songs/", items, sort, -1, -1, -1));
  EXPECT_EQ(items.Size(), 1);
}
