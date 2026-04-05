# Kodi API Surface Analysis

Comprehensive mapping of Kodi's public API boundaries: addon binary API, Python scripting interface, JSON-RPC remote control, and internal extension points.

## Addon Binary API (C/C++)

### Location and Scale

The addon API lives in `kodi-dev-kit/include/kodi/` and represents Kodi's primary extensibility surface:

- **~150+ public classes** exposed to addon developers
- **200+ enums** defining constants, capabilities, and error codes
- **27,580 lines** of header code in the public API surface

### Addon Instance Types

Kodi supports **12 addon instance types**, each with its own API contract:

| Instance Type | Description | Relative Size |
|---------------|-------------|---------------|
| **PVR** | Personal Video Recorder (live TV, EPG, timers, recordings) | Largest -- 158K lines |
| **Inputstream** | Custom stream demuxing and decryption (e.g., DRM) | Large |
| **Game** | Emulator/game engine integration (libretro-style) | Medium |
| **AudioDecoder** | Custom audio format decoding | Small |
| **AudioEncoder** | Audio encoding (e.g., for CD ripping) | Small |
| **ImageDecoder** | Custom image format support | Small |
| **Peripheral** | Input device drivers (joysticks, controllers) | Medium |
| **Screensaver** | Screensaver rendering | Small |
| **ShaderPreset** | GPU shader pipeline presets | Small |
| **VFS** | Virtual filesystem providers (SMB, NFS, cloud) | Medium |
| **VideoCodec** | Hardware/software video codec integration | Medium |
| **Visualization** | Audio visualization rendering | Small |

PVR is by far the most complex API, reflecting the breadth of live TV functionality (EPG management, timer scheduling, recording lifecycle, channel groups, providers).

### Global API Modules

Seven modules provide cross-cutting functionality available to all addon types:

| Module | Purpose |
|--------|---------|
| **AddonBase** | Addon lifecycle, settings, status reporting |
| **AudioEngine** | Direct audio output (PCM streams, format negotiation) |
| **Filesystem** | File I/O, directory listing, URL handling |
| **General** | Kodi version info, localization, paths, dialogs |
| **Network** | HTTP requests, hostname resolution, wake-on-LAN |
| **Tools** | Timers, string utilities, version helpers |
| **versions** | API version constants and compatibility macros |

### GUI API

The GUI API exposes widget-level control for addon-created windows:

- **12 control types**: Button, Edit, FadeLabel, Image, Label, Progress, RadioButton, Rendering, SettingsSlider, Slider, Spin, TextBox
- **10 dialog types**: ExtendedProgress, FileBrowser, Keyboard, Numeric, OK, Progress, Select, TextViewer, YesNo, CSelectionList
- **7,538 lines** of GUI API headers

### Versioning

Each API module is **independently versioned** using semantic versioning:

- **19 versioned APIs** total
- Each carries a `MAJOR.MINOR.PATCH` version and a `MIN_VERSION` for backward compatibility
- Addons declare the API version they were built against; Kodi checks compatibility at load time
- Major version bumps indicate breaking ABI changes; minor versions add functionality
- Version constants are defined in `versions.h` and checked at addon load time

### ABI Stability Mechanism

Binary compatibility across Kodi releases is maintained through:

1. **C API wrapper layer**: All public functions are exposed as C functions (no C++ name mangling)
2. **`ATTR_DLL_EXPORT` / `ATTR_DLL_LOCAL`** visibility attributes controlling symbol exposure
3. **Function pointer tables**: Addons receive a struct of function pointers rather than linking directly
4. **Stable struct layouts**: Public structs use fixed-size fields; new fields are appended, never inserted

This design allows addons compiled against Kodi 20 to potentially run on Kodi 21 if the API minor version is compatible.

## Python Scripting API

### SWIG-Generated Interface

The Python API is generated via **SWIG** (Simplified Wrapper and Interface Generator) from C++ definitions:

| Module | Purpose |
|--------|---------|
| `xbmc` | Core functions: logging, sleep, conditions, settings |
| `xbmcgui` | Window, dialog, and control creation |
| `xbmcplugin` | Content plugin integration (directory listings, resolved URLs) |
| `xbmcvfs` | Virtual filesystem access |
| `xbmcaddon` | Addon metadata, settings, localization |
| `xbmcdrm` | DRM/crypto operations |
| `xbmcwsgi` | WSGI web server interface for webservice addons |

### Legacy Interface Layer

The `interfaces/legacy/` directory contains **18,648 lines** of bridge code between the Python API and Kodi internals:

- `Control.h`: **105K lines** -- wraps all GUI control types for Python
- `InfoTagVideo.h`: **88K lines** -- video metadata bridge
- These files are among the largest in the codebase and represent significant maintenance burden

### Deprecation Policy

Python API deprecations follow a **3-version window**:

1. Version N: Feature marked deprecated with warning in logs
2. Version N+1: Warning becomes more prominent
3. Version N+2: Feature removed

This gives addon authors approximately 2-3 years to migrate.

## JSON-RPC API

### Scale and Coverage

The JSON-RPC API provides remote control and automation:

- **24 operation classes** organized by domain (Player, VideoLibrary, AudioLibrary, GUI, System, etc.)
- **277K lines** of schema definitions (JSON Schema format)
- Covers the **entire feature set** of Kodi -- library management, playback control, settings, system info

### Structure

Each operation class groups related methods:

| Class | Example Methods |
|-------|----------------|
| `Player` | Open, Stop, PlayPause, Seek, SetSpeed |
| `VideoLibrary` | GetMovies, GetTVShows, SetMovieDetails, Scan, Clean |
| `AudioLibrary` | GetAlbums, GetSongs, SetArtistDetails |
| `GUI` | ActivateWindow, ShowNotification, GetProperties |
| `System` | GetProperties, Shutdown, Reboot, Hibernate |
| `Addons` | GetAddons, GetAddonDetails, SetAddonEnabled |
| `Files` | GetDirectory, GetFileDetails, PrepareDownload |
| `Settings` | GetSettings, SetSettingValue, GetCategories |

### Versioning Gap

The JSON-RPC API **lacks formal versioning** comparable to the binary addon API. While the schema is well-defined, there is no explicit version negotiation or backward compatibility contract. Clients typically discover available methods at runtime via `JSONRPC.Introspect`.

## API Boundary Integrity

### No Internal Header Leakage

Analysis confirms a **clean API boundary**: addon headers in `kodi-dev-kit/include/kodi/` do not include internal Kodi headers. The separation is maintained through:

- Forward declarations in public headers
- C-style opaque handles for internal objects
- Callback function pointers rather than virtual method overrides

This is a significant architectural achievement for a codebase of this age and prevents addons from depending on internal implementation details.

### Binary API Deprecation

The binary addon API has **minimal deprecation** -- once an ABI is published, it tends to remain stable for extended periods. Breaking changes are rare and coincide with major Kodi releases (e.g., Kodi 19 Leia to Kodi 20 Nexus).

## Summary

Kodi's API surface is large but well-structured. The binary addon API is the most mature, with proper versioning and ABI stability. The Python API is functional but carries significant legacy weight in the bridge layer. The JSON-RPC API is comprehensive but would benefit from formal versioning. The clean separation between public and internal headers is a strong architectural property that should be preserved.
