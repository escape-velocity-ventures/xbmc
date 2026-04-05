/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "test/TestAssertions.h"
#include "test/TestDataBuilder.h"
#include "test/TestFileItemFactory.h"

#include <gtest/gtest.h>

// --- TestFileItemFactory tests ---

TEST(TestFileItemFactory, MakeVideoFile_SetsCorrectProperties)
{
  CFileItem item = TestFileItemFactory::MakeVideoFile("/movies/test.mkv", "Test Movie");

  ASSERT_FILEITEM_PATH_EQ("/movies/test.mkv", item);
  EXPECT_EQ("Test Movie", item.GetLabel());
  EXPECT_EQ("video/x-matroska", item.GetMimeType());
  EXPECT_FALSE(item.IsFolder());
  ASSERT_TRUE(item.HasVideoInfoTag());
  EXPECT_EQ("Test Movie", item.GetVideoInfoTag()->GetTitle());
}

TEST(TestFileItemFactory, MakeAudioFile_SetsArtistAndAlbum)
{
  CFileItem item = TestFileItemFactory::MakeAudioFile(
      "/music/song.mp3", "Come Together", "The Beatles", "Abbey Road");

  ASSERT_FILEITEM_PATH_EQ("/music/song.mp3", item);
  EXPECT_EQ("Come Together", item.GetLabel());
  EXPECT_EQ("audio/mpeg", item.GetMimeType());
  EXPECT_FALSE(item.IsFolder());
  ASSERT_TRUE(item.HasMusicInfoTag());

  const auto* tag = item.GetMusicInfoTag();
  EXPECT_EQ("Come Together", tag->GetTitle());
  EXPECT_EQ("The Beatles", tag->GetArtistString());
  EXPECT_EQ("Abbey Road", tag->GetAlbum());
  EXPECT_TRUE(tag->Loaded());
}

TEST(TestFileItemFactory, MakeFolder_SetsIsFolder)
{
  CFileItem item = TestFileItemFactory::MakeFolder("/movies/collection/", "My Collection");

  ASSERT_FILEITEM_PATH_EQ("/movies/collection/", item);
  EXPECT_EQ("My Collection", item.GetLabel());
  EXPECT_TRUE(item.IsFolder());
}

TEST(TestFileItemFactory, MakePlaylist_MusicType)
{
  CFileItem item = TestFileItemFactory::MakePlaylist("/playlists/rock.m3u", "music");

  ASSERT_FILEITEM_PATH_EQ("/playlists/rock.m3u", item);
  EXPECT_EQ("audio/x-mpegurl", item.GetMimeType());
  EXPECT_FALSE(item.IsFolder());
}

TEST(TestFileItemFactory, MakePlaylist_VideoType)
{
  CFileItem item = TestFileItemFactory::MakePlaylist("/playlists/movies.m3u", "video");

  ASSERT_FILEITEM_PATH_EQ("/playlists/movies.m3u", item);
  EXPECT_EQ("video/x-mpegurl", item.GetMimeType());
  EXPECT_FALSE(item.IsFolder());
}

// --- TestAssertions tests ---

TEST(TestAssertions, FileItemPathEq_Passes)
{
  CFileItem item("/test/path.mkv", false);
  ASSERT_FILEITEM_PATH_EQ("/test/path.mkv", item);
}

TEST(TestAssertions, UrlEq_Passes)
{
  CURL url("smb://server/share/file.txt");
  ASSERT_URL_EQ("smb://server/share/file.txt", url);
}

// --- TestDataBuilder tests ---

TEST(TestDataBuilder, MusicInfoTag_FluentBuild)
{
  auto tag = MusicInfoTagBuilder()
      .Title("Come Together")
      .Artist("The Beatles")
      .Album("Abbey Road")
      .Year(1969)
      .TrackNumber(1)
      .Duration(259)
      .Genre("Rock")
      .Rating(9.0f)
      .Loaded()
      .Build();

  EXPECT_EQ("Come Together", tag.GetTitle());
  EXPECT_EQ("The Beatles", tag.GetArtistString());
  EXPECT_EQ("Abbey Road", tag.GetAlbum());
  EXPECT_EQ(1969, tag.GetYear());
  EXPECT_EQ(1, tag.GetTrackNumber());
  EXPECT_EQ(259, tag.GetDuration());
  EXPECT_FLOAT_EQ(9.0f, tag.GetRating());
  EXPECT_TRUE(tag.Loaded());
}

TEST(TestDataBuilder, MusicInfoTag_PartialBuild)
{
  auto tag = MusicInfoTagBuilder()
      .Title("Untitled")
      .Build();

  EXPECT_EQ("Untitled", tag.GetTitle());
  EXPECT_EQ("", tag.GetAlbum());
  EXPECT_EQ(0, tag.GetYear());
}

TEST(TestDataBuilder, VideoInfoTag_FluentBuild)
{
  auto tag = VideoInfoTagBuilder()
      .Title("The Matrix")
      .Director("Lana Wachowski")
      .Year(1999)
      .Rating(8.7f)
      .Duration(8160)
      .Genre("Sci-Fi")
      .Plot("A computer hacker learns about the true nature of reality.")
      .Build();

  EXPECT_EQ("The Matrix", tag.GetTitle());
  EXPECT_EQ(1999, tag.GetYear());
  EXPECT_FLOAT_EQ(8.7f, tag.GetRating("default").rating);
  EXPECT_EQ(8160u, tag.GetDuration());
}

TEST(TestDataBuilder, VideoInfoTag_PartialBuild)
{
  auto tag = VideoInfoTagBuilder()
      .Title("Untitled Film")
      .Build();

  EXPECT_EQ("Untitled Film", tag.GetTitle());
  EXPECT_EQ(0, tag.GetYear());
}

TEST(TestDataBuilder, VideoInfoTag_MultipleDirectors)
{
  auto tag = VideoInfoTagBuilder()
      .Title("The Matrix")
      .Directors({"Lana Wachowski", "Lilly Wachowski"})
      .Build();

  EXPECT_EQ("The Matrix", tag.GetTitle());
  // GetCast/directors accessed via the tag's internal storage
}

TEST(TestDataBuilder, VideoInfoTag_MultipleGenres)
{
  auto tag = VideoInfoTagBuilder()
      .Title("Blade Runner")
      .Genres({"Sci-Fi", "Thriller", "Drama"})
      .Build();

  EXPECT_EQ("Blade Runner", tag.GetTitle());
}
