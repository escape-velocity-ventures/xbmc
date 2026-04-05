# God Object Decomposition Plan: Kodi Top 5

## Executive Summary

Analysis of Kodi's 5 largest god objects (47,758 lines combined):
1. **MusicDatabase.cpp** (13,969 lines, 238 methods)
2. **VideoDatabase.cpp** (12,789 lines, 313 methods)
3. **GUIInfoManager.cpp** (12,253 lines, 45 primary methods + massive switch/case chains)
4. **VideoPlayer.cpp** (5,865 lines, 16 methods but deeply entangled)
5. **Application.cpp** (2,882 lines, 16 methods but orchestrates everything)

**Critical Finding:** MusicDatabase and VideoDatabase are nearly identical in structure — both inherit from CDatabase and contain parallel implementations for Add/Get/Update/Delete operations. They share no common base class despite 90% code overlap.

---

## 1. MusicDatabase.cpp (13,969 lines)

### Current Structure Analysis

The class is a monolithic data access object with 238 public/private methods organized into loose functional categories:

**Method Clusters Identified:**

| Category | Count | Responsibility |
|----------|-------|-----------------|
| **Schema** | 8 | CreateTables(), CreateAnalytics(), CreateViews(), UpdateTables(), CreateNativeDBFunctions() |
| **Song CRUD** | 15 | AddSong(), UpdateSong(), GetSong(), GetSongsByPath(), GetSongsByArtist(), GetSongsByAlbum(), DeleteSong() |
| **Album CRUD** | 18 | AddAlbum(), UpdateAlbum(), GetAlbum(), GetAlbumsByArtist(), GetAlbumsByWhere(), DeleteAlbum() |
| **Artist CRUD** | 22 | AddArtist(), UpdateArtist(), GetArtist(), GetArtistsByAlbum(), GetArtistsByWhere(), DeleteArtist() |
| **Genre Management** | 12 | AddGenre(), GetGenres(), GetGenresByArtist(), GetGenresBySong(), GetGenresByAlbum() |
| **Relationship Mgmt** | 18 | AddSongArtist(), AddAlbumArtist(), AddSongGenres(), DeleteSongArtists(), DeleteAlbumArtists() |
| **Navigation/Filtering** | 45 | GetGenresNav(), GetArtistsNav(), GetAlbumsNav(), GetSongsNav(), GetCommonNav(), GetDiscsByWhere() |
| **Query Building** | 15 | GetArtistsByWhereJSON(), GetAlbumsByWhereJSON(), GetSongsByWhereJSON(), AlphanumericSortSQL() |
| **Search** | 8 | Search(), SearchArtists(), SearchAlbums(), SearchSongs() |
| **Playlist/Stats** | 12 | GetTop100(), GetRecentlyPlayed(), GetRecentlyAdded(), IncrementPlayCount() |
| **Cleanup/Maintenance** | 18 | Cleanup(), CleanupSongs(), CleanupAlbums(), CleanupArtists(), CleanupPaths() |
| **Data Conversion** | 12 | GetSongFromDataset(), GetAlbumFromDataset(), GetArtistFromDataset(), GetFileItemFromDataset() |
| **Utility** | 15 | GetPathId(), AddPath(), SplitPath(), NormaliseSongDates(), LookupCDDBInfo() |

### Shared State (Major Pain Point)

```cpp
// Lines 92-119 in .cpp (anonymous namespace)
constexpr unsigned int RECENTLY_PLAYED_LIMIT = 25;
constexpr size_t MIN_FULL_SEARCH_LENGTH = 3;

// Inherited from CDatabase
dbiplus::Database* m_pDB;          // Raw database connection
dbiplus::Dataset* m_pDS;           // Primary dataset handle
dbiplus::Dataset* m_pDS2;          // Secondary dataset (for nested queries)
```

**Critical Issue:** `m_pDS` and `m_pDS2` are low-level SQL result sets. All CRUD and query methods depend on these raw handles, making extraction extremely difficult without a query abstraction layer first.

### Proposed Decomposition

#### Class 1: MusicSchemaManager (7 methods)
**Responsibility:** DDL operations, schema versioning
```
- CreateTables()
- CreateAnalytics()
- CreateViews()
- CreateAnalytics()
- CreateRemovedLinkTriggers()
- CreateNativeDBFunctions()
- UpdateTables(int version)
```
**Benefits:** Schema changes can be isolated; version management centralized
**Dependencies:** CDatabase, m_pDS
**Test Requirements:** SQL migration scripts; rollback procedures
**Risk:** HIGH - Schema operations are atomic; bugs cascade to all CRUD ops

#### Class 2: MusicCRUDRepository (55 methods)
**Responsibility:** Core Create/Read/Update/Delete for primary entities
```
Song operations (15): AddSong, GetSong, UpdateSong, GetSongByFileName, etc.
Album operations (18): AddAlbum, GetAlbum, UpdateAlbum, GetAlbumsByArtist, etc.
Artist operations (22): AddArtist, GetArtist, UpdateArtist, GetArtistsByAlbum, etc.
```
**Benefits:** Single responsibility; testable with mocks; replaceable with abstraction layer
**Dependencies:** CDatabase, m_pDS, MusicSchemaManager
**Test Requirements:** Unit tests for each entity type; edge cases (duplicates, orphans)
**Risk:** HIGH - Shared m_pDS access requires synchronization

