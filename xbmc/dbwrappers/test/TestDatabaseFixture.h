/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/Database.h"
#include "dbwrappers/sqlitedataset.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <sqlite3.h>

namespace test
{

/*!
 * \brief A SqliteDatabase subclass that connects to an in-memory SQLite database.
 *
 * The production SqliteDatabase::setHostName and SqliteDatabase::setDatabase
 * manipulate paths in ways incompatible with SQLite's ":memory:" URI.
 * This subclass overrides those methods and connect() to open a pure
 * in-memory database, avoiding any filesystem side effects.
 */
class InMemorySqliteDatabase : public dbiplus::SqliteDatabase
{
public:
  InMemorySqliteDatabase() = default;
  ~InMemorySqliteDatabase() override = default;

  void setHostName(const char* /*newHost*/) override
  {
    // No-op: in-memory databases have no host path
  }

  void setDatabase(const char* /*newDb*/) override
  {
    // No-op: prevent the base class from appending ".db"
  }

  int connect(bool /*create*/) override
  {
    disconnect();

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY;
    int rc = sqlite3_open_v2(":memory:", &conn, flags, nullptr);
    if (rc != SQLITE_OK)
      return dbiplus::DB_CONNECTION_NONE;

    sqlite3_extended_result_codes(conn, 1);
    sqlite3_busy_handler(conn, nullptr, nullptr);

    const char* sqlcmd = "PRAGMA empty_result_callbacks=ON";
    rc = sqlite3_exec(conn, sqlcmd, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK)
    {
      sqlite3_close(conn);
      conn = nullptr;
      return dbiplus::DB_CONNECTION_NONE;
    }

    active = true;
    return dbiplus::DB_CONNECTION_OK;
  }
};

} // namespace test

/*!
 * \brief Base GTest fixture that opens an in-memory SQLite database through
 *        Kodi's CDatabase abstraction layer.
 *
 * Subclass this fixture (or use it directly) to get a fully functional
 * CDatabase with m_pDB, m_pDS, and m_pDS2 connected to a fresh in-memory
 * SQLite instance. Each test gets a completely isolated database that is
 * destroyed on TearDown -- no filesystem side effects.
 *
 * Usage from a derived fixture:
 * \code
 *   bool success = OpenInMemory();
 *   ASSERT_TRUE(success);
 *   // m_pDB, m_pDS, m_pDS2 are now usable
 * \endcode
 */
class TestDatabaseFixture : public ::testing::Test
{
protected:
  /*!
   * \brief A minimal CDatabase subclass for testing.
   *
   * Provides public access to the protected OpenInMemory helper and the
   * CreateDatabase / CreateTables / CreateAnalytics lifecycle.
   * The schema version and base name are configurable so this single class
   * can stand in for any database type during low-level connection tests.
   */
  class TestDatabase : public CDatabase
  {
  public:
    TestDatabase() = default;
    ~TestDatabase() override = default;

    /*!
     * \brief Open an in-memory SQLite connection, bypassing the normal
     *        Open() path that requires CServiceBroker::GetDatabaseManager().
     *
     * Creates the low-level dbiplus objects (Database, Dataset x2) with an
     * InMemorySqliteDatabase and marks the connection as open.
     *
     * \return true on success, false if the connection could not be established.
     */
    bool OpenInMemory()
    {
      auto db = std::make_unique<test::InMemorySqliteDatabase>();

      // Connect (create=true is implicit in InMemorySqliteDatabase)
      if (db->connect(true) != dbiplus::DB_CONNECTION_OK)
        return false;

      m_pDB = std::move(db);
      m_pDS.reset(m_pDB->CreateDataset());
      m_pDS2.reset(m_pDB->CreateDataset());

      if (!m_pDS || !m_pDS2)
        return false;

      m_pDB->postconnect();
      return true;
    }

    /*!
     * \brief Initialize the database schema by creating the version table,
     *        then calling CreateTables() and CreateAnalytics().
     *
     * This mirrors the logic of CDatabase::CreateDatabase() but is publicly
     * accessible from test code.
     *
     * \return true on success, false on failure.
     */
    bool InitializeSchema()
    {
      return CreateDatabase();
    }

    //! \brief Access the raw dbiplus::Database connection.
    dbiplus::Database* GetDatabase() { return m_pDB.get(); }

    //! \brief Access the primary dbiplus::Dataset for query execution.
    dbiplus::Dataset* GetDataset() { return m_pDS.get(); }

    //! \brief Access the secondary dbiplus::Dataset.
    dbiplus::Dataset* GetDataset2() { return m_pDS2.get(); }

  protected:
    // Minimal schema -- just enough for the base fixture tests.
    void CreateTables() override {}
    void CreateAnalytics() override {}
    int GetSchemaVersion() const override { return 1; }
    const char* GetBaseDBName() const override { return "TestDB"; }
  };

  void SetUp() override
  {
    m_db = std::make_unique<TestDatabase>();
    ASSERT_TRUE(m_db->OpenInMemory());
  }

  void TearDown() override
  {
    // Note: CDatabase::Close() is a no-op here because m_openCount is private
    // and was never incremented (we bypassed Open()). Cleanup happens via
    // unique_ptr destruction: SqliteDatabase's destructor calls disconnect()
    // which closes the sqlite3 handle.
    m_db.reset();
  }

  std::unique_ptr<TestDatabase> m_db;
};
