# Kodi Coding Conventions

Extracted from codebase analysis of naming patterns, formatting configs, commit history, and contributor documentation.

## Language Standard

Kodi targets **C++20**. The codebase makes use of modern language features including `std::optional`, structured bindings, `constexpr`, and fold expressions where appropriate. The transition from C++17 is largely complete, though legacy code predating the standard upgrade remains throughout.

## Formatting

| Rule | Value |
|------|-------|
| Brace style | Allman (opening brace on its own line) |
| Indentation | 2 spaces (no tabs) |
| Max line width | 100 columns |
| Enforced by | `.clang-format` at repo root |
| CI enforcement | Yes -- clang-format check runs in Jenkins |

A `.clang-tidy` configuration also exists and is enforced in CI, covering common static analysis checks beyond pure formatting.

## Naming Conventions

### Classes and Types

- **Classes**: PascalCase with a `C` prefix. Examples: `CFileItem`, `CApplication`, `CVideoDatabase`.
- **Interfaces**: PascalCase with an `I` prefix. Examples: `ILogger`, `IPlayer`, `ISettingCallback`.
- **Structs**: Generally PascalCase without prefix, though some older code uses the `C` prefix for structs as well.

### Methods and Functions

- **Methods**: PascalCase. Examples: `GetFileName()`, `SetVolume()`, `OnAction()`.
- **Free functions**: Same PascalCase convention in most subsystems.

### Variables and Members

| Scope | Prefix | Examples |
|-------|--------|----------|
| Instance members | `m_` with type hint | `m_strFileName`, `m_iPort`, `m_bStop`, `m_vecItems` |
| Static members | `ms_` | `ms_instance`, `ms_logger` |
| Global variables | `g_` | `g_application`, `g_windowManager` |
| Constants | `UPPER_CASE` | `DEFAULT_TIMEOUT`, `MAX_RETRY_COUNT` |
| Local variables | camelCase (no prefix) | `fileName`, `retryCount` |

The member prefix type hints follow a legacy Hungarian-influenced convention:
- `str` = string
- `i` = integer
- `b` = boolean
- `f` = float
- `vec` = vector/collection
- `p` = pointer

New code is not strictly required to use the type hints but should always use `m_` for members.

### Namespaces

- **Names**: `UPPER_CASE`. Examples: `KODI`, `ADDON`, `PVR`, `MUSIC`, `VIDEO`, `GUILIB`.
- **Indentation**: None. Code inside a namespace is not indented.
- **Nesting**: Common. `KODI::ADDON::INSTANCE`, `KODI::WINDOWING::X11`.

```cpp
namespace KODI
{
namespace ADDON
{

class CAddonMgr
{
  // not indented relative to namespace
};

} // namespace ADDON
} // namespace KODI
```

### Enums

- **Enum class name**: PascalCase. Example: `PlayerState`, `MediaType`.
- **Enum values**: `UPPER_CASE`. Example: `PLAYER_STATE_PLAYING`, `MEDIA_TYPE_VIDEO`.
- Prefer `enum class` over unscoped `enum` in new code.

## Include Order

Headers should be included in the following order, separated by blank lines:

1. **Own header** (the `.h` file corresponding to the `.cpp`)
2. **Kodi headers** (alphabetical within the group)
3. **System/standard library headers** (`<string>`, `<vector>`, `<memory>`, etc.)
4. **Third-party headers** (e.g., `<fmt/format.h>`, `<tinyxml2.h>`)
5. **Special Kodi headers** (platform-specific, generated)

Each group is sorted alphabetically. The `.clang-format` configuration enforces this ordering.

## Copyright and Licensing

All source files carry the following header:

```
/*
 *  Copyright (C) 2005-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
```

The SPDX identifier `GPL-2.0-or-later` is the standard license. Some vendored or third-party files carry their own licenses noted inline.

## Error Handling

**Kodi does not use C++ exceptions.** Error handling is performed through:

