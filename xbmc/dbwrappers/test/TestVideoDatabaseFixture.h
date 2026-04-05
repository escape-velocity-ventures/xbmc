/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/test/TestDatabaseFixture.h"
#include "video/VideoDatabase.h"

#include <memory>

/*!
 * \brief GTest fixture that provides a fully initialized CVideoDatabase
 *        backed by an in-memory SQLite instance.
 *
 * On SetUp, the fixture:
 *   1. Opens an in-memory SQLite connection (no filesystem side effects)
 *   2. Calls CVideoDatabase::CreateTables() to build the complete video schema
 *      (including the videoversiontype initialization)
 *   3. Calls CVideoDatabase::CreateAnalytics() to create indices, triggers,
 *      and views
 *
 * Each test gets a completely fresh, isolated video database. The schema is
 * identical to what Kodi creates at runtime -- no DDL is duplicated.
 *
 * Usage:
 * \code
 *   TEST_F(TestVideoDatabaseFixture, InsertMovie)
 *   {
 *     m_videoDb->ExecuteQuery("INSERT INTO movie ...");
 *     // ...
 *   }
 * \endcode
 */
class TestVideoDatabaseFixture : public ::testing::Test
{
protected:
  /*!
   * \brief A test-only subclass of CVideoDatabase that can open an
   *        in-memory SQLite connection without going through
   *        CDatabase::Open() (which requires CDatabaseManager).
   */
  class TestVideoDatabase : public CVideoDatabase
  {
  public:
    TestVideoDatabase() = default;
    ~TestVideoDatabase() override = default;

    /*!
     * \brief Open an in-memory SQLite connection and initialize the
     *        complete video database schema.
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
    m_videoDb = std::make_unique<TestVideoDatabase>();
    ASSERT_TRUE(m_videoDb->OpenInMemory());
  }

  void TearDown() override
  {
    m_videoDb.reset();
  }

  std::unique_ptr<TestVideoDatabase> m_videoDb;
};
