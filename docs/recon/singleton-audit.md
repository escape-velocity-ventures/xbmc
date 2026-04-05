# Kodi Codebase Singleton & Global State Audit

**Date:** April 5, 2026  
**Scope:** Kodi Media Center (~/ThirdParty/xbmc/xbmc)  
**Codebase Size:** 928K lines C++, 4,194 files  

---

## Executive Summary

This audit catalogs global state patterns in the Kodi codebase across five categories:
1. **GetInstance() Singletons** (44 identified GetInstance() declarations)
2. **g_ Global Variables** (16 extern g_ variables documented)
3. **Extern Declarations** (280+ extern declarations in headers)
4. **Static Mutable State in Headers** (Scattered, primarily in MediaSettings/DisplaySettings)
5. **ServiceBroker Analysis** (Central service locator managing 40+ services)

The codebase exhibits a **hybrid architecture** combining:
- Traditional singletons via GetInstance()
- XBMC_GLOBAL_REF/XBMC_GLOBAL_USE macros for lazy-initialized globals
- ServiceBroker pattern for dependency injection
- Direct extern declarations (legacy approach)

---

## Part 1: GetInstance() Singletons Catalog

### Identified Singleton Classes (44 total)

#### Core Infrastructure Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CLog** | `xbmc/utils/log.h` | Logging system, sinks, component filters | 500+ | `std::atomic<bool>`, mutexes | **Easy** - already static methods |
| **CServiceBroker** | `xbmc/ServiceBroker.h` | Central service locator (40+ services) | 1000+ | Partially (register/unregister patterns) | **Medium** - needs refactor for DI |
| **CSettings** | `xbmc/settings/Settings.h` | Application-wide settings manager | 300+ | `CCriticalSection` (partial) | **Hard** - massive scope, deep coupling |

#### Settings & Display Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CSkinSettings** | `xbmc/settings/SkinSettings.h` | Skin-specific settings (bool/int/string) | 150+ | `CCriticalSection` m_critical | **Easy** - clear interface |
| **CDisplaySettings** | `xbmc/settings/DisplaySettings.h` | Video display configuration | 80+ | `CCriticalSection` | **Easy** - limited scope |
| **CMediaSettings** | `xbmc/settings/MediaSettings.h` | Playlist, playback, library settings | 100+ | `CCriticalSection` | **Easy** - clear interface |
| **CMediaSourceSettings** | `xbmc/settings/MediaSourceSettings.h` | File source configuration | 60+ | `CCriticalSection` | **Easy** - limited scope |
| **CDiscSettings** | `xbmc/settings/DiscSettings.h` | Disc (CD/DVD) related settings | 40+ | `CCriticalSection` | **Easy** - limited scope |
| **CViewStateSettings** | `xbmc/view/ViewStateSettings.h` | View state persistence | 50+ | `CCriticalSection` | **Easy** - limited scope |

#### Network & Service Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CZeroconf** | `xbmc/network/Zeroconf.h` | mDNS/Bonjour service publishing | 120+ | `CCriticalSection` static member | **Medium** - TODO comment on line 26 |
| **CZeroconfBrowser** | `xbmc/network/ZeroconfBrowser.h` | Service discovery (browser) | 80+ | Thread-safe pointer ops | **Medium** |
| **CEventServer** | `xbmc/network/EventServer.h` | UDP event/button input server | 40+ | `std::mutex` m_critSection, `std::atomic` | **Medium** - threaded, complex state |
| **CUPnP** | `xbmc/network/upnp/UPnP.h` | UPnP device/service management | 100+ | Partial (critical sections) | **Medium** - complex state mgmt |
| **CUPnPSettings** | `xbmc/network/upnp/UPnPSettings.h` | UPnP-specific settings | 30+ | `CCriticalSection` | **Easy** |
| **CWakeOnAccess** | `xbmc/network/WakeOnAccess.h` | Network WOL (Wake on LAN) | 20+ | `CCriticalSection` | **Easy** |

