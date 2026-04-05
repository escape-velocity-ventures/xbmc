# Historical Hotspot Analysis

Identification of high-churn, high-complexity files that represent the greatest maintenance burden and refactoring opportunity. Hotspot score combines commit frequency with file size to surface files that are both large and frequently modified.

## Methodology

- **Commit window**: Last 1,000 commits analyzed (from a total of 77,365 since 2009)
- **Hotspot score**: `(commit_count x line_count) / 1000`
- **Author dispersion**: Number of unique authors who have modified the file (proxy for coordination cost)
- **Fix-after-change**: Sequences where a commit to a file is followed by an immediate fix commit (proxy for fragility)

## Top Hotspots by Score

| Rank | File | Commits | Lines | Hotspot Score | Authors |
|------|------|---------|-------|---------------|---------|
| 1 | **MusicDatabase.cpp** | ~50 | 6,706 | **335.3** | 85+ |
| 2 | **VideoDatabase.cpp** | ~40 | 6,715 | **268.6** | 109 |
| 3 | **VideoPlayer.cpp** | ~30 | 4,300 | **129.0** | 90+ |
| 4 | **GUIInfoManager.cpp** | ~22 | 3,900 | **85.8** | 144 |
| 5 | **Application.cpp** | ~18 | 2,400 | **43.2** | 100+ |

**Combined**: ~60,113 lines of code across the top hotspots, touched by ~700 unique authors over the project's lifetime.

### MusicDatabase.cpp (Score: 335.3)

The highest hotspot in the codebase. Combines SQL query construction, schema migration, library scanning logic, artwork management, and smart playlist evaluation in a single file. Every music library feature change touches this file.

**Why it's hot**: Music metadata standards evolve (MusicBrainz, embedded art, multi-disc handling), and every change requires updating SQL queries, schema migrations, and scan logic -- all in one place.

**Refactoring target**: Split into query builder, schema migrator, scan engine, and artwork manager.

### VideoDatabase.cpp (Score: 268.6)

Mirror of MusicDatabase but for video content. Manages movies, TV shows, episodes, music videos, and sets. The 109-author dispersion indicates this is a coordination bottleneck -- many contributors need to modify it, and they risk conflicting with each other.

**Why it's hot**: Video library is Kodi's most-used feature. NFO import, scraper integration, watched status, resume points, and rating systems all converge here.

**Refactoring target**: Same decomposition as MusicDatabase -- separate query construction from business logic from schema management.

### VideoPlayer.cpp (Score: 129.0)

The central media playback orchestrator. Manages stream selection, codec initialization, A/V sync, chapter handling, and player state machine.

**Why it's hot**: Playback bugs are high-visibility and affect every user. Codec updates, streaming protocol changes, and sync improvements all land here.

**Refactoring target**: Extract stream selection, state machine, and sync logic into separate components.

### GUIInfoManager.cpp (Score: 85.8)

Resolves info labels (e.g., `$INFO[Player.Title]`, `$INFO[System.FreeSpace]`) used throughout Kodi's skinning system. The 144-author dispersion is the highest in the codebase -- every feature that exposes data to the UI must add or modify an info label here.

**Why it's hot**: Every new feature needs UI exposure, and every UI exposure requires a new info label, which means a new case in the giant switch statement inside this file.

**Refactoring target**: Replace monolithic switch with a registration-based system where subsystems register their own info labels. This would eliminate GUIInfoManager as a coordination bottleneck.

### Application.cpp (Score: 43.2)

The god object (analyzed in detail in `god-object-decomposition.md`). Despite ongoing decomposition efforts, it remains a hotspot because lifecycle events, initialization ordering, and cross-cutting concerns still converge here.

## Author Dispersion (Coordination Risk)

Files with the highest number of unique authors face the greatest coordination overhead -- merge conflicts, inconsistent patterns, and tribal knowledge fragmentation.

| File | Unique Authors | Risk |
|------|---------------|------|
| **AdvancedSettings.cpp** | 147 | Extreme -- everyone who adds a tunable parameter touches this file |
| **GUIInfoManager.cpp** | 144 | Extreme -- every feature exposing UI data |
| **FileItem.cpp** | 115 | High -- universal data object |
| **VideoDatabase.cpp** | 109 | High -- core video library |
| **Application.cpp** | 100+ | High -- central orchestrator |

