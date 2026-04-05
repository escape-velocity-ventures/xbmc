/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/test/TestDatabaseFixture.h"
#include "music/MusicDatabase.h"

#include <memory>

/*!
 * \brief GTest fixture that provides a fully initialized CMusicDatabase
 *        backed by an in-memory SQLite instance.
 *
 * On SetUp, the fixture:
 *   1. Opens an in-memory SQLite connection (no filesystem side effects)
 *   2. Calls CMusicDatabase::CreateTables() to build the complete music schema
 *   3. Calls CMusicDatabase::CreateAnalytics() to create indices and views
 *
 * Each test gets a completely fresh, isolated music database. The schema is
 * identical to what Kodi creates at runtime -- no DDL is duplicated.
 *
 * Usage:
 * \code
 *   TEST_F(TestMusicDatabaseFixture, InsertArtist)
 *   {
 *     m_musicDb->ExecuteQuery("INSERT INTO artist ...");
 *     // ...
 *   }
 * \endcode
 */
class TestMusicDatabaseFixture : public ::testing::Test
{
protected:
  /*!
   * \brief A test-only subclass of CMusicDatabase that can open an
   *        in-memory SQLite connection without going through
   *        CDatabase::Open() (which requires CDatabaseManager).
   */
  class TestMusicDatabase : public CMusicDatabase
  {
  public:
    TestMusicDatabase() = default;
    ~TestMusicDatabase() override = default;

    /*!
     * \brief Open an in-memory SQLite connection and initialize the
     *        complete music database schema.
     *
     * \return true if the connection was established and schema created
     *         successfully, false otherwise.
     */
    bool OpenInMemory()
    {
      auto db = std::make_unique<test::InMemorySqliteDatabase>();

      if (db->connect(true) != dbiplus::DB_CONNECTION_OK)
        return false;

      m_pDB = std::move(db);
      m_pDS.reset(m_pDB->CreateDataset());
      m_pDS2.reset(m_pDB->CreateDataset());

      if (!m_pDS || !m_pDS2)
        return false;

      m_pDB->postconnect();

      // Initialize schema (version table + CreateTables + CreateAnalytics)
      return CreateDatabase();
    }

    //! \brief Access the raw dbiplus::Database connection.
    dbiplus::Database* GetDatabase() { return m_pDB.get(); }

    //! \brief Access the primary dbiplus::Dataset for query execution.
    dbiplus::Dataset* GetDataset() { return m_pDS.get(); }
  };

  void SetUp() override
  {
    m_musicDb = std::make_unique<TestMusicDatabase>();
    ASSERT_TRUE(m_musicDb->OpenInMemory());
  }

  void TearDown() override
  {
    m_musicDb.reset();
  }

  std::unique_ptr<TestMusicDatabase> m_musicDb;
};