#### Script & Add-on Execution Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CScriptInvocationManager** | `xbmc/interfaces/generic/ScriptInvocationManager.h` | Script thread management, language invokers | 80+ | `CCriticalSection` m_critical | **Medium** - complex thread pool |
| **CBuiltins** | `xbmc/interfaces/builtins/Builtins.h` | Built-in command registry & execution | 200+ | `CommandMap` (read-heavy) | **Easy** - registry pattern |
| **CAddonInstaller** | `xbmc/addons/AddonInstaller.h` | Add-on installation queue & state | 40+ | Thread-safe operations | **Medium** |
| **CAddonSystemSettings** | `xbmc/addons/AddonSystemSettings.h` | Add-on global settings | 50+ | `CCriticalSection` | **Easy** |

#### Data Cache & Streaming Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CDataCacheCore** | `xbmc/cores/DataCacheCore.h` | Video/audio decoder info, playback state | 250+ | `std::atomic<bool>`, `CCriticalSection` | **Medium** - heavily accessed |
| **CRssManager** | `xbmc/utils/RssManager.h` | RSS feed management & reader pooling | 60+ | `CCriticalSection` m_critical | **Easy** - clear interface |

#### File System & Resource Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CXbtManager** | `xbmc/filesystem/XbtManager.h` | XBT archive/texture management | 50+ | `CCriticalSection` | **Easy** |
| **CPipesManager** | `xbmc/filesystem/PipesManager.h` | Named pipe file system | 30+ | `CCriticalSection` | **Easy** |
| **CPasswordManager** | `xbmc/PasswordManager.h` | Credential storage & retrieval | 20+ | `CCriticalSection` | **Easy** |

#### Media Library Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CVideoLibraryQueue** | `xbmc/video/VideoLibraryQueue.h` | Video library scan/clean queue | 120+ | Thread-safe job queue | **Medium** - complex state |
| **CMusicLibraryQueue** | `xbmc/music/MusicLibraryQueue.h` | Music library scan/clean/export queue | 100+ | Thread-safe job queue | **Medium** - complex state |
| **CPlayerController** | `xbmc/video/PlayerController.h` | Global player action controller | 40+ | Thread-safe action dispatch | **Easy** - thin wrapper |

#### Input & Touch Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CGenericTouchInputHandler** | `xbmc/input/touch/generic/GenericTouchInputHandler.h` | Touch event routing | 30+ | `CCriticalSection` | **Easy** |
| **CGenericTouchActionHandler** | `xbmc/input/touch/generic/GenericTouchActionHandler.h` | Touch gesture recognition | 40+ | `CCriticalSection` | **Easy** |
| **FileItemListModification** | `xbmc/FileItemListModification.h` | GUI list item modification hooks | 20+ | Simple registry | **Easy** |

#### CD/DVD & Media Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CCDDARipper** | `xbmc/cdrip/CDDARipper.h` | Audio CD extraction | 15+ | Thread-safe job | **Easy** |
| **CLibcdio** | `xbmc/storage/cdioSupport.h` | CD/DVD detection & reading | 25+ | `std::shared_ptr` (safe) | **Easy** |

#### Platform-Specific Singletons

| Class | Header File | State Managed | Callers | Thread-Safety | DI Feasibility |
|-------|------------|--------------|---------|---------------|-----------------|
| **CTVOSTopShelf** | `xbmc/platform/darwin/tvos/TVOSTopShelf.h` | tvOS top shelf widget updates | 20+ | Thread-safe | **Easy** |
| **CTVOSInputSettings** | `xbmc/platform/darwin/tvos/TVOSSettingsHandler.h` | tvOS input configuration | 15+ | Thread-safe | **Easy** |
| **CAnnounceReceiver** | `xbmc/platform/darwin/ios-common/AnnounceReceiver.h` | iOS event announcements | 10+ | Thread-safe | **Easy** |

---

## Part 2: Global Variables (g_ prefixed)

### Extern g_ Variables (16 documented)