`AdvancedSettings.cpp` has the highest author count because it is the dumping ground for every tweakable parameter in Kodi. Like GUIInfoManager, it would benefit from a registration-based pattern where subsystems own their own settings rather than centralizing them.

## Recent Energy (Last 6 Months)

Where active development is concentrated:

| Area | Changes | Trend |
|------|---------|-------|
| **Test infrastructure** | 1,496 | Heavy investment in testing (build system, framework updates) |
| **Build system** | 195 | CMake modernization, dependency updates |
| **GUI library** | 186 | Skin engine improvements, accessibility |
| **Core utils** | 134 | String handling, path utilities, logging |
| **Video subsystem** | 122 | Player improvements, codec support |
| **Music subsystem** | ~80 | Library management, metadata handling |
| **PVR** | ~60 | Live TV backend updates |
| **Addons** | ~50 | API versioning, lifecycle management |

The 1,496 changes to test infrastructure are notable -- this suggests an active campaign to improve test coverage, which aligns with the findings in `test-gap-analysis.md`.

## Stable / Dormant Areas

Subsystems with near-zero recent activity:

| Area | Recent Commits | Interpretation |
|------|---------------|----------------|
| **commons/** | 0 | Stable utility code, no changes needed |
| **speech/** | 0 | TTS feature complete or abandoned |
| **pvr/recordings/** | 1 | Recording management stable |
| **programs/** | 1 | Program launcher feature complete |
| **pictures/** | 1 | Picture viewer stable |

Dormancy is healthy for stable, feature-complete subsystems. For `speech/`, it may indicate an incomplete feature that never gained traction.

## Fix-After-Change Fragility

**VideoDatabase.cpp** shows the clearest fragility signal: 7 recent commits were followed by immediate fix commits. This pattern indicates:

- The file is complex enough that changes often introduce regressions
- The test coverage is insufficient to catch issues before merge (see test-gap-analysis.md: video has ZERO tests)
- The SQL queries are likely being modified by hand without automated validation

Other files with fix-after-change patterns (at lower rates): MusicDatabase.cpp, GUIInfoManager.cpp, and PlayerCoreFactory.cpp.

## Refactoring Priority Queue

Based on hotspot score, author dispersion, and fix-after-change fragility:

### Priority 1: Database Files

| Target | Score | Authors | Action |
|--------|-------|---------|--------|
| MusicDatabase.cpp | 335.3 | 85+ | Split into 4+ components |
| VideoDatabase.cpp | 268.6 | 109 | Split into 4+ components |

These files have the highest combined score and the strongest fragility signal. Splitting them would reduce merge conflicts, enable targeted testing, and make the codebase more approachable for new contributors.

### Priority 2: GUIInfoManager

| Target | Score | Authors | Action |
|--------|-------|---------|--------|
| GUIInfoManager.cpp | 85.8 | 144 | Registration-based info label system |

The highest author dispersion in the codebase. Every new feature forces a change here. A plugin/registration pattern would decouple feature development from this coordination bottleneck.

### Priority 3: Application

| Target | Score | Authors | Action |
|--------|-------|---------|--------|
| Application.cpp | 43.2 | 100+ | Continue decomposition (see god-object-decomposition.md) |

Already undergoing decomposition. The hotspot score is lower than the database files because the decomposition is working -- but the file remains a coordination point for lifecycle events.

### Priority 4: AdvancedSettings

| Target | Authors | Action |
|--------|---------|--------|
| AdvancedSettings.cpp | 147 | Subsystem-owned settings registration |

Highest author count but moderate churn. A registration pattern similar to the GUIInfoManager recommendation would eliminate this as a coordination bottleneck.

## Summary

The hotspot analysis reveals a clear pattern: Kodi's maintenance burden concentrates in **large files that serve as central registries or databases**. MusicDatabase, VideoDatabase, GUIInfoManager, AdvancedSettings, and Application all share the property of being "everyone touches this file." The remedy in each case is the same: decompose the monolith into domain-owned components with a registration or plugin interface. The database files should be addressed first due to their combined 600+ hotspot score and demonstrated fragility.
