/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/SettingsComponent.h"

#include <memory>

class CProfileManager;
class CSettings;

/*!
 * \brief A lightweight CSettingsComponent for unit tests.
 *
 * The real CSettingsComponent constructor creates CSettings,
 * CAdvancedSettings, CSubtitlesSettings, and CProfileManager objects.
 * Those objects are constructed but NOT initialized (Initialize() and
 * Load() are never called), making them safe to query for type identity
 * while avoiding filesystem and registry side effects.
 *
 * This stub exists primarily to document the intent: tests that use this
 * are explicitly choosing the "uninitialized but allocated" state over
 * full bootstrap. The CSettingsComponent base class already constructs
 * its sub-objects in its constructor, so this class adds no new members.
 *
 * If tests need specific CSettings values, they should call
 * GetSettings() on this component and configure the returned CSettings
 * object directly (it supports programmatic value registration).
 *
 * Usage:
 * \code
 *   auto stub = std::make_shared<StubSettingsComponent>();
 *   CServiceBroker::RegisterSettingsComponent(stub);
 *   // stub->GetSettings() returns a real but uninitialized CSettings
 *   // stub->GetProfileManager() returns a real but uninitialized CProfileManager
 * \endcode
 */
class StubSettingsComponent : public CSettingsComponent
{
public:
  StubSettingsComponent() = default;
  ~StubSettingsComponent() override = default;
};
