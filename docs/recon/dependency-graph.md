# Include Dependency Graph Analysis

Static analysis of `#include` relationships across the Kodi codebase: fan-in/fan-out metrics, coupling hotspots, cross-subsystem dependencies, and modularization recommendations.

## Scale

| Metric | Value |
|--------|-------|
| Files analyzed | 4,194 |
| Total #include statements | 29,009 |
| Unique dependencies | 4,471 |
| Circular dependencies detected | **0** |

The absence of circular dependencies is a strong architectural signal. In a codebase of this size and age, circular includes are a common problem that leads to compilation order issues, forward-declaration hacks, and conceptual coupling. Kodi's clean dependency graph indicates disciplined header management.

## Most-Included Headers (Fan-In)

Headers sorted by the number of files that include them:

| Header | Inclusions | Category |
|--------|-----------|----------|
| `<string>` | 919 | Standard library |
| `ServiceBroker.h` | 891 | Kodi internal |
| `utils/log.h` | 877 | Kodi internal |
| `<memory>` | 743 | Standard library |
| `<vector>` | 674 | Standard library |
| `StringUtils.h` | 668 | Kodi internal |
| `<algorithm>` | 512 | Standard library |
| `<map>` | 389 | Standard library |
| `FileItem.h` | 356 | Kodi internal |
| `Settings.h` | 295 | Kodi internal |

### Analysis

The top Kodi-internal headers reveal the codebase's structural pillars:

- **ServiceBroker.h (891)**: The service locator pattern. Nearly every file in the codebase accesses services through this header. This is both a strength (consistent access pattern) and a risk (universal coupling to a single file).

- **utils/log.h (877)**: Logging is ubiquitous, which is expected and healthy. The logging API is stable, so this high fan-in carries low change-propagation risk.

- **StringUtils.h (668)**: String utility functions are universally needed in a media application that handles file paths, metadata, URLs, and user-facing text.

- **FileItem.h (356)**: The universal media item representation. Every subsystem that displays, plays, or manages content depends on this type.

- **Settings.h (295)**: Settings access is needed across the entire application. This high fan-in makes Settings a change-propagation risk (see recommendations below).

## Highest Fan-Out Files

Files with the most outgoing includes (direct dependencies):

| File | Includes | Category |
|------|----------|----------|
| **Application.cpp** | 153 | God object |
| **GUIWindowManager.cpp** | 139 | Window orchestration |
| **XBMCApp.cpp** | 93 | Android entry point |
| **VideoDatabase.cpp** | 69 | Video library |
| **MusicDatabase.cpp** | 64 | Music library |
| **GUIInfoManager.cpp** | 58 | Info label resolution |
| **PlayerCoreFactory.cpp** | 47 | Player selection |
| **AddonMgr.cpp** | 45 | Addon management |

### Interpretation

High fan-out indicates a file that orchestrates many subsystems or aggregates many concerns:

- **Application.cpp (153 includes)**: This file depends on virtually every subsystem in Kodi. It is the definition of a god object at the include level. The 153 includes mean that a change to any of those 153 headers could require recompilation of Application.cpp (and in practice, any change likely triggers recompilation of the final link target).

- **GUIWindowManager.cpp (139 includes)**: Must know about every window type to instantiate and route to them. A factory or registration pattern could reduce this.

- **XBMCApp.cpp (93 includes)**: Android-specific entry point that bridges JNI, platform services, and Kodi internals. Platform entry points tend to have high fan-out by nature.

## Fan-Out Distribution

| Include Count | Files | Percentage | Assessment |
|---------------|-------|------------|------------|
| 1-5 | 3,609 | 86.0% | Healthy |
| 6-10 | 372 | 8.9% | Normal |
| 11-20 | 143 | 3.4% | Acceptable |
| 21-50 | 49 | 1.2% | High -- review needed |
| 51-100 | 18 | 0.4% | Very high -- orchestrators |
| 100+ | 3 | 0.07% | God files |

**86% of files have 5 or fewer includes**, which is healthy. The problematic files are a small set of orchestrators and god objects that can be systematically addressed.

## Cross-Subsystem Coupling

### Most Depended-Upon Subsystems