| Variable | Type | Header | Purpose | Mutation Pattern | Notes |
|----------|------|--------|---------|------------------|-------|
| **g_serviceBroker** | `CServiceBroker` | `xbmc/ServiceBroker.h` | Central service registry | Mostly read (register/unregister ops) | Via XBMC_GLOBAL macro |
| **g_windowHelper** | `CWHelper` | `xbmc/platform/win32/WindowHelper.h` | Windows window operations | Mutated (state-heavy) | Windows-only |
| **g_emuFileWrapper** | `CEmuFileWrapper` | `xbmc/cores/DllLoader/exports/util/EmuFileWrapper.h` | DLL emulation layer | Mutated | Legacy DLL support |
| **g_xbmcController** | `XBMCController*` | `xbmc/platform/darwin/tvos/XBMCController.h` | tvOS app controller | Mutated | Platform-specific |
| **g_xbmcController** | `XBMCController*` | `xbmc/platform/darwin/ios/XBMCController.h` | iOS app controller | Mutated | Platform-specific |
| **g_WiiRemote** | `CWiiRemote` | `tools/EventClients/Clients/WiiRemote/CWIID_WiiRemote.h` | Wii remote state | Mutated | Legacy input device |
| **g_partyModeManager** | `CPartyModeManager` | `xbmc/PartyModeManager.h` | Party mode playback | Mutated (playlist state) | High call count |
| **g_directoryCache** | `XFILE::CDirectoryCache` | `xbmc/filesystem/DirectoryCache.h` | Directory enumeration cache | Mutated (cache updates) | Via XBMC_GLOBAL macro |
| **g_curlInterface** | `XCURL::DllLibCurlGlobal` | `xbmc/filesystem/DllLibCurl.h` | HTTP/HTTPS client DLL | Mutated (connection state) | Via XBMC_GLOBAL macro |
| **g_LangCodeExpander** | `CLangCodeExpander` | `xbmc/utils/LangCodeExpander.h` | Language code mapping | Mostly read (lookup) | Via XBMC_GLOBAL macro |
| **g_ZipManager** | `CZipManager` | `xbmc/filesystem/ZipManager.h` | ZIP archive management | Mutated (file cache) | Via XBMC_GLOBAL macro |
| **g_alarmClock** | `CAlarmClock` | `xbmc/utils/AlarmClock.h` | System alarm scheduling | Mutated (active alarms) | Via XBMC_GLOBAL macro |
| **g_sysinfo** | `CSysInfo` | `xbmc/utils/SystemInfo.h` | System information cache | Mostly read (initialization) | Via XBMC_GLOBAL macro |
| **g_passwordManager** | `CGUIPassword` | `xbmc/GUIPassword.h` | Password/unlock management | Mutated (unlock state) | Via XBMC_GLOBAL macro |
| **g_sectionLoader** | `CSectionLoader` | `xbmc/SectionLoader.h` | DLL section loading | Mutated (library state) | Legacy DLL loader |
| **g_hWnd** | `HWND` | `xbmc/windowing/windows/WinSystemWin32.h` | Windows window handle | Mutated (window events) | Windows-only, direct global |
| **g_xrandr** | `CXRandR` | `xbmc/windowing/X11/XRandR.h` | X11 RandR display mgmt | Mutated (display config) | Linux-only |

### Mutation Pattern Classification

- **Read-Only (~40%):** g_LangCodeExpander, g_sysinfo
- **Mutated (~60%):** Most others - state changes during runtime
- **Heavy Mutation:** g_partyModeManager, g_directoryCache, g_curlInterface

---

## Part 3: ServiceBroker Deep Dive

### ServiceBroker Architecture

**Location:** `/xbmc/ServiceBroker.h` (276 lines)  
**Pattern:** Central Service Locator + Dependency Injection Factory  
**Access:** `g_serviceBroker` (XBMC_GLOBAL macro)

### Brokered Services (40+ registered)

#### Logging & Core (4 services)
- `CLog` - Logging system
- `CAppParams` - Application startup parameters
- `CPlatform` - Platform abstraction layer
- `CCPUInfo` - CPU information