- **Boolean returns**: Functions return `bool` to indicate success/failure.
- **Error enums**: Subsystem-specific error codes (e.g., `PVR_ERROR`, `ADDON_STATUS`).
- **`std::optional`**: Used in newer code for operations that may not produce a value.
- **Out parameters**: Legacy pattern where results are written to reference parameters.

Exceptions are disabled at the compiler level for Kodi's own code. Third-party libraries that throw are wrapped at the boundary.

## Memory Management

- **New code**: Use `std::unique_ptr` and `std::shared_ptr` exclusively.
- **Legacy code**: Raw pointers remain widespread, particularly in the addon API boundary and older subsystems like `guilib`.
- **Ownership transfer**: Prefer `std::unique_ptr` with move semantics.
- **Weak references**: `std::weak_ptr` for breaking cycles in observer patterns.

Manual `new`/`delete` should not appear in new contributions.

## Threading

Kodi provides its own threading abstractions:

| Primitive | Purpose |
|-----------|---------|
| `CCriticalSection` | Mutex wrapper (recursive) |
| `CEvent` | Condition variable / event signal |
| `std::atomic` | Lock-free primitive state |
| `CThread` | Thread wrapper with lifecycle management |
| `CSingleLock` | RAII lock guard for `CCriticalSection` |

Direct use of `std::thread` or `std::mutex` is discouraged in favor of the Kodi wrappers, which provide consistent logging, naming, and lifecycle hooks.

## Logging

Logging uses `CLog::Log()` with spdlog/fmt internally:

```cpp
CLog::Log(LOGINFO, "Loading database version {}", version);
CLog::Log(LOGERROR, "Failed to open file: {}", path);
CLog::Log(LOGDEBUG, "{} - item count: {}", __FUNCTION__, items.Size());
```

### Log Levels

| Level | Usage |
|-------|-------|
| `LOGTRACE` | Extremely verbose, per-frame or per-packet |
| `LOGDEBUG` | Developer diagnostics, disabled in release |
| `LOGINFO` | Normal operational messages |
| `LOGWARNING` | Recoverable issues, degraded operation |
| `LOGERROR` | Failures that affect functionality |
| `LOGFATAL` | Unrecoverable, application will exit |

The `__FUNCTION__` macro is commonly used as the first format argument for traceability.

## Documentation

Doxygen is used with **Qt-style** comment blocks:

```cpp
/*!
 * \brief Load a media item from the database.
 * \param path The path to the media item.
 * \param item [out] The loaded item.
 * \return True if the item was found and loaded.
 */
bool LoadItem(const std::string& path, CFileItem& item);
```

Backslash commands (`\brief`, `\param`, `\return`) are preferred over `@` commands. Not all code is documented -- coverage is uneven, with public APIs better documented than internal implementation.

## Commit Messages

Commit messages follow the format:

```
[Component] Short description of the change
```

Examples:
- `[VideoPlayer] Fix seek regression with HLS streams`
- `[PVR] Add support for series recording`
- `[cmake] Update FindFFMPEG to handle shared libs`

The component tag in brackets identifies the subsystem. The description is imperative mood, concise, and focused on what the change does.

## Pull Request Process

1. Branch from `master`.
2. One logical change per PR -- avoid mixing unrelated changes.
3. Jenkins CI must pass (build, clang-format, clang-tidy, tests).
4. clang-format is enforced automatically -- non-conforming code will fail CI.
5. Code review by at least one maintainer with merge rights.
6. Squash merge is common for single-purpose PRs; merge commits for larger feature branches.

## Summary

The conventions reflect a 20-year-old codebase that has been progressively modernized. The C prefix convention and Hungarian-style member naming are legacy but consistently applied. Newer subsystems tend toward more standard C++ idioms while respecting the existing patterns for consistency. The `.clang-format` and `.clang-tidy` configs serve as the ground truth for formatting disputes.