| Subsystem | Internal Deps | External Deps | Total |
|-----------|--------------|---------------|-------|
| **utils/** | 3,580 | -- | Most-used internal library |
| **settings/** | -- | 295 | Universal configuration access |
| **guilib/** | -- | 189 | UI framework |
| **filesystem/** | -- | 167 | File access abstraction |
| **cores/** | 687 (internal) | ~120 | High internal cohesion |
| **addons/** | -- | ~95 | Addon lifecycle |

**utils/** is the clear foundation layer -- 3,580 internal dependency references make it the most critical shared code. It is also the best-tested subsystem (83 test files), which is appropriate given its role.

### Settings Coupling Detail

The `settings/` subsystem is accessed by virtually every domain:

| Consumer Subsystem | Include Count |
|-------------------|---------------|
| cores/ (players, decoders) | 157 |
| video/ | 124 |
| guilib/ | 72 |
| filesystem/ | 52 |
| platform/ | 52 |
| music/ | 48 |
| pvr/ | 41 |
| addons/ | 35 |
| network/ | 28 |
| Others | ~100 |
| **Total** | **295** (external to settings/) |

This coupling means a change to the Settings API propagates to 295 files across every major subsystem. The current monolithic `Settings.h` could be split into domain-specific settings headers (e.g., `VideoSettings.h`, `AudioSettings.h`, `NetworkSettings.h`) to reduce this fan-in.

## ServiceBroker Analysis

`ServiceBroker.h` is the most-included Kodi header (891 files) but has an interesting property:

- **Direct dependencies**: Only 2 (minimal header, forward-declares service types)
- **Transitive inclusions**: 891 (every consumer pulls it in)

ServiceBroker acts as a **thin indirection layer** -- it doesn't itself include the services it provides access to. This is well-designed: the header is lightweight and stable, so its 891 inclusions carry low compilation cost. The coupling risk is conceptual (everything depends on the service locator pattern) rather than compilation-practical.

However, ServiceBroker creates **implicit dependencies** that are invisible at the header level. A file that includes `ServiceBroker.h` and calls `CServiceBroker::GetPlayerCoreFactory()` has a runtime dependency on `PlayerCoreFactory` that isn't expressed in its includes. This makes dependency analysis harder and prevents the compiler from catching missing dependencies.

## Subsystem Cohesion

### High Cohesion (Good)

| Subsystem | Internal Deps | External Deps | Ratio |
|-----------|--------------|---------------|-------|
| cores/ | 687 | ~120 | 5.7:1 |
| pvr/ | 450+ | ~80 | 5.6:1 |
| music/ | 200+ | ~60 | 3.3:1 |

These subsystems talk to themselves much more than they talk to others, indicating well-bounded domain logic.

### Low Cohesion (Concerning)

| Subsystem | Internal Deps | External Deps | Ratio |
|-----------|--------------|---------------|-------|
| dialogs/ | ~30 | ~90 | 0.3:1 |
| favourites/ | ~10 | ~40 | 0.25:1 |
| events/ | ~15 | ~45 | 0.33:1 |

These subsystems depend more on external code than internal code. For `dialogs/`, this makes sense -- dialogs are inherently cross-cutting UI elements. For `favourites/` and `events/`, it suggests these might be better integrated into the subsystems they serve rather than existing as standalone modules.

## Recommendations

### 1. Modularize Settings (295 external deps to ~50 per module)

Split `Settings.h` into domain-specific headers:

```
settings/VideoSettings.h    -- used by video/, cores/VideoPlayer
settings/AudioSettings.h    -- used by cores/AudioEngine, music/
settings/NetworkSettings.h  -- used by network/, filesystem/
settings/GUISettings.h      -- used by guilib/, windowing/
settings/GeneralSettings.h  -- used by application/, platform/
settings/Settings.h          -- aggregate header (backward compat)
```

Each domain module would include only its relevant settings header, reducing the 295-file fan-in to ~50 per module. The aggregate header preserves backward compatibility during migration.

### 2. Extract Application.cpp Orchestration (153 to 40-50 includes)

Application.cpp's 153 includes can be reduced by extracting orchestration into purpose-specific classes:

- **InitializationSequence**: Startup ordering (currently inline in Application)
- **ShutdownSequence**: Teardown ordering
- **EventDispatcher**: Input and system event routing
- **ComponentRegistry**: Service registration (currently implicit)

Each extracted class would include only the subsystems it directly orchestrates, bringing individual file fan-out to the 40-50 range.

### 3. GUIWindowManager Registration Pattern (139 to ~20 includes)

Replace the explicit `#include` of every window type with a factory registration:

```cpp
// Instead of 139 #include "windows/GUIWindowFoo.h"
// Each window self-registers:
REGISTER_WINDOW(GUIWindowFoo, WINDOW_FOO);
```

GUIWindowManager would then only include the registration infrastructure (~20 headers) rather than every window implementation.

### 4. Consider Module-Level Include Budgets

Establish soft limits on fan-out as a code review heuristic:

| Category | Suggested Limit |
|----------|----------------|
| Utility / helper files | 5-10 includes |
| Domain logic files | 10-20 includes |
| Orchestrators | 30-50 includes |
| Entry points | 50-80 includes |

Files exceeding their category limit should be reviewed for decomposition opportunities.

## Summary

Kodi's dependency graph is structurally sound -- zero circular dependencies, 86% of files in the healthy 1-5 include range, and good subsystem cohesion in core domains. The problems are concentrated in a handful of god files (Application.cpp, GUIWindowManager.cpp) and universal dependencies (Settings.h, ServiceBroker.h). Modularizing Settings and extracting orchestration from the top fan-out files would yield the largest improvement in compilation times, change-propagation risk, and developer cognitive load.