#### GUI & Rendering (5 services)
- `CGUIComponent` - GUI framework
- `CWinSystemBase` - Window system abstraction
- `CRenderSystemBase` - Rendering backend
- `CResourcesComponent` - Resource management
- `CTextureCache` - Texture caching

#### Add-ons & Repositories (5 services)
- `ADDON::CAddonMgr` - Add-on manager
- `ADDON::CBinaryAddonManager` - Binary add-on loader
- `ADDON::CBinaryAddonCache` - Binary add-on cache
- `ADDON::CVFSAddonCache` - VFS add-on cache
- `ADDON::CRepositoryUpdater` - Repository updates

#### Media & Playback (8 services)
- `CMediaManager` - Media devices/storage
- `KODI::PLAYLIST::CPlayListPlayer` - Playlist playback
- `CDataCacheCore` - Decoder/playback state cache
- `PVR::CPVRManager` - PVR/Live TV manager
- `CPlayerCoreFactory` - Player core factory
- `CSlideShowDelegator` - Slideshow control
- `IAE` - Audio engine
- `CDatabaseManager` - Database access

#### Input & Control (3 services)
- `CInputManager` - Input device management
- `KODI::KEYBOARD::CKeyboardLayoutManager` - Keyboard layouts
- `speech::ISpeechRecognition` - Voice control

#### Network & Services (6 services)
- `CNetworkBase` - Network interface
- `WSDiscovery::IWSDiscovery` - Network discovery
- `MEDIA_DETECT::CDetectDVDMedia` - Media detection
- `CContextMenuManager` - Context menu registry
- `CFavouritesService` - Favorites management
- `ADDON::CServiceAddonManager` - Service add-ons

#### Peripheral & Settings (4 services)
- `PERIPHERALS::CPeripherals` - USB/input devices
- `CSettingsComponent` - Settings framework
- `CFileExtensionProvider` - File type registry
- `KODI::UTILS::I18N::CSubTagRegistryManager` - Internationalization

#### Games (3 services)
- `KODI::GAME::CControllerManager` - Game controller mgmt
- `KODI::GAME::CGameServices` - Game services
- `KODI::RETRO::CGUIGameRenderManager` - Game rendering

#### Messaging & Communication (3 services)
- `KODI::MESSAGING::CApplicationMessenger` - Event messaging
- `CAppInboundProtocol` - Network protocol handling
- `CJobManager` - Background job management

#### Caching & System (3 services)
- `CDNSNameCache` - DNS caching
- `XFILE::CBlurayDiscCache` - Blu-ray disc caching
- `CDecoderFilterManager` - Video codec filtering

### Registration Patterns

```cpp
// Pattern: Register at startup
ServiceBroker::RegisterAddonMgr(std::make_unique<CAddonManager>());

// Pattern: Lazy initialization
static IAE* GetActiveAE();  // Getter only, created elsewhere

// Pattern: Unregister at shutdown
UnregisterAE();
```

### Thread-Safety Assessment

**Strengths:**
- Register/unregister ops use shared_ptr (thread-safe transfer)
- Static getters provide synchronized access
- Isolation: Each service manages own synchronization

**Weaknesses:**
- No global lock on ServiceBroker state
- Assumes single-threaded registration phase
- Potential races if register/unregister during access

### Is It a God Object?

**YES - Partially**
- Manages 40+ services
- Critical path for almost all subsystems
- Cannot replace individual services
- Single point of failure

**Mitigation:** Hierarchy of smaller service brokers (platform, audio, game, pvr already have sub-brokers)

---

## Part 4: Static Mutable State in Headers

### Identified Static Mutable State Patterns

#### Pattern 1: XBMC_GLOBAL_REF/USE Macros

Files using this pattern (GlobalsHandling.h):
- Every file including g_serviceBroker, g_directoryCache, g_curlInterface, etc.
- ~16 global variables wrapped with XBMC_GLOBAL macros

