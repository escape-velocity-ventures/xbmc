set(SOURCES TestDatabaseFixture.cpp
            TestVPrepare.cpp)

set(HEADERS TestDatabaseFixture.h
            TestMusicDatabaseFixture.h
            TestVideoDatabaseFixture.h)

core_add_test_library(utils_db_test)
