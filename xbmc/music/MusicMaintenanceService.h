/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CGUIDialogProgress;
class CMusicDatabase;

/*!
 * \brief Service responsible for music database cleanup and maintenance operations
 *
 * Encapsulates all cleanup, orphan removal, CDDB management, and related
 * maintenance logic that was previously inline in CMusicDatabase.
 */
class CMusicMaintenanceService
{
public:
  /*!
   * \brief Construct a maintenance service for the given database
   * \param db the music database to operate on (must remain valid for the lifetime of this object)
   */
  explicit CMusicMaintenanceService(CMusicDatabase& db);
  ~CMusicMaintenanceService() = default;

  CMusicMaintenanceService(const CMusicMaintenanceService&) = delete;
  CMusicMaintenanceService& operator=(const CMusicMaintenanceService&) = delete;

  /*! \brief Run the full database cleanup procedure
   * \param progressDialog optional progress dialog for user feedback
   * \return an error code (ERROR_OK on success)
   */
  int Cleanup(CGUIDialogProgress* progressDialog = nullptr);

  /*! \brief Interactive prompt-and-clean wrapper (checks scan state, confirms with user) */
  void Clean() const;

  /*! \brief Clean up songs whose files no longer exist
   * \param progressDialog optional progress dialog for user feedback
   * \return true on success
   */
  bool CleanupSongs(CGUIDialogProgress* progressDialog = nullptr);

  /*! \brief Clean up songs by a list of song IDs
   * \param strSongIds SQL-formatted list of song IDs e.g. "(1,2,3)"
   * \return true on success
   */
  bool CleanupSongsByIds(const std::string& strSongIds);

  /*! \brief Remove albums with no referenced songs */
  bool CleanupAlbums();

  /*! \brief Remove paths with no referenced songs */
  bool CleanupPaths();

  /*! \brief Remove artists with no referenced songs or albums */
  bool CleanupArtists();

  /*! \brief Remove genres with no referenced songs */
  bool CleanupGenres();

  /*! \brief Remove orphaned info settings */
  bool CleanupInfoSettings();

  /*! \brief Remove orphaned roles */
  bool CleanupRoles();

  /*! \brief Remove all entries from the removed_link table */
  bool DeleteRemovedLinks();

  /*! \brief Clean up orphaned items (albums, artists, genres, roles, info settings)
   *
   * Does NOT clean paths or removed_links - those are handled elsewhere.
   */
  bool CleanupOrphanedItems();

  /*! \brief Trim image URL XML to fit within a maximum size
   * \param[in,out] strImage the image URL XML string to trim
   * \param space maximum allowed size in bytes
   * \return true if the string fits (possibly after trimming)
   */
  bool TrimImageURLs(std::string& strImage, const size_t space) const;

  /*! \brief Look up CDDB information for the currently inserted disc
   * \param bRequery if true, delete cached info and re-query
   * \return true if CDDB info is available
   */
  bool LookupCDDBInfo(bool bRequery = false) const;

  /*! \brief Interactively delete cached CDDB information for a selected album */
  void DeleteCDDBInfo() const;

private:
  CMusicDatabase& m_db;
};
