# Test Gap Analysis

Assessment of test coverage, framework usage, architectural barriers to testing, and priority recommendations for the Kodi codebase.

## Coverage Overview

| Metric | Value |
|--------|-------|
| Test files | 127 |
| Subsystems with tests | 21 |
| Subsystems without tests | 20 (49% of codebase) |
| Test-to-source ratio | 3.6% |
| Framework | Google Test 1.10.0+ |
| Mocking framework | None (no Google Mock) |
| Entry point | `xbmc/test/xbmc-test.cpp` |

The 3.6% test-to-source ratio is low by modern standards but not unusual for a large C++ media application with deep hardware and OS dependencies.

## Build System Integration

Tests are integrated via CMake:

- **Macro**: `core_add_test_library()` registers test sources
- **Build target**: `make check` runs all tests
- **Valgrind target**: `make check-valgrind` runs tests under memory analysis
- **Test data**: 60+ test data files (video samples, playlists, archives, images) stored alongside test sources

Tests are compiled into a single test binary (`xbmc-test`) rather than per-subsystem binaries. This simplifies CI but means all tests must link against the full Kodi object graph.

## Well-Tested Subsystems

| Subsystem | Test Files | Test Ratio | Notes |
|-----------|-----------|------------|-------|
| **utils/** | 83 | 0.47 | Best covered; string manipulation, URL parsing, file path logic |
| **playlists/** | 12 | 0.57 | Highest ratio; playlist format parsing is well-isolated |
| **filesystem/** | 12 | -- | File operation abstractions, protocol handlers |
| **settings/** | 6 | -- | Settings serialization and migration |
| **network/** | 6 | -- | Network utility functions |
| **music/** | 2 | -- | Minimal; only tag parsing |
| **dbwrappers/** | 1 | -- | Single test for SQLite abstraction |

The `utils/` subsystem is the clear testing leader with 83 test files covering string operations, hashing, URL manipulation, XML parsing, and other pure-logic utilities. This makes sense -- utility functions have minimal external dependencies and are straightforward to test.

## Zero-Test Subsystems

**20 subsystems (49% of the codebase by line count) have ZERO test files:**

| Subsystem | Approx. LOC | Risk Level | Barrier to Testing |
|-----------|-------------|------------|-------------------|
| **cores/** (players, decoders) | 158,000 | Critical | FFmpeg/hardware coupling |
| **pvr/** (live TV) | 64,000 | High | Complex addon API state machine |
| **guilib/** (GUI framework) | 61,000 | High | Rendering pipeline dependency |
| **interfaces/** (Python/JSON-RPC) | 54,000 | High | Interpreter embedding |
| **platform/** (OS abstraction) | 50,000 | Medium | OS API direct calls |
| **windowing/** (display backends) | 31,000 | Medium | Hardware/driver dependency |
| **peripherals/** | 13,000 | Medium | Device driver interaction |
| **dialogs/** | 12,000 | Medium | GUI + event loop coupling |
| **application/** | ~5,000 | Critical | God object (CApplication) |
| **rendering/** | ~8,000 | Low | GPU pipeline |
| **addons/** (manager) | ~15,000 | High | Filesystem + network + settings |
| **video/** (library) | ~20,000 | High | Database + scraper coupling |
| **pictures/** | ~3,000 | Low | Image library wrappers |
| **programs/** | ~2,000 | Low | Minimal logic |
| **games/** | ~10,000 | Medium | Libretro integration |
| **favourites/** | ~2,000 | Low | Simple CRUD |
| **weather/** | ~2,000 | Low | Addon wrapper |
| **speech/** | ~1,000 | Low | TTS engine wrapper |
| **commons/** | ~500 | Low | Stable utility code |
| **events/** | ~3,000 | Low | Event bus |

## Architectural Barriers to Testing

### 1. No Mocking Framework

Google Mock is not integrated, and tests rely on **real object initialization**. This means:

- Testing a component often requires constructing its entire dependency tree
- Database tests need a real SQLite instance
- Service tests need the `CServiceBroker` singleton initialized
- No ability to inject test doubles at API boundaries

**Impact**: Makes unit testing of tightly-coupled subsystems impractical. Tests become integration tests by default.

### 2. Monolithic Application Class

`CApplication` (and its decomposed parts) serves as the central orchestrator. Many subsystems access global state through `CServiceBroker::GetXxx()` calls, creating implicit dependencies that are invisible at the API level but required at runtime.

**Impact**: Even simple subsystem tests may fail without full application initialization.

### 3. Direct OS API Calls

Platform code calls POSIX, Win32, Android NDK, and Apple frameworks directly rather than through injectable abstractions. This makes platform-specific code untestable without the target OS.

**Impact**: The 50K lines in `platform/` are effectively untestable in CI (which runs on one platform).

### 4. Rendering Coupled to Window System

GUI and rendering code assumes a live display context. There is no headless rendering mode or mock display backend for testing.

**Impact**: The 61K lines in `guilib/` and 31K in `windowing/` cannot be tested without display hardware or a virtual framebuffer.

### 5. No Integration or E2E Test Suite

There is no higher-level test suite that exercises Kodi as a running application:

- No Selenium/Playwright-style UI tests
- No scenario tests (e.g., "add a video source, scan it, play a file")
- No addon loading/lifecycle tests
- No JSON-RPC client test suite

The entire test infrastructure is unit-level Google Test.

## Priority Recommendations

### Quick Wins (High Value, Low Barrier)

| Target | Current | Why | Approach |
|--------|---------|-----|----------|
| **playlists/** | 12 tests, 0.57 ratio | Already well-tested; extending coverage is incremental | Add edge case tests for M3U, PLS, XSPF parsing |
| **favourites/** | 0 tests | Simple CRUD with serialization; minimal dependencies | Direct unit tests on add/remove/reorder/serialize |
| **events/** | 0 tests | Event bus is a pure logic component | Test publish/subscribe/filter without side effects |

### Medium Effort, High Impact

| Target | Current | Approx. LOC | Why |
|--------|---------|-------------|-----|
| **music/** | 2 tests | 45,000 | Tag parsing is testable; database operations can use in-memory SQLite |
| **network/** | 6 tests | 46,000 | URL parsing, HTTP header handling, and protocol utilities are isolatable |
| **dbwrappers/** | 1 test | 7,000 | SQLite abstraction is foundational; more coverage protects all DB consumers |
| **settings/** | 6 tests | ~15,000 | Settings serialization and migration affect every subsystem |

### Requires Infrastructure Investment

| Target | Blocker | Required Investment |
|--------|---------|-------------------|
| **cores/** (158K LOC) | FFmpeg + hardware coupling | Mock media pipeline interfaces; test with synthetic streams |
| **guilib/** (61K LOC) | Display context requirement | Headless rendering backend or mock display |
| **pvr/** (64K LOC) | Addon API state machine | Mock PVR addon client; in-memory EPG database |
| **application/** | God object dependencies | Decompose CApplication first (see god-object-decomposition.md) |

## Recommended Next Steps

1. **Integrate Google Mock**: Add Google Mock to the test framework to enable dependency injection and test doubles. This is the single highest-leverage infrastructure change.

2. **Extract testable interfaces**: For the top zero-test subsystems, define abstract interfaces at subsystem boundaries. Even partial extraction enables testing the business logic without the full dependency chain.

3. **In-memory SQLite for DB tests**: The `dbwrappers/` layer already supports SQLite. Providing an in-memory database factory for tests would unlock testing of `MusicDatabase`, `VideoDatabase`, and `EPGDatabase` without filesystem side effects.

4. **Headless GUI mode**: A minimal mock rendering backend that satisfies the GUI framework's initialization requirements without actual display output would unlock testing of dialog logic, window navigation, and control state management.

5. **Coverage reporting**: Integrate `gcov`/`llvm-cov` into the CI pipeline to track coverage trends over time and identify regression in tested subsystems.

## Summary

Kodi's test coverage is concentrated in utility code and largely absent from the application's core functionality -- media playback, GUI, live TV, and library management. The primary barriers are architectural (tight coupling, global state, no mocking) rather than cultural (the existing tests are well-written and maintained). Addressing the architectural barriers through interface extraction and mock framework integration would make the untested 49% of the codebase progressively testable.
