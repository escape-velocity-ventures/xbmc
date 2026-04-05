/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItem.h"
#include "URL.h"

#include <gtest/gtest.h>

#include <string>

/*!
 * \brief Assert that a CFileItem's path equals the expected string.
 *
 * Produces a clear failure message showing both expected and actual paths.
 *
 * Usage:
 *   ASSERT_FILEITEM_PATH_EQ("/movies/test.mkv", item);
 */
#define ASSERT_FILEITEM_PATH_EQ(expected_path, fileitem) \
  ASSERT_EQ(std::string(expected_path), (fileitem).GetPath()) \
      << "CFileItem path mismatch: expected \"" << (expected_path) \
      << "\" but got \"" << (fileitem).GetPath() << "\""

/*!
 * \brief Assert that a CURL object's serialized URL equals the expected string.
 *
 * Compares via CURL::Get() which returns the full URL string.
 *
 * Usage:
 *   CURL url("smb://server/share/file.txt");
 *   ASSERT_URL_EQ("smb://server/share/file.txt", url);
 */
#define ASSERT_URL_EQ(expected_url_string, curl_object) \
  ASSERT_EQ(std::string(expected_url_string), (curl_object).Get()) \
      << "CURL URL mismatch: expected \"" << (expected_url_string) \
      << "\" but got \"" << (curl_object).Get() << "\""