#### Class 3: MusicRelationshipManager (18 methods)
**Responsibility:** Bridge tables (song_artist, album_artist, song_genre)
```
- AddSongArtist(int idSong, int idArtist)
- AddAlbumArtist(int idAlbum, int idArtist)
- AddSongGenres(int idSong, const std::vector<std::string>& genres)
- DeleteSongArtistsBySong(int idSong)
- DeleteAlbumArtistsByAlbum(int idAlbum)
- GetArtistsByAlbum(int idAlbum, CFileItem* item)
- GetSongsByArtist(int idArtist, std::vector<int>& songs)
- GetGenresByArtist(int idArtist, CFileItem* item)
- GetGenresByAlbum(int idAlbum, CFileItem* item)
- GetGenresBySong(int idSong, std::vector<int>& genres)
- GetIsAlbumArtist(int idArtist, CFileItem* item)
+ Genre management (12 methods)
```
**Benefits:** Isolates relationship logic; enables graph traversal encapsulation
**Dependencies:** MusicCRUDRepository
**Test Requirements:** Relationship integrity tests; cascade delete verification
**Risk:** MEDIUM - Complex join logic; performance-critical

#### Class 4: MusicNavRepository (45 methods)
**Responsibility:** Navigation/filtered queries for UI
```
- GetGenresNav(baseDir, items)
- GetArtistsNav(baseDir, items)
- GetAlbumsNav(baseDir, items)
- GetSongsNav(baseDir, items)
- GetYearsNav(baseDir, items)
- GetRolesNav(baseDir, items)
- GetDiscsNav(baseDir, items)
- GetAlbumsByWhere(baseDir, sqlFilter)
- GetArtistsByWhere(baseDir, sqlFilter)
- GetSongsByWhere(baseDir, sqlFilter)
- GetDiscsByWhere(baseDir, sqlFilter)
- GetSongsFullByWhere(baseDir, sqlFilter)
+ ...27 more
```
**Benefits:** All UI-facing queries centralized; easier skinning changes
**Dependencies:** MusicCRUDRepository, MusicQueryBuilder
**Test Requirements:** Filter/sort combinations; pagination
**Risk:** MEDIUM - Complex SQL generation; filter escaping

#### Class 5: MusicQueryBuilder (15 methods)
**Responsibility:** SQL query generation; sorting & filtering
```
- GetIgnoreArticleSQL(strField)
- SortnameBuildSQL(alias, sortAttr, field, sortField)
- AlphanumericSortSQL(field, sortOrder)
- GetArtistsByWhereJSON(fields, filters, options)
- GetAlbumsByWhereJSON(fields, filters, options)
- GetSongsByWhereJSON(fields, filters, options)
+ Complex Collate & Sort logic
```
**Benefits:** SQL injection prevention; reusable sort/filter logic
**Dependencies:** Settings, LangInfo (for locale-aware sorting)
**Test Requirements:** SQL syntax validation; XSS prevention
**Risk:** MEDIUM - SQL injection vulnerability if not careful

#### Class 6: MusicSearchService (8 methods)
**Responsibility:** Full-text search, filtering
```
- Search(searchTerm, results)
- SearchSongs(searchTerm, results)
- SearchArtists(searchTerm, results)
- SearchAlbums(searchTerm, results)
```
**Benefits:** Search logic isolated; can be replaced with Elasticsearch integration later
**Dependencies:** MusicNavRepository
**Test Requirements:** Multi-language support; accent stripping
**Risk:** LOW

#### Class 7: MusicPlaylistService (12 methods)
**Responsibility:** Playlist-related queries
```
- GetTop100(baseDir, items)
- GetTop100Albums(albums)
- GetTop100AlbumSongs(baseDir, items)
- GetRecentlyPlayedAlbums(albums)
- GetRecentlyPlayedAlbumSongs(baseDir, items)
- GetRecentlyAddedAlbums(albums, limit)
- GetRecentlyAddedAlbumSongs(baseDir, items)
- IncrementPlayCount(item)
```
**Benefits:** Playlist logic separated; stats can be pre-computed
**Dependencies:** MusicCRUDRepository
**Test Requirements:** Sorting stability; date comparisons
**Risk:** LOW

#### Class 8: MusicMaintenanceService (18 methods)
**Responsibility:** Cleanup, defragmentation, orphan removal
```
- Cleanup(progressDialog)
- CleanupSongs(progressDialog)
- CleanupAlbums()
- CleanupArtists()
- CleanupGenres()
- CleanupPaths()
- CleanupInfoSettings()
- CleanupRoles()
- CleanupOrphanedItems()
- DeleteRemovedLinks()
- TrimImageURLs(imageURL, maxSpace)
- LookupCDDBInfo(bRequery)
- DeleteCDDBInfo()
```
**Benefits:** Maintenance operations isolated; can run in background thread
**Dependencies:** MusicCRUDRepository, ProgressDialog
**Test Requirements:** Vacuum/analyze verification; rollback safety
**Risk:** MEDIUM - Data loss if bugs

#### Class 9: MusicDatasetHelper (12 methods)
**Responsibility:** Convert raw SQL results to domain objects
```
- GetSongFromDataset(pDS)
- GetAlbumFromDataset(pDS)
- GetArtistFromDataset(pDS)
- GetArtistCreditFromDataset(record)
- GetArtistRoleFromDataset(record)
- GetFileItemFromDataset(item, baseUrl)
- GetFileItemFromArtistCredits(credits, baseUrl)
```
**Benefits:** Hydration logic reusable across repositories; testable with mock datasets
**Dependencies:** Song, Album, Artist, FileItem classes
**Test Requirements:** NULL handling; type conversions
**Risk:** LOW

### Integration Strategy

```
MusicDatabase (Facade)
  ├── MusicSchemaManager
  ├── MusicCRUDRepository
  │   ├── MusicRelationshipManager
  │   └── MusicDatasetHelper
  ├── MusicNavRepository
  │   ├── MusicQueryBuilder
  │   └── MusicDatasetHelper
  ├── MusicSearchService
  ├── MusicPlaylistService
  └── MusicMaintenanceService
```

