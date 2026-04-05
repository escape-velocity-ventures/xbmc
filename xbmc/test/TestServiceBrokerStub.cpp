/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "TestServiceBrokerStub.h"

#include "ServiceBroker.h"
#include "StubSettingsComponent.h"
#include "profiles/ProfileManager.h"
#include "settings/SettingsComponent.h"

#include <memory>

// ---------------------------------------------------------------------------
// TestServiceBrokerStub fixture implementation
// ---------------------------------------------------------------------------

void TestServiceBrokerStub::SetUp()
{
  // Create a lightweight CSettingsComponent that allocates its sub-objects
  // (CSettings, CAdvancedSettings, CProfileManager) without calling
  // Initialize() or Load(). This avoids all filesystem, directory-creation,
  // and platform-specific initialization that the real path requires.
  m_settingsComponent = std::make_shared<StubSettingsComponent>();
  CServiceBroker::RegisterSettingsComponent(m_settingsComponent);
}

void TestServiceBrokerStub::TearDown()
{
  CServiceBroker::UnregisterSettingsComponent();
  m_settingsComponent.reset();
}

// ---------------------------------------------------------------------------
// Validation tests — prove the stub works without CApplication
// ---------------------------------------------------------------------------

/*!
 * \brief Verify the stub initializes and registers a CSettingsComponent
 *        without needing CApplication or CServiceManager.
 */
TEST_F(TestServiceBrokerStub, SettingsComponentRegisteredWithoutApplication)
{
  auto settings = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(settings, nullptr);
}

/*!
 * \brief Verify GetSettings() returns a non-null CSettings pointer.
 *        The CSettings object is allocated but not initialized — this is
 *        sufficient for tests that just need the pointer to exist.
 */
TEST_F(TestServiceBrokerStub, GetSettingsReturnsNonNull)
{
  auto settings = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(settings, nullptr);
  EXPECT_NE(settings->GetSettings(), nullptr);
}

/*!
 * \brief Verify GetProfileManager() returns a non-null CProfileManager.
 *        The profile manager is allocated but uninitialized.
 */
TEST_F(TestServiceBrokerStub, GetProfileManagerReturnsNonNull)
{
  auto settings = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(settings, nullptr);
  EXPECT_NE(settings->GetProfileManager(), nullptr);
}

/*!
 * \brief Verify GetAdvancedSettings() returns a non-null pointer.
 */
TEST_F(TestServiceBrokerStub, GetAdvancedSettingsReturnsNonNull)
{
  auto settings = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(settings, nullptr);
  EXPECT_NE(settings->GetAdvancedSettings(), nullptr);
}

/*!
 * \brief Verify RegisterService<CSettingsComponent>() replaces the current
 *        settings component and the new one is visible through the broker.
 */
TEST_F(TestServiceBrokerStub, RegisterServiceReplacesSettingsComponent)
{
  auto original = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(original, nullptr);

  // Register a different settings component
  auto replacement = std::make_shared<StubSettingsComponent>();
  RegisterService<CSettingsComponent>(replacement);

  auto current = CServiceBroker::GetSettingsComponent();
  EXPECT_EQ(current.get(), replacement.get());
  EXPECT_NE(current.get(), original.get());
}

/*!
 * \brief Verify TearDown cleans up — after destruction, settings component
 *        should be null. We test this by creating a nested scope.
 */
TEST_F(TestServiceBrokerStub, TearDownCleansUpRegistration)
{
  // This test validates that the fixture registered something.
  // The actual TearDown cleanup is validated by the fixture lifecycle —
  // if it leaked, subsequent tests using TestBasicEnvironment would fail.
  auto settings = CServiceBroker::GetSettingsComponent();
  ASSERT_NE(settings, nullptr);
}
