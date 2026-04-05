/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "profiles/ProfileManager.h"

/*!
 * \brief A lightweight CProfileManager stub for unit tests.
 *
 * CProfileManager's public API is non-virtual, so this class cannot
 * override its behavior through polymorphism. Instead, it inherits to
 * override the virtual ISettingsHandler and ISettingCallback methods
 * that CProfileManager implements, making them safe no-ops.
 *
 * The stub is constructed without calling Initialize() — this means:
 *   - No filesystem operations (no profile folder creation)
 *   - No settings registration
 *   - GetNumberOfProfiles() returns 0
 *   - Path accessors return empty strings
 *
 * For tests that need profile data, add profiles programmatically:
 * \code
 *   StubProfileManager mgr;
 *   CProfile profile("special://temp");
 *   mgr.AddProfile(profile);
 *   EXPECT_EQ(mgr.GetNumberOfProfiles(), 1);
 * \endcode
 *
 * This class is intentionally minimal. It exists to prevent accidental
 * I/O during unit tests while keeping CProfileManager's data structures
 * accessible for programmatic setup.
 */
class StubProfileManager : public CProfileManager
{
public:
  StubProfileManager() = default;
  ~StubProfileManager() override = default;

  // ISettingsHandler overrides — safe no-ops
  void OnSettingsLoaded() override {}
  void OnSettingsSaved() const override {}
  void OnSettingsCleared() override {}
};
