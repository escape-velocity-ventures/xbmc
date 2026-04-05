# Build System Analysis

Deep analysis of Kodi's CMake build system: structure, dependency management, platform matrix, custom macros, and modernization status.

## Scale

| Metric | Count |
|--------|-------|
| CMakeLists.txt files | 362 |
| .cmake script files | 199 |
| Find modules | 93 |
| Root CMakeLists.txt | 737 lines |
| Estimated total build system code | ~11,900 lines |

The build system is substantial but proportional to a multi-platform C++ project of this size. The 93 Find modules reflect Kodi's large dependency surface.

## Root CMakeLists.txt

The root `CMakeLists.txt` is 737 lines (the file is ~31KB due to verbose CMake syntax) and handles:

- Minimum CMake version and policy settings
- Platform detection and toolchain selection
- Global compiler flags and standard settings (C++20)
- Feature option definitions (`ENABLE_*` flags)
- Subdirectory traversal
- Final link step for the Kodi executable

## Dependency Management

### Required External Dependencies (23)

These must be present for Kodi to build:

| Dependency | Purpose |
|-----------|---------|
| FFmpeg | Audio/video decode, demux, mux |
| OpenSSL / GnuTLS | TLS for network operations |
| SQLite3 | Local database (video/music library, EPG) |
| spdlog | Logging backend |
| fmt | String formatting (used via spdlog and directly) |
| PCRE / PCRE2 | Regular expressions |
| zlib | Compression |
| FreeType | Font rendering |
| FriBidi | Bidirectional text support |
| libxml2 | XML parsing |
| libxslt | XSLT transformations |
| libjpeg-turbo | JPEG decoding |
| libpng | PNG decoding |
| giflib | GIF decoding |
| tinyxml2 | Lightweight XML parsing |
| TagLib | Audio metadata reading |
| curl | HTTP/FTP transfers |
| flatbuffers | Serialization (addon API) |
| rapidjson | JSON parsing |
| crossguid | UUID generation |
| libfstrcmp | Fuzzy string comparison |
| liblzo2 | Fast compression |
| libass | Subtitle rendering |

### Optional Dependencies (23)

Feature-gated dependencies that enable additional functionality:

| Dependency | Feature | Guard |
|-----------|---------|-------|
| libbluray | Blu-ray playback | `ENABLE_BLURAY` |
| libcec | HDMI-CEC control | `ENABLE_CEC` |
| libudev | Linux device hotplug | `ENABLE_UDEV` |
| Wayland | Wayland display server | `ENABLE_WAYLAND` |
| X11 | X Window System | `ENABLE_X11` |
| GBM/DRM | Direct rendering | `ENABLE_GBM` |
| PulseAudio | Audio output | `ENABLE_PULSEAUDIO` |
| ALSA | Audio output (Linux) | `ENABLE_ALSA` |
| pipewire | Audio output (modern Linux) | `ENABLE_PIPEWIRE` |
| Avahi | Network service discovery | `ENABLE_AVAHI` |
| DBUS | Desktop integration | `ENABLE_DBUS` |
| microhttpd | Built-in web server | `ENABLE_MICROHTTPD` |
| libnfs | NFS filesystem | `ENABLE_NFS` |
| Samba (libsmbclient) | SMB filesystem | `ENABLE_SMBCLIENT` |
| libmysqlclient | MySQL database (music sharing) | `ENABLE_MYSQLCLIENT` |
| VAAPI | Video acceleration (Intel/AMD) | `ENABLE_VAAPI` |
| VDPAU | Video acceleration (NVIDIA) | `ENABLE_VDPAU` |
| DAV1D | AV1 software decode | `ENABLE_DAV1D` |
| LCMS2 | Color management | `ENABLE_LCMS2` |
| libudfread | UDF filesystem (Blu-ray) | `ENABLE_UDFREAD` |
| libinput | Input device handling | `ENABLE_LIBINPUT` |
| libxkbcommon | Keyboard layout handling | `ENABLE_XKBCOMMON` |
| Cap'n Proto | Serialization (experimental) | `ENABLE_CAPNPROTO` |

## Platform Matrix

### Supported Platforms (9)

| Platform | Toolchain | Graphics Backend(s) |
|----------|-----------|---------------------|
| **Linux** | GCC / Clang | X11, Wayland, GBM |
| **Android** | NDK (Clang) | Android (EGL/GLES) |
| **Windows** | MSVC | DirectX 11 |
| **Windows Store (UWP)** | MSVC | DirectX 11 (UWP) |
| **macOS** | Apple Clang | OpenGL (deprecated), Metal |
| **iOS** | Apple Clang | Metal |
| **tvOS** | Apple Clang | Metal |
| **FreeBSD** | Clang | X11, Wayland, GBM |
| **webOS** | Clang (LG SDK) | Wayland |

