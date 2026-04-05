/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ServiceBroker.h"

#include <functional>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

class CSettingsComponent;

/*!
 * \brief Lightweight GTest fixture that registers minimal services with
 *        CServiceBroker without requiring CApplication or CServiceManager.
 *
 * Use this fixture for fast unit tests that need Settings or Profile
 * access through CServiceBroker but do not need the full application
 * bootstrap provided by TestBasicEnvironment.
 *
 * Tests choose ONE of:
 *   - TestBasicEnvironment  (full bootstrap, slow, integration-level)
 *   - TestServiceBrokerStub (minimal services, fast, unit-level)
 *
 * Example usage:
 * \code
 *   class MyTest : public TestServiceBrokerStub
 *   {
 *   };
 *
 *   TEST_F(MyTest, SettingsComponentIsAvailable)
 *   {
 *     auto settings = CServiceBroker::GetSettingsComponent();
 *     ASSERT_NE(settings, nullptr);
 *   }
 * \endcode
 *
 * To register additional services in a derived fixture, override SetUp()
 * and call TestServiceBrokerStub::SetUp() first, then register your
 * services. Mirror the cleanup in TearDown().
 */
class TestServiceBrokerStub : public ::testing::Test
{
protected:
  void SetUp() override;
  void TearDown() override;

  /*!
   * \brief Register a service with CServiceBroker using the appropriate
   *        Register*() method.
   *
   * Supported types (each maps to its CServiceBroker registration API):
   *   - CSettingsComponent  -> RegisterSettingsComponent / UnregisterSettingsComponent
   *
   * \tparam T The service type to register.
   * \param service Shared pointer to the service instance.
   *
   * Example:
   * \code
   *   auto mySettings = std::make_shared<CSettingsComponent>();
   *   RegisterService<CSettingsComponent>(mySettings);
   * \endcode
   */
  template<typename T>
  void RegisterService(std::shared_ptr<T> service);

  /*!
   * \brief Access the CSettingsComponent that was registered during SetUp.
   * \return Shared pointer to the settings component (never null after SetUp).
   */
  std::shared_ptr<CSettingsComponent> GetSettingsComponent() const
  {
    return m_settingsComponent;
  }

private:
  std::shared_ptr<CSettingsComponent> m_settingsComponent;
};

// --- Template specializations ------------------------------------------------

template<>
inline void TestServiceBrokerStub::RegisterService<CSettingsComponent>(
    std::shared_ptr<CSettingsComponent> service)
{
  CServiceBroker::UnregisterSettingsComponent();
  m_settingsComponent = service;
  CServiceBroker::RegisterSettingsComponent(service);
}