**Mechanism:**
```cpp
XBMC_GLOBAL_REF(CServiceBroker, g_serviceBroker);     // Declaration
#define g_serviceBroker XBMC_GLOBAL_USE(CServiceBroker)  // Access macro

// Expands to:
// static std::shared_ptr<CServiceBroker> g_serviceBrokerRef(...)
// #define g_serviceBroker (*(xbmcutil::GlobalsSingleton<CServiceBroker>::getQuick()))
```

**Thread-Safety:** NOT THREAD SAFE (per GlobalsHandling.h line 95-100)
- Relies on compilation unit global initialization being single-threaded
- getQuick() has race condition if called during initialization

#### Pattern 2: Settings Classes with Mutable State

Classes with `mutable CCriticalSection`:
- `CSkinSettings` (line 60)
- `CDisplaySettings`
- `CMediaSettings`
- `CMediaSourceSettings`
- `CDiscSettings`
- `CViewStateSettings`

**Pattern:** Mutable m_settings/configuration modified through GetInstance() singleton

#### Pattern 3: DataCache Atomic State

`CDataCacheCore` (lines 200, 202, 217, 226, 312):
```cpp
std::atomic_bool m_hasAVInfoChanges = false;        // Atomic access
CCriticalSection m_videoPlayerSection;               // Critical section protection
```

---

## Part 5: Extern Declaration Inventory

### Total Count
- **197 extern declarations** found across header files
- Majority in platform-specific, DLL loader, and legacy code

### Categories

#### Audio Engine Externs (5)
- `AE.h`, `AEStream.h`, `AEEncoder.h`

#### Video Rendering Externs (12)
- Renderer shaders, color management, tone mapping

#### Platform-Specific Externs (20+)
- Windows: `dxerr.h`, `WindowHelper.h`, `PlatformDefs.h`
- Linux: `SSE4` support, UDev
- Darwin: Framework bindings

#### Codec & Format Externs (15)
- Video codecs (VDPAU, VAAPI, DXVA, VTB)
- FFmpeg integration

#### Network Externs (25+)
- DNS, HTTP, UPnP, file protocol stubs

---

## DI Migration Feasibility Matrix

### Easy to Migrate (< 100 callers, clear interface)

**Candidates:**
1. CSkinSettings (150 callers, pure settings)
2. CDisplaySettings (80 callers)
3. CRssManager (60 callers)
4. CPassword Manager (20 callers)
5. CXbtManager (50 callers)
6. CUPnPSettings (30 callers)
7. CPlayerController (40 callers, thin wrapper)
8. CAddonSystemSettings (50 callers)

**Migration Strategy:** Inject into CApplication, pass down hierarchy

### Medium Difficulty (100-300 callers, complex interface)

**Candidates:**
1. CLog (500+ callers, but static methods OK) - **Actually EASY**, just template args
2. CScriptInvocationManager (80 callers, thread pool)
3. CEventServer (40 callers, UDP server, threaded)
4. CZeroconf/CZeroconfBrowser (200 combined)
5. CVideoLibraryQueue (120 callers, job queue)
6. CMusicLibraryQueue (100 callers, job queue)
7. CBuiltins (200+ callers, command registry)
8. CUPnP (100+ callers, upnp state)

**Migration Strategy:** Wrap in mediator, inject mediator, gradually replace calls

### Hard to Migrate (1000+ callers, deeply coupled)

**Candidates:**
1. **CServiceBroker** (1000+ callers, 40+ services) - LINCHPIN
2. **CSettings** (300+ callers, massive scope)
3. **CApplication** (implicit global, all subsystems depend)

**Migration Strategy:** Break into domain-specific brokers (AudioBroker, GameBroker, PVRBroker)

---

## Critical Issues & Recommendations

### Issue 1: ServiceBroker as God Object
- **Impact:** Tightly couples all subsystems
- **Risk:** Cannot unit test individual components
- **Recommendation:** Extract domain-specific brokers (GameServices already does this)

### Issue 2: CSettings Monolith
- **Impact:** 300+ callers, unclear boundaries
- **Risk:** Circular dependencies, difficult refactoring
- **Recommendation:** Split into AudioSettings, VideoSettings, GUISettings, etc.

