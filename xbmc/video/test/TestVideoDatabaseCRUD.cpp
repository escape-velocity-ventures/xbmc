/*
 *  Copyright (C) 2025 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "dbwrappers/test/TestVideoDatabaseFixture.h"
#include "utils/Artwork.h"
#include "utils/StreamDetails.h"
#include "video/Bookmark.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

// ---------------------------------------------------------------------------
// Helper: build a CVideoInfoTag with the minimum fields needed by the
// VideoDatabase CRUD methods.  Every tag gets a valid m_dateAdded so that
// CVideoDatabase::GetDateAdded() never reaches CServiceBroker (which is
// unavailable in unit tests).
// ---------------------------------------------------------------------------
CVideoInfoTag MakeMovieTag(const std::string& title,
                           const std::string& path,
                           const std::string& file)
{
  CVideoInfoTag tag;
  tag.SetTitle(title);
  tag.SetPath(path);
  tag.SetFile(file);
  tag.SetFileNameAndPath(path + file);
  tag.m_type = MediaTypeMovie;
  tag.m_dateAdded.SetDateTime(2025, 1, 15, 12, 0, 0);
  return tag;
}

CVideoInfoTag MakeTvShowTag(const std::string& title, const std::string& path)
{
  CVideoInfoTag tag;
  tag.SetTitle(title);
  tag.SetPath(path);
  tag.m_type = MediaTypeTvShow;
  tag.m_dateAdded.SetDateTime(2025, 2, 1, 10, 0, 0);
  return tag;
}

CVideoInfoTag MakeEpisodeTag(const std::string& title,
                              const std::string& path,
                              const std::string& file,
                              int season,
                              int episode)
{
  CVideoInfoTag tag;
  tag.SetTitle(title);
  tag.SetPath(path);
  tag.SetFile(file);
  tag.SetFileNameAndPath(path + file);
  tag.m_iSeason = season;
  tag.m_iEpisode = episode;
  tag.m_type = MediaTypeEpisode;
  tag.m_dateAdded.SetDateTime(2025, 3, 10, 8, 30, 0);
  return tag;
}

CVideoInfoTag MakeMusicVideoTag(const std::string& title,
                                 const std::string& path,
                                 const std::string& file)
{
  CVideoInfoTag tag;
  tag.SetTitle(title);
  tag.SetPath(path);
  tag.SetFile(file);
  tag.SetFileNameAndPath(path + file);
  tag.m_type = MediaTypeMusicVideo;
  tag.m_dateAdded.SetDateTime(2025, 4, 20, 14, 0, 0);
  return tag;
}

} // anonymous namespace

// ===========================================================================
// Movie CRUD operations (15 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsAddsMovie)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsPopulatesDbId)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_EQ(tag.m_iDbId, id);
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsPopulatesFileId)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(tag.m_iFileId, 0);
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_HasMovieInfoAfterAdd)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_TRUE(m_videoDb->HasMovieInfo("/movies/inception.mkv"));
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_HasMovieInfoReturnsFalseForMissing)
{
  EXPECT_FALSE(m_videoDb->HasMovieInfo("/movies/nonexistent.mkv"));
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_GetMovieInfoReturnsDetails)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetPlot("A thief who enters dreams");
  tag.SetYear(2010);
  tag.SetGenre({"Sci-Fi", "Action"});
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved));
  EXPECT_EQ(retrieved.GetTitle(), "Inception");
  EXPECT_EQ(retrieved.m_strPlot, "A thief who enters dreams");
  EXPECT_EQ(retrieved.GetYear(), 2010);
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_GetMovieInfoReturnsGenre)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetGenre({"Sci-Fi", "Action"});
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved);
  EXPECT_EQ(retrieved.m_genre.size(), 2u);
  EXPECT_EQ(retrieved.m_genre[0], "Sci-Fi");
  EXPECT_EQ(retrieved.m_genre[1], "Action");
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_GetMovieInfoById)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetMovieInfo("", retrieved, id));
  EXPECT_EQ(retrieved.GetTitle(), "Inception");
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_GetMovieInfoReturnsFalseForMissing)
{
  CVideoInfoTag retrieved;
  EXPECT_FALSE(m_videoDb->GetMovieInfo("/movies/nonexistent.mkv", retrieved));
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_UpdateExistingMovie)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetPlot("Original plot");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);

  // Update the same movie with new details
  auto tag2 = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag2.SetPlot("Updated plot");
  tag2.m_iDbId = id;
  tag2.m_iFileId = tag.m_iFileId;
  int id2 = m_videoDb->SetDetailsForMovie(tag2, art, id);
  EXPECT_EQ(id, id2);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved);
  EXPECT_EQ(retrieved.m_strPlot, "Updated plot");
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsWithDirectors)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetDirector({"Christopher Nolan"});
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved);
  ASSERT_EQ(retrieved.m_director.size(), 1u);
  EXPECT_EQ(retrieved.m_director[0], "Christopher Nolan");
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsWithStudios)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetStudio({"Warner Bros."});
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved);
  ASSERT_EQ(retrieved.m_studio.size(), 1u);
  EXPECT_EQ(retrieved.m_studio[0], "Warner Bros.");
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_SetDetailsWithRating)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  tag.SetRating(8.8f, 100000, "imdb", true);
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMovie(tag, art);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/inception.mkv", retrieved);
  CRating rating = retrieved.GetRating("imdb");
  EXPECT_NEAR(rating.rating, 8.8f, 0.1f);
  EXPECT_EQ(rating.votes, 100000);
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_DeleteMovie)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);

  EXPECT_TRUE(m_videoDb->DeleteMovie(id));
  EXPECT_FALSE(m_videoDb->HasMovieInfo("/movies/inception.mkv"));
}

TEST_F(TestVideoDatabaseFixture, MovieCRUD_DeleteMovieInvalidId)
{
  EXPECT_FALSE(m_videoDb->DeleteMovie(-1));
}

// ===========================================================================
// TV Show hierarchy operations (15 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_SetDetailsAddsTvShow)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int id = m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_HasTvShowInfoAfterAdd)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);
  EXPECT_TRUE(m_videoDb->HasTvShowInfo("/tvshows/breaking_bad/"));
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetTvShowInfoReturnsDetails)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  tag.SetPlot("A chemistry teacher turns to crime");
  tag.SetGenre({"Drama", "Crime"});
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetTvShowInfo("/tvshows/breaking_bad/", retrieved));
  EXPECT_EQ(retrieved.GetTitle(), "Breaking Bad");
  EXPECT_EQ(retrieved.m_strPlot, "A chemistry teacher turns to crime");
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetTvShowInfoReturnsGenre)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  tag.SetGenre({"Drama", "Crime"});
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  CVideoInfoTag retrieved;
  m_videoDb->GetTvShowInfo("/tvshows/breaking_bad/", retrieved);
  EXPECT_EQ(retrieved.m_genre.size(), 2u);
  EXPECT_EQ(retrieved.m_genre[0], "Drama");
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_AddSeason)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  int idSeason = m_videoDb->AddSeason(idShow, 1, "Season 1");
  EXPECT_GT(idSeason, 0);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_AddSeasonIdempotent)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  int id1 = m_videoDb->AddSeason(idShow, 1);
  int id2 = m_videoDb->AddSeason(idShow, 1);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetSeasonId)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  int idSeason = m_videoDb->AddSeason(idShow, 3);
  int retrieved = m_videoDb->GetSeasonId(idShow, 3);
  EXPECT_EQ(idSeason, retrieved);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetSeasonIdReturnsMinus1ForMissing)
{
  auto tag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, tag, art, seasonArt);

  EXPECT_EQ(m_videoDb->GetSeasonId(idShow, 99), -1);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_SetDetailsForEpisode)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  int idEp = m_videoDb->SetDetailsForEpisode(epTag, art, idShow);
  EXPECT_GT(idEp, 0);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetEpisodeInfoAfterAdd)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  epTag.SetPlot("Walter White begins his transformation");
  int idEp = m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetEpisodeInfo(
    "/tvshows/breaking_bad/Season 1/s01e01.mkv", retrieved, idEp));
  EXPECT_EQ(retrieved.GetTitle(), "Pilot");
  EXPECT_EQ(retrieved.m_strPlot, "Walter White begins his transformation");
  EXPECT_EQ(retrieved.m_iSeason, 1);
  EXPECT_EQ(retrieved.m_iEpisode, 1);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_HasEpisodeInfoAfterAdd)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  EXPECT_TRUE(m_videoDb->HasEpisodeInfo(
    "/tvshows/breaking_bad/Season 1/s01e01.mkv"));
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_EpisodeCreatesSeason)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  // Season 1 should have been auto-created by SetDetailsForEpisode
  int idSeason = m_videoDb->GetSeasonId(idShow, 1);
  EXPECT_GT(idSeason, 0);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_GetTvShowForEpisode)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  int idEp = m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  EXPECT_EQ(m_videoDb->GetTvShowForEpisode(idEp), idShow);
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_DeleteTvShowCascade)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  // Add an episode
  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  // Delete show - should cascade to episodes
  m_videoDb->DeleteTvShow(idShow);

  EXPECT_FALSE(m_videoDb->HasTvShowInfo("/tvshows/breaking_bad/"));
  EXPECT_FALSE(m_videoDb->HasEpisodeInfo(
    "/tvshows/breaking_bad/Season 1/s01e01.mkv"));
}

TEST_F(TestVideoDatabaseFixture, TvShowCRUD_DeleteEpisodeKeepsShow)
{
  auto showTag = MakeTvShowTag("Breaking Bad", "/tvshows/breaking_bad/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/breaking_bad/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto epTag = MakeEpisodeTag("Pilot", "/tvshows/breaking_bad/Season 1/", "s01e01.mkv", 1, 1);
  int idEp = m_videoDb->SetDetailsForEpisode(epTag, art, idShow);

  m_videoDb->DeleteEpisode(idEp);

  // Show should survive episode deletion
  EXPECT_TRUE(m_videoDb->HasTvShowInfo("/tvshows/breaking_bad/"));
}

// ===========================================================================
// Music Video operations (5 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, MusicVideoCRUD_SetDetailsAddsMusicVideo)
{
  auto tag = MakeMusicVideoTag("Thriller", "/musicvideos/", "thriller.mkv");
  tag.SetArtist({"Michael Jackson"});
  tag.SetAlbum("Thriller");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMusicVideo(tag, art);
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, MusicVideoCRUD_HasMusicVideoInfoAfterAdd)
{
  auto tag = MakeMusicVideoTag("Thriller", "/musicvideos/", "thriller.mkv");
  KODI::ART::Artwork art;
  m_videoDb->SetDetailsForMusicVideo(tag, art);
  EXPECT_TRUE(m_videoDb->HasMusicVideoInfo("/musicvideos/thriller.mkv"));
}

TEST_F(TestVideoDatabaseFixture, MusicVideoCRUD_GetMusicVideoInfoReturnsDetails)
{
  auto tag = MakeMusicVideoTag("Thriller", "/musicvideos/", "thriller.mkv");
  tag.SetArtist({"Michael Jackson"});
  tag.SetAlbum("Thriller");
  tag.SetYear(1983);
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMusicVideo(tag, art);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetMusicVideoInfo("/musicvideos/thriller.mkv", retrieved, id));
  EXPECT_EQ(retrieved.GetTitle(), "Thriller");
  EXPECT_EQ(retrieved.m_strAlbum, "Thriller");
}

TEST_F(TestVideoDatabaseFixture, MusicVideoCRUD_GetMusicVideoInfoWithArtist)
{
  auto tag = MakeMusicVideoTag("Thriller", "/musicvideos/", "thriller.mkv");
  tag.SetArtist({"Michael Jackson"});
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMusicVideo(tag, art);

  CVideoInfoTag retrieved;
  m_videoDb->GetMusicVideoInfo("/musicvideos/thriller.mkv", retrieved, id);
  ASSERT_EQ(retrieved.m_artist.size(), 1u);
  EXPECT_EQ(retrieved.m_artist[0], "Michael Jackson");
}

TEST_F(TestVideoDatabaseFixture, MusicVideoCRUD_DeleteMusicVideo)
{
  auto tag = MakeMusicVideoTag("Thriller", "/musicvideos/", "thriller.mkv");
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMusicVideo(tag, art);

  m_videoDb->DeleteMusicVideo(id);
  EXPECT_FALSE(m_videoDb->HasMusicVideoInfo("/musicvideos/thriller.mkv"));
}

// ===========================================================================
// Cross-type: Path management (10 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, PathManagement_AddPathReturnsId)
{
  int id = m_videoDb->AddPath("/videos/movies/");
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, PathManagement_AddPathIdempotent)
{
  int id1 = m_videoDb->AddPath("/videos/movies/");
  int id2 = m_videoDb->AddPath("/videos/movies/");
  EXPECT_EQ(id1, id2);
}

TEST_F(TestVideoDatabaseFixture, PathManagement_GetPathIdAfterAdd)
{
  int addedId = m_videoDb->AddPath("/videos/movies/");
  int retrievedId = m_videoDb->GetPathId("/videos/movies/");
  EXPECT_EQ(addedId, retrievedId);
}

TEST_F(TestVideoDatabaseFixture, PathManagement_GetPathIdReturnsMinus1ForMissing)
{
  EXPECT_EQ(m_videoDb->GetPathId("/nonexistent/path/"), -1);
}

TEST_F(TestVideoDatabaseFixture, PathManagement_SetAndGetPathHash)
{
  m_videoDb->AddPath("/videos/movies/");
  EXPECT_TRUE(m_videoDb->SetPathHash("/videos/movies/", "abc123hash"));

  std::string hash;
  EXPECT_TRUE(m_videoDb->GetPathHash("/videos/movies/", hash));
  EXPECT_EQ(hash, "abc123hash");
}

TEST_F(TestVideoDatabaseFixture, FileManagement_AddFileReturnsId)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int id = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, FileManagement_AddFileIdempotent)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int id1 = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);
  int id2 = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);
  EXPECT_EQ(id1, id2);
}

TEST_F(TestVideoDatabaseFixture, FileManagement_GetFileIdAfterAdd)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int addedId = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);
  int retrievedId = m_videoDb->GetFileId("/videos/movies/test.mkv");
  EXPECT_EQ(addedId, retrievedId);
}

TEST_F(TestVideoDatabaseFixture, FileManagement_GetFileIdReturnsMinus1ForMissing)
{
  EXPECT_EQ(m_videoDb->GetFileId("/nonexistent/file.mkv"), -1);
}

TEST_F(TestVideoDatabaseFixture, FileManagement_DeleteFile)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int id = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);
  EXPECT_TRUE(m_videoDb->DeleteFile(id));
  EXPECT_EQ(m_videoDb->GetFileId("/videos/movies/test.mkv"), -1);
}

// ===========================================================================
// Bookmarks (5 tests within cross-type budget)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, Bookmark_AddAndGetResumeBookmark)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CBookmark bookmark;
  bookmark.timeInSeconds = 120.0;
  bookmark.totalTimeInSeconds = 7200.0;
  bookmark.type = CBookmark::RESUME;
  EXPECT_TRUE(m_videoDb->AddBookMarkToFile(
    "/videos/movies/test.mkv", bookmark, CBookmark::RESUME));

  CBookmark retrieved;
  EXPECT_TRUE(m_videoDb->GetResumeBookMark(
    "/videos/movies/test.mkv", retrieved));
  EXPECT_DOUBLE_EQ(retrieved.timeInSeconds, 120.0);
  EXPECT_DOUBLE_EQ(retrieved.totalTimeInSeconds, 7200.0);
}

TEST_F(TestVideoDatabaseFixture, Bookmark_AddStandardBookmark)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CBookmark bookmark;
  bookmark.timeInSeconds = 300.0;
  bookmark.totalTimeInSeconds = 7200.0;
  EXPECT_TRUE(m_videoDb->AddBookMarkToFile(
    "/videos/movies/test.mkv", bookmark, CBookmark::STANDARD));

  VECBOOKMARKS bookmarks;
  m_videoDb->GetBookMarksForFile(
    "/videos/movies/test.mkv", bookmarks, CBookmark::STANDARD);
  ASSERT_EQ(bookmarks.size(), 1u);
  EXPECT_DOUBLE_EQ(bookmarks[0].timeInSeconds, 300.0);
}

TEST_F(TestVideoDatabaseFixture, Bookmark_ClearBookMarksOfFile)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CBookmark bookmark;
  bookmark.timeInSeconds = 300.0;
  bookmark.totalTimeInSeconds = 7200.0;
  m_videoDb->AddBookMarkToFile(
    "/videos/movies/test.mkv", bookmark, CBookmark::STANDARD);

  EXPECT_TRUE(m_videoDb->ClearBookMarksOfFile(
    "/videos/movies/test.mkv", CBookmark::STANDARD));

  VECBOOKMARKS bookmarks;
  m_videoDb->GetBookMarksForFile(
    "/videos/movies/test.mkv", bookmarks, CBookmark::STANDARD);
  EXPECT_TRUE(bookmarks.empty());
}

TEST_F(TestVideoDatabaseFixture, Bookmark_MultipleBookmarks)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  for (int i = 1; i <= 3; ++i)
  {
    CBookmark bm;
    bm.timeInSeconds = i * 100.0;
    bm.totalTimeInSeconds = 7200.0;
    m_videoDb->AddBookMarkToFile(
      "/videos/movies/test.mkv", bm, CBookmark::STANDARD);
  }

  VECBOOKMARKS bookmarks;
  m_videoDb->GetBookMarksForFile(
    "/videos/movies/test.mkv", bookmarks, CBookmark::STANDARD);
  EXPECT_EQ(bookmarks.size(), 3u);
}

TEST_F(TestVideoDatabaseFixture, Bookmark_NoResumeBookmarkReturnsEmpty)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CBookmark retrieved;
  EXPECT_FALSE(m_videoDb->GetResumeBookMark(
    "/videos/movies/test.mkv", retrieved));
}

// ===========================================================================
// Stream details (2 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, StreamDetails_SetAndGetForFile)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int idFile = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CStreamDetails details;
  auto* videoStream = new CStreamDetailVideo();
  videoStream->m_iWidth = 1920;
  videoStream->m_iHeight = 1080;
  videoStream->m_strCodec = "h264";
  details.AddStream(videoStream);

  auto* audioStream = new CStreamDetailAudio();
  audioStream->m_iChannels = 6;
  audioStream->m_strCodec = "aac";
  audioStream->m_strLanguage = "eng";
  details.AddStream(audioStream);

  EXPECT_TRUE(m_videoDb->SetStreamDetailsForFileId(details, idFile));

  CStreamDetails retrieved;
  EXPECT_TRUE(m_videoDb->GetStreamDetails("/videos/movies/test.mkv", retrieved));
  EXPECT_EQ(retrieved.GetVideoWidth(), 1920);
  EXPECT_EQ(retrieved.GetVideoHeight(), 1080);
  EXPECT_EQ(retrieved.GetAudioChannels(), 6);
}

TEST_F(TestVideoDatabaseFixture, StreamDetails_DeleteStreamDetails)
{
  CDateTime dateAdded;
  dateAdded.SetDateTime(2025, 6, 1, 0, 0, 0);
  int idFile = m_videoDb->AddFile("/videos/movies/test.mkv", "", dateAdded);

  CStreamDetails details;
  auto* videoStream = new CStreamDetailVideo();
  videoStream->m_iWidth = 1920;
  videoStream->m_iHeight = 1080;
  details.AddStream(videoStream);
  m_videoDb->SetStreamDetailsForFileId(details, idFile);

  m_videoDb->DeleteStreamDetails(idFile);

  CStreamDetails retrieved;
  // After deletion, stream details should be empty
  m_videoDb->GetStreamDetails("/videos/movies/test.mkv", retrieved);
  EXPECT_EQ(retrieved.GetVideoStreamCount(), 0);
}

// ===========================================================================
// Edge cases (10 tests)
// ===========================================================================

TEST_F(TestVideoDatabaseFixture, EdgeCase_UnicodeMovieTitle)
{
  auto tag = MakeMovieTag(
    "\xe5\x8d\x83\xe3\x81\xa8\xe5\x8d\x83\xe5\xb0\x8b\xe3\x81\xae\xe7\xa5\x9e\xe9\x9a\xa0\xe3\x81\x97",
    "/movies/", "spirited_away.mkv");  // Spirited Away in Japanese
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);

  CVideoInfoTag retrieved;
  EXPECT_TRUE(m_videoDb->GetMovieInfo("/movies/spirited_away.mkv", retrieved));
  EXPECT_EQ(retrieved.GetTitle(),
    "\xe5\x8d\x83\xe3\x81\xa8\xe5\x8d\x83\xe5\xb0\x8b\xe3\x81\xae\xe7\xa5\x9e\xe9\x9a\xa0\xe3\x81\x97");
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_UnicodeMovieTitleCyrillic)
{
  auto tag = MakeMovieTag(
    "\xd0\x91\xd1\x80\xd0\xb0\xd1\x82",
    "/movies/", "brat.mkv");  // "Brat" in Cyrillic
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/brat.mkv", retrieved);
  EXPECT_EQ(retrieved.GetTitle(), "\xd0\x91\xd1\x80\xd0\xb0\xd1\x82");
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_LongPlotSummary)
{
  auto tag = MakeMovieTag("Epic Film", "/movies/", "epic.mkv");
  // Build a very long plot string (10,000 characters)
  std::string longPlot(10000, 'A');
  longPlot[0] = 'T';
  longPlot[longPlot.size() - 1] = '.';
  tag.SetPlot(longPlot);
  KODI::ART::Artwork art;
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);

  CVideoInfoTag retrieved;
  m_videoDb->GetMovieInfo("/movies/epic.mkv", retrieved);
  EXPECT_EQ(retrieved.m_strPlot.size(), 10000u);
  EXPECT_EQ(retrieved.m_strPlot[0], 'T');
  EXPECT_EQ(retrieved.m_strPlot[retrieved.m_strPlot.size() - 1], '.');
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_EmptyDatabaseGetMovieInfo)
{
  CVideoInfoTag retrieved;
  EXPECT_FALSE(m_videoDb->GetMovieInfo("/no/such/path.mkv", retrieved));
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_EmptyDatabaseGetTvShowInfo)
{
  CVideoInfoTag retrieved;
  EXPECT_FALSE(m_videoDb->GetTvShowInfo("/no/such/path/", retrieved));
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_EmptyDatabaseGetEpisodeInfo)
{
  CVideoInfoTag retrieved;
  EXPECT_FALSE(m_videoDb->GetEpisodeInfo("/no/such/episode.mkv", retrieved));
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_EmptyDatabaseGetMusicVideoInfo)
{
  CVideoInfoTag retrieved;
  EXPECT_FALSE(m_videoDb->GetMusicVideoInfo("/no/such/video.mkv", retrieved));
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_MultipleArtTypes)
{
  auto tag = MakeMovieTag("Inception", "/movies/", "inception.mkv");
  KODI::ART::Artwork art;
  art["poster"] = "http://example.com/poster.jpg";
  art["fanart"] = "http://example.com/fanart.jpg";
  art["thumb"] = "http://example.com/thumb.jpg";
  int id = m_videoDb->SetDetailsForMovie(tag, art);
  EXPECT_GT(id, 0);
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_MultipleMoviesSamePath)
{
  auto tag1 = MakeMovieTag("Movie One", "/movies/collection/", "movie1.mkv");
  auto tag2 = MakeMovieTag("Movie Two", "/movies/collection/", "movie2.mkv");
  KODI::ART::Artwork art;
  int id1 = m_videoDb->SetDetailsForMovie(tag1, art);
  int id2 = m_videoDb->SetDetailsForMovie(tag2, art);
  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_NE(id1, id2);

  CVideoInfoTag r1, r2;
  EXPECT_TRUE(m_videoDb->GetMovieInfo("/movies/collection/movie1.mkv", r1));
  EXPECT_TRUE(m_videoDb->GetMovieInfo("/movies/collection/movie2.mkv", r2));
  EXPECT_EQ(r1.GetTitle(), "Movie One");
  EXPECT_EQ(r2.GetTitle(), "Movie Two");
}

TEST_F(TestVideoDatabaseFixture, EdgeCase_MultipleEpisodesForShow)
{
  auto showTag = MakeTvShowTag("The Wire", "/tvshows/the_wire/");
  KODI::ART::Artwork art;
  KODI::ART::SeasonsArtwork seasonArt;
  std::vector<std::string> paths = {"/tvshows/the_wire/"};
  int idShow = m_videoDb->SetDetailsForTvShow(paths, showTag, art, seasonArt);

  auto ep1 = MakeEpisodeTag("Episode 1", "/tvshows/the_wire/Season 1/", "s01e01.mkv", 1, 1);
  auto ep2 = MakeEpisodeTag("Episode 2", "/tvshows/the_wire/Season 1/", "s01e02.mkv", 1, 2);
  auto ep3 = MakeEpisodeTag("Episode 1 S2", "/tvshows/the_wire/Season 2/", "s02e01.mkv", 2, 1);

  int id1 = m_videoDb->SetDetailsForEpisode(ep1, art, idShow);
  int id2 = m_videoDb->SetDetailsForEpisode(ep2, art, idShow);
  int id3 = m_videoDb->SetDetailsForEpisode(ep3, art, idShow);

  EXPECT_GT(id1, 0);
  EXPECT_GT(id2, 0);
  EXPECT_GT(id3, 0);
  EXPECT_NE(id1, id2);
  EXPECT_NE(id2, id3);

  // Season 1 and Season 2 should both exist
  EXPECT_GT(m_videoDb->GetSeasonId(idShow, 1), 0);
  EXPECT_GT(m_videoDb->GetSeasonId(idShow, 2), 0);
}
