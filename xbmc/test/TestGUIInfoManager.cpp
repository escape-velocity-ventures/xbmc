/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIInfoManager.h"

#include "FileItem.h"
#include "ServiceBroker.h"
#include "StubSettingsComponent.h"
#include "TestServiceBrokerStub.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "utils/SystemInfo.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

/*!
 * \brief Fixture for testing CGUIInfoManager label and bool resolution.
 *
 * Extends TestServiceBrokerStub to provide CSettingsComponent via
 * CServiceBroker. Constructs a CGUIInfoManager and sets an empty
 * CFileItem as the current item so providers that dereference the
 * item pointer do not crash.
 *
 * NOTE: This fixture does NOT initialize CServiceManager, database
 * connections, or the windowing system. Tests must only query info
 * labels and booleans that resolve through static methods, OS-level
 * calls, or default member values of the application components.
 * Labels that depend on CDataCacheCore, CGUIWindowManager, or live
 * database connections are out of scope.
 */
class TestGUIInfoManager : public TestServiceBrokerStub
{
protected:
  void SetUp() override
  {
    TestServiceBrokerStub::SetUp();

    m_infoManager = std::make_unique<CGUIInfoManager>();

    // Set an empty CFileItem as the current item. Without this, info
    // providers that unconditionally dereference the item pointer
    // (CVideoGUIInfo, CPlayerGUIInfo, etc.) would crash on null.
    m_infoManager->SetCurrentItem(CFileItem());
  }

  void TearDown() override
  {
    m_infoManager->ResetCurrentItem();
    m_infoManager.reset();

    TestServiceBrokerStub::TearDown();
  }

  CGUIInfoManager& InfoManager() { return *m_infoManager; }

private:
  std::unique_ptr<CGUIInfoManager> m_infoManager;
};

// ===========================================================================
// System info labels (10 tests)
// ===========================================================================

/// \brief SYSTEM_BUILD_VERSION returns a non-empty version string.
TEST_F(TestGUIInfoManager, SystemBuildVersionReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_BUILD_VERSION, 0);
  EXPECT_FALSE(label.empty());
  // Cross-check against the static accessor
  EXPECT_EQ(label, CSysInfo::GetVersion());
}

/// \brief SYSTEM_BUILD_DATE returns a non-empty build date string.
TEST_F(TestGUIInfoManager, SystemBuildDateReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_BUILD_DATE, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_EQ(label, CSysInfo::GetBuildDate());
}

/// \brief SYSTEM_BUILD_VERSION_SHORT returns a non-empty short version.
TEST_F(TestGUIInfoManager, SystemBuildVersionShortReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_BUILD_VERSION_SHORT, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_EQ(label, CSysInfo::GetVersionShort());
}

/// \brief SYSTEM_BUILD_VERSION_CODE returns a non-empty version code.
TEST_F(TestGUIInfoManager, SystemBuildVersionCodeReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_BUILD_VERSION_CODE, 0);
  EXPECT_FALSE(label.empty());
}

/// \brief SYSTEM_BUILD_VERSION_GIT returns a non-empty git hash.
TEST_F(TestGUIInfoManager, SystemBuildVersionGitReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_BUILD_VERSION_GIT, 0);
  EXPECT_FALSE(label.empty());
}

/// \brief SYSTEM_TIME returns a non-empty time string.
TEST_F(TestGUIInfoManager, SystemTimeReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_TIME, 0);
  EXPECT_FALSE(label.empty());
}

/// \brief SYSTEM_DATE returns a non-empty date string.
TEST_F(TestGUIInfoManager, SystemDateReturnsNonEmpty)
{
  std::string label = InfoManager().GetLabel(SYSTEM_DATE, 0);
  EXPECT_FALSE(label.empty());
}

/// \brief SYSTEM_FREE_MEMORY returns a string ending in "MB".
TEST_F(TestGUIInfoManager, SystemFreeMemoryReturnsFormattedValue)
{
  std::string label = InfoManager().GetLabel(SYSTEM_FREE_MEMORY, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("MB"), std::string::npos);
}

/// \brief SYSTEM_TOTAL_MEMORY returns a string ending in "MB".
TEST_F(TestGUIInfoManager, SystemTotalMemoryReturnsFormattedValue)
{
  std::string label = InfoManager().GetLabel(SYSTEM_TOTAL_MEMORY, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("MB"), std::string::npos);
}

