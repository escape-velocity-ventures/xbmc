/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItem.h"
#include "music/tags/MusicInfoTag.h"
#include "video/VideoInfoTag.h"

#include <string>

/*!
 * \brief Factory for creating pre-configured CFileItem objects in tests.
 *
 * Reduces boilerplate when tests need CFileItem instances with specific
 * properties set. Each factory method returns a fully-constructed item
 * with appropriate metadata (path, label, info tags, mime type, folder flag).
 */
class TestFileItemFactory
{
public:
  /*!
   * \brief Create a video file item with path and title set.
   * \param path File path (e.g. "/movies/test.mkv")
   * \param title Display title for the video
   * \return CFileItem with video info tag title set, m_bIsFolder=false,
   *         mime type "video/x-matroska"
   */
  static CFileItem MakeVideoFile(const std::string& path, const std::string& title)
  {
    CFileItem item(path, false);
    item.SetLabel(title);
    item.SetMimeType("video/x-matroska");
    item.GetVideoInfoTag()->SetTitle(title);
    return item;
  }

  /*!
   * \brief Create an audio file item with music info tag populated.
   * \param path File path (e.g. "/music/song.mp3")
   * \param title Song title
   * \param artist Artist name
   * \param album Album name
   * \return CFileItem with music info tag populated, m_bIsFolder=false,
   *         mime type "audio/mpeg"
   */
  static CFileItem MakeAudioFile(const std::string& path,
                                 const std::string& title,
                                 const std::string& artist,
                                 const std::string& album)
  {
    CFileItem item(path, false);
    item.SetLabel(title);
    item.SetMimeType("audio/mpeg");
    auto* tag = item.GetMusicInfoTag();
    tag->SetTitle(title);
    tag->SetArtist(artist);
    tag->SetAlbum(album);
    tag->SetLoaded(true);
    return item;
  }

  /*!
   * \brief Create a folder item.
   * \param path Folder path (e.g. "/movies/collection/")
   * \param label Display label for the folder
   * \return CFileItem with m_bIsFolder=true
   */
  static CFileItem MakeFolder(const std::string& path, const std::string& label)
  {
    CFileItem item(path, true);
    item.SetLabel(label);
    return item;
  }

  /*!
   * \brief Create a playlist file item.
   * \param path Playlist file path (e.g. "/playlists/rock.m3u")
   * \param type Playlist type - "music" or "video"
   * \return CFileItem with appropriate mime type, m_bIsFolder=false
   */
  static CFileItem MakePlaylist(const std::string& path, const std::string& type)
  {
    CFileItem item(path, false);
    item.SetLabel(path);
    if (type == "music")
      item.SetMimeType("audio/x-mpegurl");
    else if (type == "video")
      item.SetMimeType("video/x-mpegurl");
    return item;
  }
};