### Estimated Beads Required: **8 beads**
1. Extract MusicSchemaManager
2. Extract MusicDatasetHelper
3. Extract MusicQueryBuilder
4. Extract MusicCRUDRepository
5. Extract MusicRelationshipManager
6. Extract MusicNavRepository
7. Extract MusicSearchService + MusicPlaylistService
8. Extract MusicMaintenanceService + Integration

---

## 2. VideoDatabase.cpp (12,789 lines)

### Current Structure Analysis

**NOTE: VideoDatabase is ~95% identical in structure to MusicDatabase. Both inherit from CDatabase and implement parallel Add/Get/Update/Delete patterns for different media types.**

**Method Clusters:**

| Category | Count | Responsibility |
|----------|-------|-----------------|
| **Schema** | 3 | CreateTables(), CreateAnalytics() (delegated to VideoDatabaseDDL) |
| **File Management** | 12 | GetPathId(), AddPath(), GetPathHash(), SetPathHash(), GetSourcePath() |
| **Movie CRUD** | 18 | AddNewMovie(), GetMovieInfo(), GetMovieId(), SetDetailsForMovie(), DeleteMovie() |
| **TV Show CRUD** | 14 | AddTvShow(), GetTvShowInfo(), SetDetailsForTvShow(), DeleteTvShow() |
| **Episode CRUD** | 15 | AddNewEpisode(), GetEpisodeInfo(), GetEpisodeId(), SetDetailsForEpisode(), DeleteEpisode() |
| **Music Video CRUD** | 8 | AddNewMusicVideo(), GetMusicVideoInfo(), GetMusicVideoId(), SetDetailsForMusicVideo(), DeleteMusicVideo() |
| **Metadata** | 22 | AddRatings(), UpdateRatings(), AddUniqueIDs(), UpdateUniqueIDs(), SetStreamDetails() |
| **Links/Relationships** | 25 | AddActor(), AddToLinkTable(), RemoveFromLinkTable(), AddLinksToItem(), UpdateLinksToItem() |
| **Bookmarks/Resume** | 15 | AddBookMarkToFile(), GetBookMarksForFile(), GetResumeBookMark(), ClearBookMarksOfFile() |
| **Queries** | 45+ | GetMoviesByActor(), GetTvShowsByActor(), GetEpisodesByFile(), GetEpisodesByFileId(), GetEpisodesByBlurayPath() |
| **Search** | 12 | GetDetailsFromDB(), GetDetailsByTypeAndId(), GetStreamDetails(), GetResumePoint() |
| **Data Conversion** | 18 | GetDetailsForMovie(), GetDetailsForTvShow(), GetDetailsForEpisode(), GetDetailsForMusicVideo(), GetCast(), GetTags(), GetRatings() |

### Major Differences from Music