/// \brief SYSTEM_FPS returns "0.00" when no rendering is active.
TEST_F(TestGUIInfoManager, SystemFpsReturnsDefaultWhenNotRendering)
{
  std::string label = InfoManager().GetLabel(SYSTEM_FPS, 0);
  EXPECT_EQ(label, "0.00");
}

// ===========================================================================
// Player info labels with no active player (10 tests)
// ===========================================================================

/// \brief PLAYER_VOLUME returns a formatted dB string when no media is playing.
TEST_F(TestGUIInfoManager, PlayerVolumeReturnsDecibelString)
{
  std::string label = InfoManager().GetLabel(PLAYER_VOLUME, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("dB"), std::string::npos);
}

/// \brief PLAYER_CHAPTER returns "00" when no media is playing.
TEST_F(TestGUIInfoManager, PlayerChapterReturnsZeroDefault)
{
  std::string label = InfoManager().GetLabel(PLAYER_CHAPTER, 0);
  EXPECT_EQ(label, "00");
}

/// \brief PLAYER_CHAPTERCOUNT returns "00" when no media is playing.
TEST_F(TestGUIInfoManager, PlayerChapterCountReturnsZeroDefault)
{
  std::string label = InfoManager().GetLabel(PLAYER_CHAPTERCOUNT, 0);
  EXPECT_EQ(label, "00");
}

/// \brief PLAYER_CHAPTERNAME returns an empty string when no media is playing.
TEST_F(TestGUIInfoManager, PlayerChapterNameReturnsEmptyDefault)
{
  std::string label = InfoManager().GetLabel(PLAYER_CHAPTERNAME, 0);
  EXPECT_TRUE(label.empty());
}

/// \brief PLAYER_TITLE returns an empty string when the current item has no title.
TEST_F(TestGUIInfoManager, PlayerTitleReturnsEmptyForBlankItem)
{
  std::string label = InfoManager().GetLabel(PLAYER_TITLE, 0);
  EXPECT_TRUE(label.empty());
}

/// \brief PLAYER_PLAYSPEED returns "0.00" when no player is active.
///        CApplicationPlayer::GetPlaySpeed() returns 0 without a player.
TEST_F(TestGUIInfoManager, PlayerPlayspeedReturnsZeroWhenIdle)
{
  std::string label = InfoManager().GetLabel(PLAYER_PLAYSPEED, 0);
  EXPECT_EQ(label, "0.00");
}

/// \brief PLAYER_PROGRESS returns "0" when no media is playing.
TEST_F(TestGUIInfoManager, PlayerProgressReturnsZeroWhenIdle)
{
  std::string label = InfoManager().GetLabel(PLAYER_PROGRESS, 0);
  EXPECT_EQ(label, "0");
}

/// \brief PLAYER_SUBTITLE_DELAY returns "0.000 s" when no media is playing.
TEST_F(TestGUIInfoManager, PlayerSubtitleDelayReturnsZeroDefault)
{
  std::string label = InfoManager().GetLabel(PLAYER_SUBTITLE_DELAY, 0);
  EXPECT_EQ(label, "0.000 s");
}

/// \brief PLAYER_AUDIO_DELAY returns "0.000 s" when no media is playing.
TEST_F(TestGUIInfoManager, PlayerAudioDelayReturnsZeroDefault)
{
  std::string label = InfoManager().GetLabel(PLAYER_AUDIO_DELAY, 0);
  EXPECT_EQ(label, "0.000 s");
}

/// \brief PLAYER_FILENAME returns an empty string when the current item
///        has no path set.
TEST_F(TestGUIInfoManager, PlayerFilenameReturnsEmptyForBlankItem)
{
  std::string label = InfoManager().GetLabel(PLAYER_FILENAME, 0);
  EXPECT_TRUE(label.empty());
}

// ===========================================================================
// Invalid / out-of-range label IDs (3 tests)
// ===========================================================================

/// \brief An info ID of 0 returns an empty label.
TEST_F(TestGUIInfoManager, InvalidLabelIdZeroReturnsEmpty)
{
  std::string label = InfoManager().GetLabel(0, 0);
  EXPECT_TRUE(label.empty());
}

/// \brief An extremely large info ID (below special ranges) returns empty.
TEST_F(TestGUIInfoManager, InvalidLabelIdLargeReturnsEmpty)
{
  // Pick an ID well above the defined labels but below LISTITEM_START
  std::string label = InfoManager().GetLabel(34000, 0);
  EXPECT_TRUE(label.empty());
}

/// \brief A negative info ID returns an empty label.
TEST_F(TestGUIInfoManager, InvalidLabelIdNegativeReturnsEmpty)
{
  std::string label = InfoManager().GetLabel(-1, 0);
  EXPECT_TRUE(label.empty());
}

