/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

/// \brief Abstract interface used to verify gmock integration.
class IGreeter
{
public:
  virtual ~IGreeter() = default;
  virtual std::string Greet(const std::string& name) = 0;
  virtual int GetGreetCount() const = 0;
};

/// \brief Mock implementation of IGreeter for gmock verification.
class CMockGreeter : public IGreeter
{
public:
  MOCK_METHOD(std::string, Greet, (const std::string& name), (override));
  MOCK_METHOD(int, GetGreetCount, (), (const, override));
};

// ---------------------------------------------------------------------------
// Basic EXPECT_CALL verification
// ---------------------------------------------------------------------------

TEST(TestGmockIntegration, ExpectCallVerification)
{
  CMockGreeter mock;

  EXPECT_CALL(mock, Greet("Kodi"))
      .Times(1)
      .WillOnce(::testing::Return("Hello, Kodi!"));

  std::string result = mock.Greet("Kodi");
  EXPECT_EQ(result, "Hello, Kodi!");
}

// ---------------------------------------------------------------------------
// Multiple expectations with WillRepeatedly
// ---------------------------------------------------------------------------

TEST(TestGmockIntegration, MultipleCallExpectations)
{
  CMockGreeter mock;

  EXPECT_CALL(mock, Greet(::testing::_))
      .Times(3)
      .WillRepeatedly(::testing::Return("Hi!"));

  EXPECT_CALL(mock, GetGreetCount())
      .WillOnce(::testing::Return(3));

  for (int i = 0; i < 3; ++i)
  {
    EXPECT_EQ(mock.Greet("anyone"), "Hi!");
  }

  EXPECT_EQ(mock.GetGreetCount(), 3);
}

// ---------------------------------------------------------------------------
// NiceMock ignores uninteresting calls without warnings
// ---------------------------------------------------------------------------

TEST(TestGmockIntegration, NiceMockUsage)
{
  ::testing::NiceMock<CMockGreeter> nice;

  // NiceMock returns default values for unconfigured calls.
  // For std::string the default is "".
  EXPECT_EQ(nice.Greet("anyone"), "");
  EXPECT_EQ(nice.GetGreetCount(), 0);

  // Explicit expectation still works on a NiceMock.
  EXPECT_CALL(nice, Greet("Kodi"))
      .WillOnce(::testing::Return("Nice hello"));

  EXPECT_EQ(nice.Greet("Kodi"), "Nice hello");
}

// ---------------------------------------------------------------------------
// StrictMock fails on any unexpected call
// ---------------------------------------------------------------------------

TEST(TestGmockIntegration, StrictMockUsage)
{
  ::testing::StrictMock<CMockGreeter> strict;

  // Every call must be explicitly expected.
  EXPECT_CALL(strict, Greet("Kodi"))
      .WillOnce(::testing::Return("Strict hello"));

  EXPECT_CALL(strict, GetGreetCount())
      .WillOnce(::testing::Return(1));

  EXPECT_EQ(strict.Greet("Kodi"), "Strict hello");
  EXPECT_EQ(strict.GetGreetCount(), 1);
}