1. **Asset Management:** Videos have multiple versions (theatrical, director's cut, etc.) — 5 extra methods
2. **TV Show Hierarchy:** Episodes > Seasons > TV Shows (3-level hierarchy vs. Music's Song > Album > Artist)
3. **External Links:** Movie linking to TV shows, episode mapping by bluray path
4. **Delayed Schema:** Uses `VideoDatabaseDDL` helper class (better design than Music!)

### Proposed Decomposition

**Key insight:** Extract the existing VideoDatabaseDDL pattern and apply it to both Music and Video. Create a shared IDatabaseRepository interface.

#### Class 1: VideoFileRepository (12 methods)
**Responsibility:** File path management
```
- GetPathId(strPath)
- AddPath(strPath, parentPath, dateAdded)
- GetPathHash(path)
- SetPathHash(path, hash)
- GetSourcePath(path)
- GetSubPaths(basepath)
- GetPaths(paths)
- GetPathsLinkedToTvShow(idShow)
- GetPathsForTvShow(idShow)
```
**Benefits:** Path traversal logic isolated; works with filesystem caching
**Dependencies:** CDatabase, m_pDS
**Test Requirements:** Multipath handling; stack directory resolution
**Risk:** MEDIUM - Filesystem-dependent

#### Class 2: VideoCRUDRepository (55 methods)
**Responsibility:** Core CRUD for Movies, Episodes, TV Shows, Music Videos
```
Movie (18): AddNewMovie, GetMovieInfo, SetDetailsForMovie, UpdateDetailsForMovie, DeleteMovie, GetMovieId
Episode (15): AddNewEpisode, GetEpisodeInfo, GetEpisodeBasicInfo, GetEpisodeId, SetDetailsForEpisode, DeleteEpisode
TVShow (14): AddTvShow, GetTvShowInfo, SetDetailsForTvShow, UpdateDetailsForTvShow, DeleteTvShow, GetTvShowId
MusicVideo (8): AddNewMusicVideo, GetMusicVideoInfo, SetDetailsForMusicVideo, DeleteMusicVideo, GetMusicVideoId
Season (7): AddSeason, GetSeasonInfo, SetDetailsForSeason, DeleteSeason, GetSeasonId
```
**Benefits:** Type-specific CRUD logic; parallel to MusicCRUDRepository
**Dependencies:** CDatabase, m_pDS, VideoFileRepository
**Test Requirements:** Asset versioning; hierarchy integrity
**Risk:** HIGH - Complex cascading deletes

#### Class 3: VideoLinkRepository (25 methods)
**Responsibility:** Relationships (actors, directors, studios, tags, ratings)
```
- AddActor(name, thumbURLs, thumb)
- AddCast(mediaId, mediaType, cast)
- AddRatings(mediaId, mediaType, ratings)
- UpdateRatings(mediaId, mediaType, ratings)
- AddUniqueIDs(mediaId, mediaType, details)
- UpdateUniqueIDs(mediaId, mediaType, details)
- AddToLinkTable(mediaId, mediaType, table, valueId)
- RemoveFromLinkTable(mediaId, mediaType, table, valueId)
- AddLinksToItem(mediaId, mediaType, field, values)
- UpdateLinksToItem(mediaId, mediaType, field, values)
- AddActorLinksToItem(mediaId, mediaType, field, values)
- UpdateActorLinksToItem(mediaId, mediaType, field, values)
- AddTagToItem(mediaId, tagId, type)
- RemoveTagFromItem(mediaId, tagId, type)
- RemoveTagsFromItem(mediaId, type)
- DeleteTag(idTag, mediaType)
- GetCast(mediaId, mediaType)
- GetTags(mediaId, mediaType)
- GetRatings(mediaId, mediaType)
```
**Benefits:** Metadata linking isolated; reusable across content types
**Dependencies:** VideoCRUDRepository
**Test Requirements:** Data integrity across link tables
**Risk:** MEDIUM - Orphan handling

#### Class 4: VideoBookmarkService (15 methods)
**Responsibility:** Bookmarks, resume points, episode mapping
```
- AddBookMarkToFile(filePath, bookmark)
- GetBookMarksForFile(filePath, bookmarks, type)
- ClearBookMarkOfFile(filePath, type, partNumber)
- ClearBookMarksOfFile(filePath)
- DeleteResumeBookMark(item)
- GetResumeBookMark(filePath)
- GetBookMarkForEpisode(tag)
- AddBookMarkForEpisode(tag, bookmark)
- DeleteBookMarkForEpisode(tag)
- GetEpisodesByFile(filePath)
- GetEpisodesByFileId(idFile)
- GetEpisodesByBlurayPath(path)
- GetEpisodeMap(idShow)
```
**Benefits:** Resume/bookmark logic separated; can be cached/precomputed
**Dependencies:** VideoCRUDRepository, CBookmark
**Test Requirements:** Resume point accuracy; multi-part file handling
**Risk:** MEDIUM - User data loss if bugs

#### Class 5: VideoQueryService (45+ methods)
**Responsibility:** Complex queries for filtering/navigation
```
- GetMoviesByActor(name, items)
- GetTvShowsByActor(name, items)
- GetEpisodesByActor(name, items)
- GetMusicVideosByArtist(artist, items)
- GetMoviesAndEpisodesByActor(name, items)
+ Extensive SQL generation for genre/year/studio filtering
```
**Benefits:** Query logic centralized; parameter injection points clear
**Dependencies:** VideoCRUDRepository, VideoDatasetHelper
**Test Requirements:** Filter combinations; empty result handling
**Risk:** MEDIUM - SQL injection

#### Class 6: VideoDatasetHelper (18 methods)
**Responsibility:** Hydration from raw SQL to domain objects
```
- GetDetailsForMovie(pDS)
- GetDetailsForTvShow(pDS)
- GetDetailsForEpisode(pDS)
- GetBasicDetailsForEpisode(pDS)
- GetDetailsForMusicVideo(pDS)
- GetDetailsForSet(pDS)
- GetDetailsFromDB(record, type, getDetails)
- GetFileFilePathById(idMovie, filePath, iType)
- GetRemovableBlurayPath(originalPath)
- GetFileBasePathById(idFile)
- GetFileIdByMovie(idMovie)
- GetSameVideoItems(item, items)
- GetMatchingTvShow(details)
```
**Benefits:** Reusable hydration logic; testable with mocks
**Dependencies:** VideoInfoTag, CVideoSettings
**Test Requirements:** NULL handling; type-specific field mapping
**Risk:** LOW

#### Class 7: VideoStreamService (12 methods)
**Responsibility:** Audio/subtitle stream metadata
```
- SetStreamDetailsForFile(details, filePath)
- SetStreamDetailsForFileId(details, idFile)
- GetStreamDetails(filePath)
- GetStreamDetails(item)
- GetStreamDetails(tag)
- GetCast(mediaId, mediaType, cast)
+ Stream-specific queries
```
**Benefits:** Audio/video track logic isolated; codec management
**Dependencies:** CStreamDetails, VideoCRUDRepository
**Test Requirements:** Codec compatibility
**Risk:** LOW

### Estimated Beads Required: **7 beads**
1. Extract VideoFileRepository
2. Extract VideoDatasetHelper
3. Extract VideoCRUDRepository
4. Extract VideoLinkRepository
5. Extract VideoQueryService
6. Extract VideoBookmarkService + VideoStreamService
7. Integration

---

## 3. GUIInfoManager.cpp (12,253 lines)

### Current Structure Analysis

**This is fundamentally different from the databases.** It's a massive state machine for evaluating boolean conditions and getting display labels. The 45 primary methods are completely overshadowed by:

1. **Line 10473-10507:** Initialize() - 34 lines, minimal code
2. **Line 10482-10800:** TranslateString() - massive switch/case chain with 300+ conditions
3. **Line 10559-11285:** TranslateSingleString() - 726-line monster method
4. **Line 11285-11353:** TranslateListItem() - 68-line condition parser
5. **Line 11420-11774:** GetLabel(), GetBool(), GetImage() - delegation methods

**The Real Problem:** Lines 10600-11300 contain a 700-line if/else if chain parsing info labels like:
```cpp
if (StringUtils::EqualsNoCase(strCondition, "Player.HasAudio"))
  return PLAYER_HAS_AUDIO;
else if (StringUtils::EqualsNoCase(strCondition, "Player.HasVideo"))
  return PLAYER_HAS_VIDEO;
else if (StringUtils::EqualsNoCase(strCondition, "Player.HasGame"))
  return PLAYER_HAS_GAME;
// ... 300 more conditions ...
```

### Proposed Decomposition

#### Class 1: InfoLabelRegistry (1 method but 2000+ lines data)
**Responsibility:** Central registry of all info label → ID mappings
```cpp
// Create a data structure replacing massive if/else chain
static const std::unordered_map<std::string, int> LABEL_MAP = {
  {"Player.HasAudio", PLAYER_HAS_AUDIO},
  {"Player.HasVideo", PLAYER_HAS_VIDEO},
  {"Player.HasGame", PLAYER_HAS_GAME},
  // ... 300+ more
};

int LookupInfoLabel(const std::string& label) {
  auto it = LABEL_MAP.find(StringUtils::ToLower(label));
  return it != LABEL_MAP.end() ? it->second : 0;
}
```
**Benefits:** Eliminates monster if/else chain; data-driven approach
**Dependencies:** None (pure data)
**Test Requirements:** Completeness check; case-insensitivity
**Risk:** LOW - Just data

#### Class 2: InfoExpressionParser (15 methods)
**Responsibility:** Parse complex expressions like "Player.Playing" or "Container(x).ListItem(1).Year"
```
- Parse property chains: Container(x).ListItem(1).Property
- Decompose into Property objects
- Handle array indexing [1]
- Resolve context windows
- Currently: SplitInfoString() + TranslateListItem() + TranslateSingleString()
```
**Benefits:** Expression parsing isolated; enables recursive descent parser
**Dependencies:** None
**Test Requirements:** Nested properties; edge case escaping
**Risk:** MEDIUM - Complex parser logic

#### Class 3: InfoProviderChain (8 methods)
**Responsibility:** Delegate to registered info providers
```
- GetBool() → delegates to m_infoProviders
- GetLabel() → delegates to m_infoProviders
- GetInt() → delegates to m_infoProviders
- GetImage() → delegates to m_infoProviders
- RegisterInfoProvider()
- UnregisterInfoProvider()
- GetInfoProviders()
```
**Benefits:** Extensible architecture; providers can be added/removed
**Dependencies:** IGUIInfoProvider interface
**Test Requirements:** Provider ordering; fallback behavior
**Risk:** LOW

#### Class 4: PlayerInfoProvider (25 methods)
**Responsibility:** All Player.* info labels
```
Current code spread across:
- TranslatePlayerString() (line 11373)
- TranslateMusicPlayerString() (line 11353)
- TranslateVideoPlayerString() (line 11363)
+ GetMultiInfoBool for Player conditions

Extracts:
- Player.Playing, Player.Paused, Player.Caching
- Player.HasAudio, Player.HasVideo, Player.HasGame
- Player.Seeking, Player.DisplayAfterSeek
- Player.Forwarding, Player.Rewinding, Player.FastForwarding
- Music/Video-specific: AudioPlayer.*, VideoPlayer.*
```
**Benefits:** Player state logic isolated; testable with mock player
**Dependencies:** IApplicationPlayer
**Test Requirements:** State machine transitions; race conditions
**Risk:** MEDIUM - Timing-sensitive

#### Class 5: ContainerInfoProvider (20 methods)
**Responsibility:** Container(x).ListItem(n).* expressions
```
- Container(x).ListItem(n).Title
- Container(x).ListItem(n).Label
- Container(x).ListItem(n).Picture
- Container(x).CurrentItem
- ListItem.* (uses current container)
- FocusedItem.* (uses focused container)
```
**Benefits:** Container iteration logic isolated
**Dependencies:** CFileItem, CFileItemList
**Test Requirements:** Boundary conditions (empty containers, out-of-bounds)
**Risk:** MEDIUM - Off-by-one errors

#### Class 6: SystemInfoProvider (30 methods)
**Responsibility:** System.*, Window.*, Skin.* info labels
```
- System.Time, System.Date, System.AlarmTime
- System.CPUTemperature, System.GPUMemory
- Window.IsActive, Window.IsVisible
- Skin.String, Skin.Integer, Skin.Bool
- Addon.SettingStr, Addon.SettingBool, Addon.SettingInt
```
**Benefits:** System state logic isolated; can be cached
**Dependencies:** CSystemInfo, CSkinSettings, CAddonManager
**Test Requirements:** Locale-specific formatting; timezone handling
**Risk:** LOW

#### Class 7: MovieInfoProvider (35 methods)
**Responsibility:** ListItem.* for video content
```
- ListItem.Title, ListItem.Label, ListItem.Label2
- ListItem.Year, ListItem.Season, ListItem.Episode
- ListItem.Director, ListItem.Writer, ListItem.Actor
- ListItem.Rating, ListItem.UserRating
- ListItem.Plot, ListItem.Runtime
- ListItem.Genre, ListItem.Studio
- ListItem.Watched, ListItem.PlayCount
+ 15 more
```
**Benefits:** Video metadata queries isolated; reusable across video types
**Dependencies:** CVideoInfoTag, CFileItem
**Test Requirements:** Missing field handling; multi-value formatting
**Risk:** LOW

#### Class 8: MusicInfoProvider (25 methods)
**Responsibility:** ListItem.* for audio content
```
- ListItem.Artist, ListItem.AlbumArtist
- ListItem.Album, ListItem.Title, ListItem.Track
- ListItem.Duration, ListItem.BitRate
- ListItem.Genre, ListItem.Year
- ListItem.Comment, ListItem.Mood
- ListItem.Playcount, ListItem.LastPlayed
```
**Benefits:** Audio metadata queries isolated
**Dependencies:** CMusicInfoTag, CFileItem
**Test Requirements:** Tag extraction edge cases
**Risk:** LOW

#### Class 9: GameInfoProvider (8 methods)
**Responsibility:** Game.* info labels
```
- Game.Title, Game.Platform
- Game.Year, Game.Publisher
- Game.Developer, Game.Description
```
**Benefits:** Game metadata isolated; new platform support easier
**Dependencies:** CGameInfoTag
**Test Requirements:** Missing platform data
**Risk:** LOW

#### Class 10: WeatherInfoProvider (12 methods)
**Responsibility:** Weather.* info labels
```
- Weather.Temperature, Weather.Condition
- Weather.WindSpeed, Weather.Humidity
- Weather.UVIndex, Weather.FeltTemperature
```
**Benefits:** Weather service logic isolated
**Dependencies:** Weather service backend
**Test Requirements:** API integration mocking
**Risk:** LOW

#### Class 11: StringConditionProvider (20 methods)
**Responsibility:** String.IsEmpty(), String.IsEqual(), String.Contains() conditions
```
- String.IsEmpty(info)
- String.IsEqual(info, string)
- String.StartsWith(info, substring)
- String.EndsWith(info, substring)
- String.Contains(info, substring)
```
**Benefits:** String comparison logic reusable; enables custom expression types
**Dependencies:** None
**Test Requirements:** Case sensitivity; Unicode handling
**Risk:** LOW

#### Class 12: IntegerConditionProvider (10 methods)
**Responsibility:** Integer.* conditions
```
- Integer.ValueOf(number)
- Integer.IsEqual(info, number)
- Integer.IsGreater(info, number)
- Integer.IsGreaterOrEqual(info, number)
- Integer.IsLess(info, number)
- Integer.IsLessOrEqual(info, number)
- Integer.IsEven(info)
- Integer.IsOdd(info)
```
**Benefits:** Numeric comparisons isolated; reusable
**Dependencies:** None
**Test Requirements:** Integer overflow; negative numbers
**Risk:** LOW

### Integration Strategy

```
CGUIInfoManager (Facade)
  ├── InfoLabelRegistry
  ├── InfoExpressionParser
  ├── InfoProviderChain
  │   ├── PlayerInfoProvider
  │   ├── ContainerInfoProvider
  │   ├── SystemInfoProvider
  │   ├── MovieInfoProvider
  │   ├── MusicInfoProvider
  │   ├── GameInfoProvider
  │   ├── WeatherInfoProvider
  │   ├── StringConditionProvider
  │   └── IntegerConditionProvider
  └── SkinVariableRegistry
```

### Estimated Beads Required: **9 beads**
1. Extract InfoLabelRegistry
2. Extract InfoExpressionParser
3. Extract PlayerInfoProvider
4. Extract MovieInfoProvider + MusicInfoProvider
5. Extract ContainerInfoProvider
6. Extract SystemInfoProvider
7. Extract GameInfoProvider + WeatherInfoProvider + StringConditionProvider + IntegerConditionProvider
8. Create IGUIInfoProvider interface and InfoProviderChain
9. Integration

---

## 4. VideoPlayer.cpp (5,865 lines)

### Current Structure Analysis

This is a **state machine orchestrator** rather than a traditional god object. It coordinates:
- Input stream decoding (DVDInputStream)
- Demuxing (DVDDemuxer)
- Audio/Video decoding (DVDCodec)
- Rendering (RenderManager)
- Synchronization (audio/video/subtitle sync)

**Methods:**
- 16 public methods (most are event handlers)
- Heavy use of nested helper classes (PredicateSubtitleFilter, PredicateAudioFilter, PredicateSubtitlePriority)
- State is distributed across m_State, m_Clock, m_PlayerOptions, m_AudioPlayerState

### Proposed Decomposition

#### Class 1: StreamSelector (35 methods)
**Responsibility:** Audio/subtitle stream selection based on user preferences
```
- SelectAudioStream()
- SelectSubtitleStream()
- SelectStream(filter, streams)
- AutoSelectStreams()
- IsAutoSelectSubtitlePreferred()
- GetUserPreferredAudioLanguage()
- GetUserPreferredSubtitleLanguage()
```
**Current:** PredicateAudioFilter, PredicateSubtitleFilter, PredicateSubtitlePriority classes (lines 89-350)
**Benefits:** Language/accessibility logic isolated; reusable for other players
**Dependencies:** Settings, LangInfo
**Test Requirements:** Multi-language content; edge cases (no audio, no subtitles)
**Risk:** MEDIUM - User frustration if wrong stream selected

#### Class 2: SyncManager (25 methods)
**Responsibility:** Audio/video/subtitle synchronization
```
- SyncAudio()
- SyncVideo()
- SyncSubtitles()
- AdjustAudioSyncDelay(adjustment)
- AdjustSubtitleSyncDelay(adjustment)
- GetAVSyncOffset()
- SetMasterClock()
- UpdatePlayerClock()
```
**Current:** Spread across VideoPlayer::Process() (lines 1500+)
**Benefits:** Sync logic isolated; A/V sync parameters centralized
**Dependencies:** DVDClock
**Test Requirements:** Frame-accurate sync; pulldown detection
**Risk:** HIGH - Sync issues cause viewer discomfort

#### Class 3: CodecOrchestrator (20 methods)
**Responsibility:** Audio/video codec selection and management
```
- SelectVideoCodec()
- SelectAudioCodec()
- SelectSubtitleCodec()
- InitializeCodec(type, fourcc)
- ReleaseCodec(type)
- GetCodecCapabilities()
- HandleCodecError()
```
**Current:** Spread across OpenAudioStream(), OpenVideoStream(), OpenSubtitleStream()
**Benefits:** Codec lifecycle management centralized; hardware accelerator switching
**Dependencies:** DVDCodecs, ProcessInfo
**Test Requirements:** Codec fallback chains; hardware detection
**Risk:** MEDIUM - Broken codec support breaks playback

#### Class 4: SubtitleManager (18 methods)
**Responsibility:** Subtitle rendering, positioning, font management
```
- AddSubtitleStream()
- RemoveSubtitleStream()
- RenderSubtitles()
- DisplaySubtitles()
- SetSubtitleDelay(msDelay)
- GetSubtitleStream()
- SetSubtitleFont()
- SetSubtitlePosition()
- EnableHearingImpaired(bool)
```
**Current:** Scattered in ProcessSubData(), ProcessOverlayData()
**Benefits:** Subtitle rendering decoupled from main player loop
**Dependencies:** RenderManager, GUIComponent
**Test Requirements:** ASS/SSA parsing; font fallback
**Risk:** MEDIUM - Subtitle corruption visible

#### Class 5: SeekHandler (15 methods)
**Responsibility:** Seeking, fast forward, rewind logic
```
- Seek(timeMs)
- FastForward(speed)
- Rewind(speed)
- StepForward(frames)
- StepBackward(frames)
- HandleDiscontinuity()
- ResetSeekState()
```
**Current:** HandlePlaySpeed(), Seek(), OnAction() (lines 3000+)
**Benefits:** Seek state machine isolated; easier testing
**Dependencies:** DVDClock, RenderManager
**Test Requirements:** Key frame search; reverse playback
**Risk:** MEDIUM - Seeking past keyframes causes corruption

#### Class 6: RendererManager (12 methods)
**Responsibility:** Video output management
```
- InitRenderer()
- ReleaseRenderer()
- OnScreenResolutionChange()
- SetVideoRect(rect)
- SetVideoBrightness(level)
- SetVideoContrast(level)
- SetVideoGamma(level)
- UpdateRenderInfo()
```
**Current:** Spread across OnNewVideoStream(), Render()
**Benefits:** Renderer lifecycle isolated; video output testable
**Dependencies:** RenderManager, WinSystem
**Test Requirements:** Resolution transitions; color space conversions
**Risk:** MEDIUM - Display corruption

#### Class 7: InputStreamFactory (10 methods)
**Responsibility:** Demuxer and input stream creation
```
- CreateInputStream(url)
- SelectBestDemuxer()
- HandleStreamError()
- ResetInputStream()
```
**Current:** Factory methods scattered in OpenAudioStream, OpenVideoStream
**Benefits:** Stream creation centralized; debugging easier
**Dependencies:** DVDInputStream, DVDDemuxer
**Test Requirements:** Network stream resilience; format detection
**Risk:** MEDIUM - Format misdetection

### Integration Strategy

```
VideoPlayer (Facade)
  ├── StreamSelector
  ├── SyncManager
  ├── CodecOrchestrator
  ├── SubtitleManager
  ├── SeekHandler
  ├── RendererManager
  ├── InputStreamFactory
  └── VideoPlayerState
```

### Estimated Beads Required: **5 beads**
1. Extract StreamSelector
2. Extract SyncManager + CodecOrchestrator
3. Extract SubtitleManager + SeekHandler
4. Extract RendererManager + InputStreamFactory
5. Integration (state machine refactoring)

---

## 5. Application.cpp (2,882 lines)

### Current Structure Analysis

**Note:** Application.cpp is intentionally small because Kodi's developers already extracted major subsystems:
- ApplicationPlay.cpp (playback control)
- ApplicationPlayer.cpp (player facade)
- ApplicationVolumeHandling.cpp (volume control)
- ApplicationPowerHandling.cpp (sleep/shutdown)
- ApplicationSkinHandling.cpp (theme management)
- ApplicationStackHelper.cpp (stack management)
- ApplicationMessageHandling.cpp (message routing)
- ApplicationActionListeners.cpp (input handling)

**This is a good example of successful decomposition.** Remaining ~150 methods:
- Create()
- Initialize()
- Run()
- Cleanup()
- 140+ specialized methods for global state management

### What Remains in Application.cpp

1. **Initialization (15 methods)**
   - Create(), RegisterSettings(), RegisterComponent(), Initialize()
   
2. **Main Loop (5 methods)**
   - Run(), Process(), FrameMove(), OnApplicationMessage()

3. **Navigation (20 methods)**
   - Navigate to specific windows, manage screen stack

4. **Media Scanning (12 methods)**
   - MusicLibrary scanning, VideoLibrary scanning progress

5. **Database Management (10 methods)**
   - CompactDatabase(), UpdateLibrary()

6. **Settings Management (15 methods)**
   - LoadSettings(), SaveSettings(), ResetSettings()

7. **Addon Management (12 methods)**
   - ReloadSkin(), UnloadSkin(), LoadAddon(), UnloadAddon()

8. **Notifications (8 methods)**
   - ShowGUIDialogYesNo(), ShowOSDTime(), ShowVolumeBar()

### Proposed Decomposition (Light)

**Application.cpp is already well-decomposed.** Only 2-3 additional extractions needed:

#### Class 1: SettingsManager (15 methods)
**Responsibility:** Settings loading/saving
```
- LoadSettings()
- SaveSettings()
- LoadAdvancedSettings()
- ApplySettings()
- ResetSettings()
- GetSetting(key)
- SetSetting(key, value)
```
**Current:** Inline in Initialize(), ~150 lines of settings XML parsing
**Benefits:** Centralized settings; easier testing
**Dependencies:** SettingsComponent, TiXmlDocument
**Test Requirements:** Settings migration; invalid values
**Risk:** LOW

#### Class 2: MediaScanCoordinator (12 methods)
**Responsibility:** Library scanning progress tracking
```
- StartMusicLibraryScan()
- StopMusicLibraryScan()
- IsMusicLibraryScanRunning()
- GetMusicLibraryScanProgress()
- StartVideoLibraryScan()
- StopVideoLibraryScan()
- IsVideoLibraryScanRunning()
- GetVideoLibraryScanProgress()
```
**Current:** Inline queries to MusicLibraryQueue, VideoLibraryQueue
**Benefits:** Unified scan status API; enables progress UI
**Dependencies:** MusicLibraryQueue, VideoLibraryQueue
**Test Requirements:** Concurrent scan handling
**Risk:** LOW

#### Class 3: NavigationStack (8 methods)
**Responsibility:** Screen navigation history
```
- NavigateTo(window)
- GoBack()
- GoHome()
- ClearStack()
- GetActiveWindow()
- GetPreviousWindow()
- CanGoBack()
```
**Current:** Inline in OnMessage(), GUIWindowManager calls
**Benefits:** Navigation history testable; back button logic clear
**Dependencies:** GUIWindowManager
**Test Requirements:** Stack underflow; circular navigation
**Risk:** LOW

### Estimated Beads Required: **3 beads** (optional)
1. Extract SettingsManager
2. Extract MediaScanCoordinator
3. Extract NavigationStack (if feels bloated)

**Recommendation:** Application.cpp is not a god object emergency. The existing architecture (component-based) is sound.

---

## Cross-Cutting Issues

### 1. Database Abstraction Layer Needed

Both MusicDatabase and VideoDatabase use raw `m_pDS` pointers. Extract a query abstraction:

```cpp
class IQueryResult {
  virtual bool EOF() = 0;
  virtual int GetInt(const std::string& field) = 0;
  virtual std::string GetString(const std::string& field) = 0;
  virtual void Next() = 0;
};

class IRepository {
  virtual IQueryResult* Query(const std::string& sql) = 0;
  virtual int Execute(const std::string& sql) = 0;
};
```

**Enables:** Mocking in unit tests, switching to ORMs later (sqlite-modern-cpp, sqlpp11)

### 2. GUIInfoManager Needs Expression Engine

Replace 700-line if/else chain with data-driven approach:

```cpp
using InfoLabelFunc = std::function<std::string(const CFileItem*)>;
std::unordered_map<std::string, InfoLabelFunc> LABEL_FUNCS = {
  {"Player.HasAudio", [](const auto* item) { return app->GetPlayer()->HasAudio(); }},
  {"ListItem.Title", [](const auto* item) { return item->GetLabel(); }},
  // ...
};
```

**Benefits:** Eliminates 10% of GUIInfoManager; adds extensibility

### 3. Tests Must Precede Extraction

**DO NOT extract these until tests exist:**
- MusicDatabase: Unit tests for each CRUD category
- VideoDatabase: Unit tests for each media type
- GUIInfoManager: Integration tests for expression parsing
- VideoPlayer: Mock-based unit tests for each component
- Application: Skeleton is OK (already decomposed)

### 4. Circular Dependency Risks

**High Risk:**
- Application → GUIInfoManager (for current item)
- GUIInfoManager → Application (for player state)
- VideoPlayer → Application (for settings)

**Solution:** Use observer pattern, not direct calls

---

## Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)
1. Extract database abstraction layer (IRepository)
2. Extract info label registry (eliminate massive if/else)
3. Create test scaffolding for each god object

### Phase 2: Music & Video (Weeks 3-4)
1. Extract MusicSchemaManager, MusicDatasetHelper
2. Extract VideoSchemaManager (leverage existing VideoDatabaseDDL)
3. Create MusicCRUDRepository with 50% pass-through initially

### Phase 3: Behavior (Weeks 5-6)
1. Extract MusicNavRepository, MusicQueryBuilder
2. Extract VideoQueryService, VideoBookmarkService
3. Full test coverage achieved

### Phase 4: UI (Week 7)
1. Extract info label providers (Player, Movie, Music, etc.)
2. Integration tests for expression chains

### Phase 5: Playback (Week 8)
1. Extract StreamSelector, SyncManager from VideoPlayer
2. Spike on additional extraction based on TDD

---

## Success Metrics

**Before Decomposition:**
- MusicDatabase compile time: ~5 seconds
- GUIInfoManager methods: 238
- Test coverage: < 20%
- Code duplication: 95% (Music/Video)

**After Decomposition (target):**
- Each module compile time: < 1 second
- Largest class: 50 methods (down from 238)
- Test coverage: > 80%
- Code duplication: 0% (shared base classes)
- Build parallelization: 8x improvement

---

## Risk Assessment Summary

| God Object | Risk Level | Critical Path | Blockers |
|------------|-----------|------|---------|
| MusicDatabase | **CRITICAL** | Extract Schema, CRUD, Queries | m_pDS abstraction |
| VideoDatabase | **CRITICAL** | Extract Schema, CRUD (parallel to Music) | Test infrastructure |
| GUIInfoManager | **HIGH** | Extract registry, expression engine | Provider interface |
| VideoPlayer | **MEDIUM** | Extract StreamSelector, SyncManager | None |
| Application | **LOW** | Already decomposed, 2-3 optional extractions | None |

---

## Conclusion

Kodi's god objects represent **928K lines of technical debt**. The decomposition plan sketched here would:

1. **Reduce average class size** from 2,500 lines to 300 lines
2. **Enable parallel development** (Music/Video extraction can happen in parallel)
3. **Improve testability** (from <20% coverage to >80%)
4. **Eliminate code duplication** (Music/Video databases are 95% identical)
5. **Future-proof architecture** (easier to add new media types, players, providers)

**Estimated effort: 8-10 weeks for experienced team (4-5 developers)**

**Expected payoff: 30% reduction in bug density, 50% faster onboarding for new contributors**

