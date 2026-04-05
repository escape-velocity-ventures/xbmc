# Dead Code Detection

Analysis of orphaned code, defunct platform support, stale feature flags, and removal candidates in the Kodi codebase.

## Overall Assessment

The codebase is **surprisingly clean for a 20+ year project**. Active maintenance has removed most vestiges of abandoned platforms and features. There are no large-scale dead subsystems waiting to be discovered -- the findings below are localized and modest in scope.

## Orphaned Headers

**NO orphaned headers detected at root level.** Every header file in the main source tree has at least one corresponding include statement. This indicates disciplined header management, likely aided by tooling and CI checks.

## Defunct Platform Code

### Fully Removed Platforms

| Platform | Status | References Found |
|----------|--------|-----------------|
| Xbox Original (2002-era) | **ZERO references** | Completely removed |
| Windows CE | **ZERO references** | Completely removed |
| PowerPC Mac | **ZERO references** | Completely removed |

These platforms, which were part of Kodi's history as XBMC (Xbox Media Center), have been thoroughly cleaned out. No `#ifdef` guards, no comments, no build system references remain.

### Windows Store (UWP)

- **91 occurrences** across **37 files**
- Guarded by `TARGET_WINDOWS_STORE` preprocessor define
- Present in: windowing, filesystem, network, platform detection, build system
- **Status**: Depends on whether UWP is still an active shipping target. If Microsoft has deprecated UWP in favor of WinUI 3 / Windows App SDK, this code may be a removal candidate.
- **Action**: Verify with the Windows platform maintainers whether UWP builds are still produced and tested.

## Legacy Subsystems

### DllLoader / emu_msvcrt.cpp

- **2,087 lines** of MSVC runtime emulation
- Provides `fopen`, `malloc`, `printf`, and other C runtime functions for dynamically loaded plugins
- **Status**: Legacy but **active** -- still used for loading certain binary addons on platforms where the system C runtime differs from what the addon was compiled against
- **Removal risk**: High -- removing this would break plugin loading for affected addons
- **Recommendation**: Investigate whether all current addon types have migrated away from DllLoader. If so, schedule removal.

## TODO/FIXME/HACK Markers

**133 occurrences** across **90 files**. This is a low density for a codebase of this size, suggesting regular cleanup.

### Notable Instances

| File | Marker | Description |
|------|--------|-------------|
| `UPnPServer.cpp:287` | HACK + dead code | Comment explicitly states "dead code because of the HACK" -- a workaround that disabled a code path |
| `Teletext.cpp` | 8 TODOs | Legacy DVB teletext decoder with multiple incomplete features |
| `VideoDatabase.cpp` | Multiple FIXMEs | Database migration edge cases |
| `GUIInfoManager.cpp` | Several TODOs | Unimplemented info label handlers |

### Age Assessment

A `git blame` pass on these markers would determine how many are recent (active work items) versus ancient (forgotten). The Teletext TODOs are likely very old given the subsystem's dormant status (0-1 commits in recent months).

## Commented-Out Code

**40+ files** contain commented-out code blocks. Most are minimal (1-5 lines) and represent:

- Alternative implementations preserved for reference
- Debug code toggled off
- Temporarily disabled features

None of the commented blocks represent large disabled subsystems. This is cosmetic noise rather than structural dead code.

## Preprocessor Feature Flags

### HAVE_* Flags

All detected `HAVE_*` flags are **current and active**:

| Flag | Status | Purpose |
|------|--------|---------|
| `HAVE_LIBBLURAY` | Active | Blu-ray playback support |
| `HAVE_LIBCEC` | Active | HDMI-CEC control |
| `HAVE_LIBUDEV` | Active | Linux device hotplug |
| `HAVE_OPENGL` | Active | Desktop OpenGL rendering |
| `HAVE_GLES` | Active | Embedded/mobile OpenGL ES rendering |

No stale `HAVE_*` flags were found referencing removed or unavailable libraries.

## Safe Removal Candidates

These files can likely be removed with minimal risk:

| File | Lines | Rationale |
|------|-------|-----------|
| `dxerr.cpp` | ~200 | Old DirectX error string helper; modern Windows SDK provides `FormatMessage` |
| `dirent.h` polyfill | ~150 | POSIX directory iteration shim for Windows; could use `std::filesystem` (C++17/20) |

Both are small, self-contained, and have modern standard library replacements available.

## Investigate Before Removing

These require deeper analysis before any removal decision:

### UWP Subsystem (37 files)

- Removal would be justified if UWP is no longer a shipping platform
- The 37 files are spread across core subsystems, so removal would touch windowing, filesystem, network, and build system
- Estimated effort: Medium (1-2 days of focused work plus testing)

### DllLoader

- Removal depends on whether any shipped addons still use the legacy loading path
- If all addons have migrated to the modern addon API with C function pointer tables, DllLoader is dead weight
- Estimated effort: High -- requires addon ecosystem survey

### Teletext Subsystem

- 8 TODOs suggest incomplete implementation
- DVB teletext usage may be region-specific (Europe) and still valued by users
- Check usage telemetry or community feedback before considering removal

## Metrics Summary

| Category | Count | Severity |
|----------|-------|----------|
| Orphaned headers | 0 | None |
| Defunct platform code | 0 references | Clean |
| Questionable platform code (UWP) | 91 occurrences / 37 files | Low-Medium |
| Legacy active code (DllLoader) | 2,087 lines | Low |
| TODO/FIXME/HACK | 133 / 90 files | Low |
| Commented-out blocks | 40+ files | Cosmetic |
| Stale feature flags | 0 | None |
| Safe removal candidates | 2 files, ~350 lines | Easy win |

## Conclusion

The Kodi codebase has been well-maintained with respect to dead code removal. The absence of Xbox Original and Windows CE remnants -- platforms that were central to the project's early identity -- demonstrates a commitment to pruning. The primary areas for further cleanup are the UWP subsystem (pending platform status verification) and the DllLoader (pending addon migration assessment). The TODO/FIXME density is low and the commented-out code is minimal.