// ===========================================================================
// Boolean conditions -- system (7 tests)
// ===========================================================================

/// \brief SYSTEM_ALWAYS_TRUE evaluates to true.
TEST_F(TestGUIInfoManager, SystemAlwaysTrueReturnsTrue)
{
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_ALWAYS_TRUE, 0));
}

/// \brief SYSTEM_ALWAYS_FALSE evaluates to false.
TEST_F(TestGUIInfoManager, SystemAlwaysFalseReturnsFalse)
{
  EXPECT_FALSE(InfoManager().GetBool(SYSTEM_ALWAYS_FALSE, 0));
}

/// \brief SYSTEM_ETHERNET_LINK_ACTIVE is hardcoded to true in the
///        current implementation.
TEST_F(TestGUIInfoManager, SystemEthernetLinkActiveReturnsTrue)
{
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_ETHERNET_LINK_ACTIVE, 0));
}

/// \brief Exactly one platform boolean matches the current build target.
///        This test verifies that the platform booleans are consistent
///        (at least one is true, the rest match their compile-time defines).
TEST_F(TestGUIInfoManager, PlatformBoolMatchesBuildTarget)
{
#if defined(TARGET_LINUX) || defined(TARGET_FREEBSD)
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_PLATFORM_LINUX, 0));
#else
  EXPECT_FALSE(InfoManager().GetBool(SYSTEM_PLATFORM_LINUX, 0));
#endif

#ifdef TARGET_WINDOWS
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_PLATFORM_WINDOWS, 0));
#else
  EXPECT_FALSE(InfoManager().GetBool(SYSTEM_PLATFORM_WINDOWS, 0));
#endif

#ifdef TARGET_DARWIN
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_PLATFORM_DARWIN, 0));
#else
  EXPECT_FALSE(InfoManager().GetBool(SYSTEM_PLATFORM_DARWIN, 0));
#endif

#if defined(TARGET_ANDROID)
  EXPECT_TRUE(InfoManager().GetBool(SYSTEM_PLATFORM_ANDROID, 0));
#else
  EXPECT_FALSE(InfoManager().GetBool(SYSTEM_PLATFORM_ANDROID, 0));
#endif
}

/// \brief Negated conditions: negative condition IDs invert the result.
///        -SYSTEM_ALWAYS_TRUE should be false.
TEST_F(TestGUIInfoManager, NegatedConditionInvertsResult)
{
  EXPECT_FALSE(InfoManager().GetBool(-static_cast<int>(SYSTEM_ALWAYS_TRUE), 0));
}

/// \brief Negated SYSTEM_ALWAYS_FALSE should be true.
TEST_F(TestGUIInfoManager, NegatedAlwaysFalseReturnsTrue)
{
  EXPECT_TRUE(InfoManager().GetBool(-static_cast<int>(SYSTEM_ALWAYS_FALSE), 0));
}

/// \brief An invalid/unhandled boolean condition returns false.
TEST_F(TestGUIInfoManager, InvalidBoolConditionReturnsFalse)
{
  // Pick an ID in the valid range that no provider handles
  EXPECT_FALSE(InfoManager().GetBool(34000, 0));
}

// ===========================================================================
// Boolean conditions -- player state with no active player (8 tests)
// ===========================================================================

/// \brief PLAYER_HAS_MEDIA is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerHasMediaFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_HAS_MEDIA, 0));
}

/// \brief PLAYER_HAS_AUDIO is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerHasAudioFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_HAS_AUDIO, 0));
}

/// \brief PLAYER_HAS_VIDEO is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerHasVideoFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_HAS_VIDEO, 0));
}

/// \brief PLAYER_HAS_GAME is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerHasGameFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_HAS_GAME, 0));
}

/// \brief PLAYER_PLAYING is false when nothing is playing.
///        CApplicationPlayer::GetPlaySpeed() returns 0 without a player,
///        and PLAYER_PLAYING checks speed == 1.0f.
TEST_F(TestGUIInfoManager, PlayerPlayingFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_PLAYING, 0));
}

/// \brief PLAYER_PAUSED is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerPausedFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_PAUSED, 0));
}

/// \brief PLAYER_REWINDING is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerRewindingFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_REWINDING, 0));
}

/// \brief PLAYER_FORWARDING is false when nothing is playing.
TEST_F(TestGUIInfoManager, PlayerForwardingFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_FORWARDING, 0));
}