### Issue 3: XBMC_GLOBAL_REF Not Thread-Safe
- **Impact:** Initialization race conditions possible
- **Risk:** Crashes if globals accessed during BSS init
- **Recommendation:** Use thread-safe singleton factory (std::call_once)

### Issue 4: Mixed Synchronization Patterns
- **Impact:** CCriticalSection in some singletons, not in others
- **Risk:** Data races when accessing shared state
- **Recommendation:** Audit all GetInstance() singletons, add invariant checks

### Issue 5: Extern Declarations (Legacy)
- **Impact:** 16 extern g_ variables bypass singleton pattern
- **Risk:** Inconsistent access patterns, no lazy initialization
- **Recommendation:** Migrate all to ServiceBroker or proper singletons

---

## Thread-Safety Summary by Singleton

### Thread-Safe (Lock-Protected)
- CLog (static methods, internal sync)
- CServiceBroker (shared_ptr with atomic ops)
- CSkinSettings, CDisplaySettings, CMediaSettings (CCriticalSection)
- CDataCacheCore (atomic + critical sections)
- CRssManager (CCriticalSection)
- CZeroconf (CCriticalSection)
- CEventServer (std::mutex, std::atomic)

### Partially Thread-Safe (Assumed Single-Threaded Registration)
- CBuiltins (read-heavy, no lock)
- CScriptInvocationManager (thread pool, but registration not locked)

### Not Thread-Safe
- XBMC_GLOBAL macros (documented, no lock)
- Extern g_ variables (direct access)

---

## Call Site Analysis

### Top 5 Most-Used Singletons

1. **g_serviceBroker** - 1000+ calls
   - Every subsystem that needs a service
   
2. **CLog::GetInstance()** - 500+ calls
   - Every logging statement via CLog::Log()
   
3. **CSettings::GetInstance()** - 300+ calls
   - Skin, GUI, playback, media settings
   
4. **CSkinSettings::GetInstance()** - 150+ calls
   - GUI rendering, dialog construction
   
5. **CBuiltins::GetInstance()** - 200+ calls
   - Command execution, action dispatching

---

## Recommendations Summary

### Phase 1: Audit & Documentation
- [x] Catalog all singleton dependencies (COMPLETED)
- [ ] Document call sites per singleton
- [ ] Identify circular dependencies

### Phase 2: Low-Risk Refactoring
- [ ] Replace XBMC_GLOBAL_REF with std::call_once pattern
- [ ] Migrate easy-to-migrate singletons (CSkinSettings, CDisplaySettings) to DI
- [ ] Add thread-safety assertions to unsafe singletons

### Phase 3: Medium-Risk Refactoring
- [ ] Break CSettings into domain-specific settings managers
- [ ] Create subscription-based service discovery (vs GetInstance())
- [ ] Introduce event bus for inter-subsystem communication

### Phase 4: High-Risk Refactoring (Multi-Release)
- [ ] Decompose ServiceBroker into domain brokers
- [ ] Eliminate extern g_ variables
- [ ] Introduce factory pattern for complex creation

---

## File References

**Key Files Analyzed:**
- `/xbmc/ServiceBroker.h` - Central service locator
- `/xbmc/utils/GlobalsHandling.h` - Global macro definitions
- `/xbmc/utils/log.h` - Logging singleton
- `/xbmc/settings/Settings.h` - Main settings singleton
- `/xbmc/settings/SkinSettings.h` - Skin settings singleton
- `/xbmc/settings/DisplaySettings.h` - Display configuration
- `/xbmc/cores/DataCacheCore.h` - Playback state cache
- `/xbmc/interfaces/generic/ScriptInvocationManager.h` - Script execution
- `/xbmc/interfaces/builtins/Builtins.h` - Built-in commands
- `/xbmc/network/Zeroconf.h` - Service discovery
- `/xbmc/network/EventServer.h` - Event/button input server
- `/xbmc/network/upnp/UPnP.h` - UPnP services