### Graphics Backends (6+)

- **X11**: Traditional Linux desktop
- **Wayland**: Modern Linux desktop compositor
- **GBM**: Kernel-mode-setting direct rendering (kiosks, embedded)
- **iOS/tvOS Metal**: Apple GPU framework
- **Android EGL/GLES**: Mobile/TV embedded graphics
- **Windows DirectX 11**: Desktop and UWP

The platform and graphics backend are selected at CMake configure time and determine which windowing, rendering, and input subsystems are compiled.

## CMake Style and Patterns

### Modern CMake Usage

Kodi uses modern CMake practices in most areas:

- **Imported targets**: Dependencies are exposed as `PkgConfig::libname` or `FindModule::Lib` with `::` naming
- **`target_link_libraries` with scope**: `PUBLIC`, `PRIVATE`, `INTERFACE` keywords properly used
- **Generator expressions**: `$<TARGET_PROPERTY:...>`, `$<BUILD_INTERFACE:...>` for conditional logic
- **Config-mode packages**: Where available, `find_package(foo CONFIG)` is preferred

### Legacy Patterns Still Present

Some older patterns persist in the root CMakeLists.txt and early-authored subdirectories:

- `include_directories()` without target scope (global include path pollution)
- `add_compile_options()` at root level (affects all targets)
- String-based variable manipulation instead of target properties

These are functional but make it harder to reason about per-target compilation settings.

## Key Build Macros

Kodi defines several custom CMake functions/macros used throughout the tree:

| Macro | Purpose |
|-------|---------|
| `core_add_library(name)` | Register a subdirectory as a static library component |
| `core_require_dep(dep)` | Declare a required external dependency |
| `core_optional_dep(dep)` | Declare an optional external dependency |
| `core_add_subdirs_from_filelist()` | Read subdirectory lists from text files |
| `core_add_test_library()` | Register test sources for the test binary |
| `core_link_library(target lib)` | Link a dependency with platform-aware settings |

These macros abstract away platform differences and ensure consistent build configuration across the 362 CMakeLists.txt files.

## Non-Standard Build Features

### Text-Based Dependency Specs

The `cmake/treedata/` directory contains text files listing subdirectories to include per platform. Rather than conditional `add_subdirectory()` calls scattered throughout, Kodi centralizes the directory tree structure in data files that the build macros read.

Example: `cmake/treedata/common/` lists directories built on all platforms; `cmake/treedata/android/` adds Android-specific directories.

This is unconventional but provides a clear overview of per-platform build composition.

### Texture Compilation Pipeline

Kodi compiles textures (skin images) into packed `.xbt` archives during the build. This is handled by a custom tool (`TexturePacker`) that is built as part of the Kodi build and then invoked to process skin assets.

### FlatBuffers Code Generation

The addon API uses FlatBuffers for serialization. `.fbs` schema files are compiled into C++ headers during the build using the `flatc` compiler.

### SWIG Code Generation

Python bindings are generated from SWIG `.i` files during the build. This produces C++ wrapper code that bridges Python calls to Kodi internals.

## Build Parallelism

### Compilation Phase

Kodi builds as **per-directory static libraries**, which enables high compilation parallelism:

- Each `core_add_library()` call creates an independent static library target
- These targets have no ordering dependencies on each other (only on generated headers)
- A 16-core machine can compile 16 translation units simultaneously

### Link Phase

The final link step is **serial and slow**:

- All static libraries are combined into a single executable using whole-archive linking
- This is a single `ld` invocation that processes the entire object graph
- On a large codebase, this can take 30-60 seconds even on fast hardware
- LTO (Link Time Optimization) is supported but makes this step significantly longer

## Estimated Build System Code

| Category | Files | Est. Lines |
|----------|-------|-----------|
| CMakeLists.txt | 362 | ~5,400 |
| .cmake modules | 199 | ~4,500 |
| Find modules (in .cmake count) | 93 | ~2,000 |
| **Total** | **561** | **~11,900** |

## Recommendations

1. **Migrate remaining `include_directories()` to target scope**: Replace global includes with `target_include_directories(PRIVATE ...)` to prevent accidental header leakage.

2. **Consider presets**: CMake presets (`CMakePresets.json`) could replace the text-based dependency specs with a standard mechanism, improving IDE integration.

3. **Split final link**: Investigate whether Kodi can be split into a core shared library plus a thin executable to reduce link times during development.

4. **Audit Find modules**: With 93 Find modules, some may be superseded by upstream config-mode packages that CMake can discover natively.