// ===========================================================================
// Boolean conditions -- library scanning status (3 tests)
//
// Library scanning checks use singleton queues that do not require
// CServiceManager or database initialization, making them safe to test.
//
// NOTE: LIBRARY_HAS_* booleans (LIBRARY_HAS_MUSIC, LIBRARY_HAS_MOVIES,
// etc.) are NOT tested here because they attempt to open a database via
// CServiceBroker::GetDatabaseManager(), which requires CServiceManager
// initialization. Those should be tested in integration-level tests
// using TestBasicEnvironment.
// ===========================================================================

/// \brief LIBRARY_IS_SCANNING is false when no scan is active.
TEST_F(TestGUIInfoManager, LibraryIsScanningFalseByDefault)
{
  EXPECT_FALSE(InfoManager().GetBool(LIBRARY_IS_SCANNING, 0));
}

/// \brief LIBRARY_IS_SCANNING_VIDEO is false when no video scan is active.
TEST_F(TestGUIInfoManager, LibraryIsScanningVideoFalseByDefault)
{
  EXPECT_FALSE(InfoManager().GetBool(LIBRARY_IS_SCANNING_VIDEO, 0));
}

/// \brief LIBRARY_IS_SCANNING_MUSIC is false when no music scan is active.
TEST_F(TestGUIInfoManager, LibraryIsScanningMusicFalseByDefault)
{
  EXPECT_FALSE(InfoManager().GetBool(LIBRARY_IS_SCANNING_MUSIC, 0));
}

// ===========================================================================
// Additional player boolean conditions (5 tests)
// ===========================================================================

/// \brief PLAYER_IS_REMOTE is false when no player is active.
TEST_F(TestGUIInfoManager, PlayerIsRemoteFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_IS_REMOTE, 0));
}

/// \brief PLAYER_IS_EXTERNAL is false when no player is active.
TEST_F(TestGUIInfoManager, PlayerIsExternalFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_IS_EXTERNAL, 0));
}

/// \brief PLAYER_CAN_PAUSE is false when no player is active.
TEST_F(TestGUIInfoManager, PlayerCanPauseFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_CAN_PAUSE, 0));
}

/// \brief PLAYER_CAN_SEEK is false when no player is active.
TEST_F(TestGUIInfoManager, PlayerCanSeekFalseWhenIdle)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_CAN_SEEK, 0));
}

/// \brief PLAYER_MUTED is false by default (volume is at maximum, not muted).
TEST_F(TestGUIInfoManager, PlayerMutedFalseByDefault)
{
  EXPECT_FALSE(InfoManager().GetBool(PLAYER_MUTED, 0));
}

// ===========================================================================
// Additional system info labels (5 tests)
// ===========================================================================

/// \brief SYSTEM_FREE_MEMORY_PERCENT returns a string ending in "%".
TEST_F(TestGUIInfoManager, SystemFreeMemoryPercentReturnsFormattedValue)
{
  std::string label = InfoManager().GetLabel(SYSTEM_FREE_MEMORY_PERCENT, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("%"), std::string::npos);
}

/// \brief SYSTEM_USED_MEMORY returns a string ending in "MB".
TEST_F(TestGUIInfoManager, SystemUsedMemoryReturnsFormattedValue)
{
  std::string label = InfoManager().GetLabel(SYSTEM_USED_MEMORY, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("MB"), std::string::npos);
}

/// \brief SYSTEM_USED_MEMORY_PERCENT returns a string ending in "%".
TEST_F(TestGUIInfoManager, SystemUsedMemoryPercentReturnsFormattedValue)
{
  std::string label = InfoManager().GetLabel(SYSTEM_USED_MEMORY_PERCENT, 0);
  EXPECT_FALSE(label.empty());
  EXPECT_NE(label.find("%"), std::string::npos);
}

/// \brief PLAYER_TIME returns a time string when no media is playing.
///        SecondsToTimeString(0) returns "00:00" with default format.
TEST_F(TestGUIInfoManager, PlayerTimeReturnsZeroTimeWhenIdle)
{
  std::string label = InfoManager().GetLabel(PLAYER_TIME, 0);
  // With 0 seconds and TIME_FORMAT_GUESS (total < 3600), format is MM:SS
  EXPECT_FALSE(label.empty());
}

/// \brief PLAYER_DURATION returns empty when no media is playing.
///        GetDuration() returns empty string when total play time is 0.
TEST_F(TestGUIInfoManager, PlayerDurationReturnsEmptyWhenIdle)
{
  std::string label = InfoManager().GetLabel(PLAYER_DURATION, 0);
  EXPECT_TRUE(label.empty());
}
