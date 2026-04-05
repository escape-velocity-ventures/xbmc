set(SOURCES TestAutoSwitch.cpp
            TestBasicEnvironment.cpp
            TestCueDocument.cpp
            TestDateTime.cpp
            TestDateTimeSpan.cpp
            TestFileItem.cpp
            TestHelpers.cpp
            TestLangInfo.cpp
            TestMediaSource.cpp
            TestURL.cpp
            TestUtil.cpp
            TestUtils.cpp)

set(HEADERS TestAssertions.h
            TestBasicEnvironment.h
            TestDataBuilder.h
            TestFileItemFactory.h
            TestUtils.h)

core_add_test_library(xbmc_test)
