/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "music/tags/MusicInfoTag.h"
#include "video/VideoInfoTag.h"

#include <string>
#include <vector>

/*!
 * \brief Fluent builder for CMusicInfoTag test objects.
 *
 * Usage:
 *   auto tag = MusicInfoTagBuilder()
 *       .Title("Come Together")
 *       .Artist("The Beatles")
 *       .Album("Abbey Road")
 *       .Year(1969)
 *       .TrackNumber(1)
 *       .Duration(259)
 *       .Build();
 */
class MusicInfoTagBuilder
{
public:
  MusicInfoTagBuilder& Title(const std::string& title)
  {
    m_tag.SetTitle(title);
    return *this;
  }

  MusicInfoTagBuilder& Artist(const std::string& artist)
  {
    m_tag.SetArtist(artist);
    return *this;
  }

  MusicInfoTagBuilder& Album(const std::string& album)
  {
    m_tag.SetAlbum(album);
    return *this;
  }

  MusicInfoTagBuilder& Year(int year)
  {
    m_tag.SetYear(year);
    return *this;
  }

  MusicInfoTagBuilder& TrackNumber(int track)
  {
    m_tag.SetTrackNumber(track);
    return *this;
  }

  MusicInfoTagBuilder& Duration(int seconds)
  {
    m_tag.SetDuration(seconds);
    return *this;
  }

  MusicInfoTagBuilder& Genre(const std::string& genre)
  {
    m_tag.SetGenre(genre);
    return *this;
  }

  MusicInfoTagBuilder& Rating(float rating)
  {
    m_tag.SetRating(rating);
    return *this;
  }

  MusicInfoTagBuilder& Loaded(bool loaded = true)
  {
    m_tag.SetLoaded(loaded);
    return *this;
  }

  MUSIC_INFO::CMusicInfoTag Build()
  {
    return m_tag;
  }

private:
  MUSIC_INFO::CMusicInfoTag m_tag;
};

/*!
 * \brief Fluent builder for CVideoInfoTag test objects.
 *
 * Usage:
 *   auto tag = VideoInfoTagBuilder()
 *       .Title("The Matrix")
 *       .Director("Lana Wachowski")
 *       .Year(1999)
 *       .Rating(8.7f)
 *       .Duration(8160)
 *       .Genre("Sci-Fi")
 *       .Plot("A computer hacker learns about the true nature of reality.")
 *       .Build();
 */
class VideoInfoTagBuilder
{
public:
  VideoInfoTagBuilder& Title(const std::string& title)
  {
    m_tag.SetTitle(title);
    return *this;
  }

  VideoInfoTagBuilder& Director(const std::string& director)
  {
    m_tag.SetDirector({director});
    return *this;
  }

  VideoInfoTagBuilder& Directors(const std::vector<std::string>& directors)
  {
    m_tag.SetDirector(directors);
    return *this;
  }

  VideoInfoTagBuilder& Year(int year)
  {
    m_tag.SetYear(year);
    return *this;
  }

  VideoInfoTagBuilder& Rating(float rating, const std::string& type, bool isDefault)
  {
    m_tag.SetRating(rating, type, isDefault);
    return *this;
  }

  VideoInfoTagBuilder& Rating(float rating)
  {
    m_tag.SetRating(rating, "default", true);
    return *this;
  }

  VideoInfoTagBuilder& Duration(int seconds)
  {
    m_tag.SetDuration(seconds);
    return *this;
  }

  VideoInfoTagBuilder& Genre(const std::string& genre)
  {
    m_tag.SetGenre({genre});
    return *this;
  }

  VideoInfoTagBuilder& Genres(const std::vector<std::string>& genres)
  {
    m_tag.SetGenre(genres);
    return *this;
  }

  VideoInfoTagBuilder& Plot(const std::string& plot)
  {
    m_tag.SetPlot(plot);
    return *this;
  }

  VideoInfoTagBuilder& Path(const std::string& path)
  {
    m_tag.SetPath(path);
    return *this;
  }

  CVideoInfoTag Build()
  {
    return m_tag;
  }

private:
  CVideoInfoTag m_tag;
};
