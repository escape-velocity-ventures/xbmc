/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "dbwrappers/test/TestDatabaseFixture.h"
#include "dbwrappers/test/TestMusicDatabaseFixture.h"
#include "dbwrappers/test/TestVideoDatabaseFixture.h"

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Base fixture: in-memory SQLite connection tests
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, OpenAndCloseWithoutErrors)
{
  // SetUp already opened the connection -- verify the primitives are valid
  EXPECT_NE(nullptr, m_db->GetDatabase());
  EXPECT_NE(nullptr, m_db->GetDataset());
  EXPECT_NE(nullptr, m_db->GetDataset2());
  EXPECT_TRUE(m_db->GetDatabase()->isActive());
}

TEST_F(TestDatabaseFixture, InitializeMinimalSchema)
{
  // The base TestDatabase has empty CreateTables/CreateAnalytics,
  // but InitializeSchema still creates the version table.
  ASSERT_TRUE(m_db->InitializeSchema());

  auto* ds = m_db->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT idVersion FROM version");
  ASSERT_GT(ds->num_rows(), 0);
  EXPECT_EQ(1, ds->fv("idVersion").get_asInt());
  ds->close();
}

TEST_F(TestDatabaseFixture, BasicCrudOnAdhocTable)
{
  auto* ds = m_db->GetDataset();
  ASSERT_NE(nullptr, ds);

  // CREATE
  ds->exec("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT, value INTEGER)");

  // INSERT
  m_db->ExecuteQuery("INSERT INTO test_table (id, name, value) VALUES (1, 'alpha', 100)");
  m_db->ExecuteQuery("INSERT INTO test_table (id, name, value) VALUES (2, 'beta', 200)");

  // READ
  ds->query("SELECT name, value FROM test_table ORDER BY id");
  ASSERT_EQ(2, ds->num_rows());
  EXPECT_EQ("alpha", ds->fv("name").get_asString());
  EXPECT_EQ(100, ds->fv("value").get_asInt());
  ds->next();
  EXPECT_EQ("beta", ds->fv("name").get_asString());
  EXPECT_EQ(200, ds->fv("value").get_asInt());
  ds->close();

  // UPDATE
  m_db->ExecuteQuery("UPDATE test_table SET value = 999 WHERE id = 1");
  std::string result = m_db->GetSingleValue("test_table", "value", "id = 1");
  EXPECT_EQ("999", result);

  // DELETE
  m_db->ExecuteQuery("DELETE FROM test_table WHERE id = 2");
  int count = m_db->GetSingleValueInt("SELECT COUNT(*) FROM test_table");
  EXPECT_EQ(1, count);
}

TEST_F(TestDatabaseFixture, IsolationBetweenTests)
{
  // This test runs after BasicCrudOnAdhocTable in the same fixture class,
  // but each test gets a fresh database. Verify the adhoc table does NOT exist.
  auto* ds = m_db->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='table' AND name='test_table'");
  EXPECT_EQ(0, ds->num_rows());
  ds->close();
}

// ---------------------------------------------------------------------------
// Music database fixture: schema creation tests
// ---------------------------------------------------------------------------

TEST_F(TestMusicDatabaseFixture, SchemaCreatesSuccessfully)
{
  // If we got here, SetUp succeeded -- the full music schema was created.
  // Verify a sampling of key tables exist.
  auto* ds = m_musicDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
  ASSERT_GT(ds->num_rows(), 0);

  // Collect all table names
  std::vector<std::string> tables;
  while (!ds->eof())
  {
    tables.push_back(ds->fv("name").get_asString());
    ds->next();
  }
  ds->close();

  // Verify core music tables are present
  auto hasTable = [&tables](const std::string& name)
  {
    return std::find(tables.begin(), tables.end(), name) != tables.end();
  };

  EXPECT_TRUE(hasTable("artist"));
  EXPECT_TRUE(hasTable("album"));
  EXPECT_TRUE(hasTable("song"));
  EXPECT_TRUE(hasTable("genre"));
  EXPECT_TRUE(hasTable("path"));
  EXPECT_TRUE(hasTable("album_artist"));
  EXPECT_TRUE(hasTable("song_artist"));
  EXPECT_TRUE(hasTable("song_genre"));
  EXPECT_TRUE(hasTable("role"));
  EXPECT_TRUE(hasTable("art"));
  EXPECT_TRUE(hasTable("version"));
  EXPECT_TRUE(hasTable("versiontagscan"));
}

TEST_F(TestMusicDatabaseFixture, BlankArtistExists)
{
  // CMusicDatabase::CreateTables inserts a blank artist with id=1
  auto* ds = m_musicDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT idArtist, strArtist FROM artist WHERE idArtist = 1");
  ASSERT_EQ(1, ds->num_rows());
  EXPECT_EQ(1, ds->fv("idArtist").get_asInt());
  ds->close();
}

TEST_F(TestMusicDatabaseFixture, DefaultRoleExists)
{
  // CreateTables inserts a default 'Artist' role with id=1
  std::string role = m_musicDb->GetSingleValue("role", "strRole", "idRole = 1");
  EXPECT_EQ("Artist", role);
}

TEST_F(TestMusicDatabaseFixture, CrudSmoke)
{
  // INSERT a genre
  m_musicDb->ExecuteQuery("INSERT INTO genre (idGenre, strGenre) VALUES (1, 'Rock')");

  // READ it back
  std::string genre = m_musicDb->GetSingleValue("genre", "strGenre", "idGenre = 1");
  EXPECT_EQ("Rock", genre);

  // UPDATE
  m_musicDb->ExecuteQuery("UPDATE genre SET strGenre = 'Alternative Rock' WHERE idGenre = 1");
  genre = m_musicDb->GetSingleValue("genre", "strGenre", "idGenre = 1");
  EXPECT_EQ("Alternative Rock", genre);

  // DELETE
  m_musicDb->ExecuteQuery("DELETE FROM genre WHERE idGenre = 1");
  int count = m_musicDb->GetSingleValueInt("SELECT COUNT(*) FROM genre");
  EXPECT_EQ(0, count);
}

TEST_F(TestMusicDatabaseFixture, IndicesExist)
{
  // Verify that CreateAnalytics created at least some indices
  auto* ds = m_musicDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'idx%'");
  EXPECT_GT(ds->num_rows(), 0);
  ds->close();
}

// ---------------------------------------------------------------------------
// Video database fixture: schema creation tests
// ---------------------------------------------------------------------------

TEST_F(TestVideoDatabaseFixture, SchemaCreatesSuccessfully)
{
  // If we got here, SetUp succeeded -- the full video schema was created.
  auto* ds = m_videoDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
  ASSERT_GT(ds->num_rows(), 0);

  std::vector<std::string> tables;
  while (!ds->eof())
  {
    tables.push_back(ds->fv("name").get_asString());
    ds->next();
  }
  ds->close();

  auto hasTable = [&tables](const std::string& name)
  {
    return std::find(tables.begin(), tables.end(), name) != tables.end();
  };

  EXPECT_TRUE(hasTable("movie"));
  EXPECT_TRUE(hasTable("tvshow"));
  EXPECT_TRUE(hasTable("episode"));
  EXPECT_TRUE(hasTable("musicvideo"));
  EXPECT_TRUE(hasTable("files"));
  EXPECT_TRUE(hasTable("path"));
  EXPECT_TRUE(hasTable("bookmark"));
  EXPECT_TRUE(hasTable("settings"));
  EXPECT_TRUE(hasTable("genre"));
  EXPECT_TRUE(hasTable("actor"));
  EXPECT_TRUE(hasTable("art"));
  EXPECT_TRUE(hasTable("sets"));
  EXPECT_TRUE(hasTable("seasons"));
  EXPECT_TRUE(hasTable("rating"));
  EXPECT_TRUE(hasTable("uniqueid"));
  EXPECT_TRUE(hasTable("videoversion"));
  EXPECT_TRUE(hasTable("videoversiontype"));
  EXPECT_TRUE(hasTable("version"));
}

TEST_F(TestVideoDatabaseFixture, VideoVersionTypesPopulated)
{
  // CreateTables calls InitializeVideoVersionTypeTable which populates
  // the videoversiontype table with system-defined version types.
  int count = m_videoDb->GetSingleValueInt("SELECT COUNT(*) FROM videoversiontype");
  EXPECT_GT(count, 0);
}

TEST_F(TestVideoDatabaseFixture, CrudSmoke)
{
  // INSERT a path
  m_videoDb->ExecuteQuery(
      "INSERT INTO path (idPath, strPath) VALUES (1, '/movies/')");

  // READ
  std::string path = m_videoDb->GetSingleValue("path", "strPath", "idPath = 1");
  EXPECT_EQ("/movies/", path);

  // INSERT a file
  m_videoDb->ExecuteQuery(
      "INSERT INTO files (idFile, idPath, strFilename) VALUES (1, 1, 'movie.mkv')");

  // READ
  std::string filename = m_videoDb->GetSingleValue("files", "strFilename", "idFile = 1");
  EXPECT_EQ("movie.mkv", filename);

  // UPDATE
  m_videoDb->ExecuteQuery(
      "UPDATE files SET strFilename = 'movie_v2.mkv' WHERE idFile = 1");
  filename = m_videoDb->GetSingleValue("files", "strFilename", "idFile = 1");
  EXPECT_EQ("movie_v2.mkv", filename);

  // DELETE
  m_videoDb->ExecuteQuery("DELETE FROM files WHERE idFile = 1");
  int count = m_videoDb->GetSingleValueInt("SELECT COUNT(*) FROM files");
  EXPECT_EQ(0, count);
}

TEST_F(TestVideoDatabaseFixture, IndicesExist)
{
  auto* ds = m_videoDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'ix_%'");
  EXPECT_GT(ds->num_rows(), 0);
  ds->close();
}

TEST_F(TestVideoDatabaseFixture, ViewsExist)
{
  // CreateAnalytics should create views for the video database
  auto* ds = m_videoDb->GetDataset();
  ASSERT_NE(nullptr, ds);

  ds->query("SELECT name FROM sqlite_master WHERE type='view'");
  // Video database has views like movie_view, episode_view, etc.
  EXPECT_GT(ds->num_rows(), 0);
  ds->close();
}
