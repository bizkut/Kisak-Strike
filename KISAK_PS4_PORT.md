# Kisak-Strike PS4/OpenGNM Port

## Goal

Port Kisak-Strike to PS4 homebrew as a monolithic OpenOrbis executable. Keep
Source's D3D9-compatible material-system boundary and use the PS3 implementation
as the console architecture reference, while replacing every Cell, SPU, PRX,
RSX/GCM, PS3 audio, input, filesystem, and presentation dependency.

OpenGNM is the only graphics backend. Linux ToGL/OpenGL and PS3 shader binaries
are not part of the PS4 runtime. The first acceptance target is menu navigation
and a complete offline bot match.

## Current status — 2026-07-11

The OpenOrbis bootstrap is hardware validated. Title `KISK00001` starts on PS4,
creates `/data/kisak-strike/startup.log`, and remains stable in a one-second
kernel sleep loop. The first package returned from `main()` after writing all
markers; the PS4 shell reported that normal return as an application crash.
Commit `b8f56b31` fixed the behavior by keeping the bootstrap alive.

Latest validated bootstrap package:

```text
Package: IV0000-KISK00001_00-KISAKBOOTSTRAP00.pkg
Version: 1.01
SHA-256: 04e06501d0ce999355dfafdb9843fe6b876688d05ab1a1e9ed66605eb3dfbec8
Staged:  /data/pkg/IV0000-KISK00001_00-KISAKBOOTSTRAP00.pkg
```

Expected hardware log:

```text
kisak-ps4: bootstrap entered
kisak-ps4: bootstrap-only build
kisak-ps4: launcher not linked
kisak-ps4: bootstrap idle
```

## Completed work

### Orbis platform and build foundation

- Added an explicit `ORBIS`/`PLATFORM_PS4`/`PLATFORM_CONSOLE` platform branch.
- Enabled the initial static/monolithic application model.
- Added an LLVM 18 OpenOrbis CMake toolchain for `x86_64-ps4-elf`.
- Added native macOS build, FSELF conversion, package generation, validation,
  and FTP staging scripts.
- Linked the bootstrap with OpenOrbis `crt1.o`, `crti.o`, `crtn.o`, libc, and
  libkernel.
- Included `libc.prx` and `libSceFios2.prx` in the package.
- Verified `/data/kisak-strike/` is writable on hardware.

### Static Source module loading

- Added a fixed-capacity, allocation-free static module registry.
- Routed PS4 `CAppSystemGroup::LoadModule(name)` through the registry instead
  of `dlopen` or PS3 PRX loading.
- Added distinct tier0, vstdlib, and launcher factory symbols.
- All three factories delegate to the single tier1 interface registry used by
  the monolithic executable.
- Registration failures are deterministic and abort monolithic startup.
- Host tests cover normalization, `_client` aliases, duplicate registration,
  missing modules, factory identity, and interface lookup.

### Verified commands

```sh
./build-ps4-bootstrap.sh
./package-ps4-bootstrap.sh
./stage-ps4-bootstrap.sh

cmake -S tests -B /tmp/kisak-platform-tests
cmake --build /tmp/kisak-platform-tests
ctest --test-dir /tmp/kisak-platform-tests --output-on-failure
```

Both host tests currently pass.

## Current implementation attempt

The root CMake build now has an Orbis-only minimal graph containing:

```text
interfaces → tier0 → tier1 → tier2 → tier3 → vstdlib → appframework → launcher
```

Former shared-library targets become static archives on Orbis. Proprietary
Steam API linkage and Linux `dl`/`pthread` linkage are excluded. OpenOrbis has a
dedicated `/orbis` library output directory. The SDK's libc++ headers are added
explicitly because Clang did not discover `${OO_PS4_TOOLCHAIN}/include/c++/v1`
from the PS4 sysroot automatically.

The initial core archive set now compiles successfully for Orbis: tier0, tier1,
tier2, tier3, interfaces, vstdlib, appframework, and launcher. The build
produces `launcher_client.a`; it has not yet produced the monolithic engine
executable.

## Cleared compiler blockers

The core compile pass fixed the following PS4 compatibility gaps:

1. Added self-contained time and varargs declarations plus correct `va_copy`.
2. Added x86-64 `ThreadInterlockedExchangeAdd64` using the compiler atomic.
3. Enabled Source's generic thread-local wrappers for PS4.
4. Switched PS4 socket nonblocking setup from missing `FIONBIO` to `fcntl`.
5. Added PS4-safe `iswascii`, `qsort_s`, pthread yield, filesystem platform
   path, and launcher single-instance behavior.
6. Forced the legacy Source code to GNU C++11 instead of Clang's newer default.
7. Added a temporary PS4 `D3DFORMAT` declaration until the PS4 D3D façade owns
   the complete format definitions.

The active boundary is now link closure: combine the archives with the
bootstrap, libc++, libc++abi, OpenOrbis libraries, and the additional Source
subsystems referenced by `LauncherMain`.

The first link closure is now complete. `kisak_ps4_monolithic` combines the
bootstrap, all eight core archives, libc++, libc++abi, libc, and libkernel. The
PS4 build enables `cmpxchg16b` so Source's 128-bit lock-free list operation does
not require a missing `libatomic` runtime. Former DLL skeleton memory overrides
are omitted so tier0 remains the single allocator owner.

Latest monolithic diagnostic package:

```text
Package: IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg
Version: 1.64
SHA-256: 5f7c000082d4a9b9ddc1351d8685f885df83ecdd3c2e209d3819f4dcb625c2f8
Staged:  /data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg
```

The v1.00 hardware run reached bootstrap entry, static module registration, and
the call into `LauncherMain`, then crashed before any launcher-local marker.
Version 1.01 adds raw file breadcrumbs around launcher entry, logging-listener
registration, hardware-key verification, command-line creation, and base-dir
setup.

The v1.01 run stopped inside command-line creation. ELF inspection then proved
the legacy Clang PS4 target emitted constructors into `.ctors`, while the
OpenOrbis linker script only populated `.init_array`; the failing executable
had `DT_INIT_ARRAYSZ = 0`. Version 1.02 uses a generated derivative of the SDK
link script that retains `.ctors` inside `.init_array`. The rebuilt executable
has a 352-byte initialization array covering 44 constructor entries.

The v1.02 run crashed before `main()`, proving at least one restored constructor
is not yet PS4-safe. Version 1.03 moves the same 44 entries into a private
`.kisak_ctors` section and keeps the runtime initialization array empty. The
bootstrap walks the constructor table in legacy reverse order after opening the
log, writing before/after markers for every constructor index. This turns the
pre-main crash into a bounded constructor-level diagnostic.

The v1.03 trace completed constructor slots 43 and 42, then failed in slot 41,
`_GLOBAL__sub_I_tslist.cpp`. That translation unit contains tier0 stress-test
queues, lists, counters, and test operation objects rather than production
runtime state. Version 1.04 excludes `tslist.cpp` from the Orbis tier0 archive
instead of relaxing the failing lock-free queue invariant.

The v1.04 run completed the remaining 36 constructors after the removed TSList
slot, then failed at the new slot 6, `_GLOBAL__sub_I_jobthread.cpp`. That
initializer combines the required global thread pool with developer-only
`ThreadPoolTest` events and counters. Version 1.05 excludes the test namespace
and retains an inert `RunThreadPoolTests()` probe, leaving the production global
thread pool constructor in place for direct validation.

The v1.05 trace still failed in the reduced `jobthread.cpp` constructor, which
isolates the production `CGlobalThreadPool`. Version 1.06 gives the PS4 pool
aligned static storage and constructs it explicitly after the other constructor
entries, with independent before/after markers. The public `g_pThreadPool`
continues to reference the same singleton object once initialization completes.

The v1.06 trace completed all 42 retained constructor entries and reached
`before global thread pool`, then crashed inside explicit pool construction.
`CThreadPool` begins with a `CJobQueue` containing 16-byte-aligned lock-free
queue heads, but Source's legacy `DECL_ALIGN` macro expands to nothing for this
OpenOrbis Clang configuration. Version 1.07 explicitly aligns the singleton's
backing storage to 16 bytes so the queue-head invariant is satisfied before the
pool constructor runs.

The v1.07 trace still stopped before the explicit pool constructor returned.
The linked storage and first `CJobQueue` address are both 16-byte aligned, so
version 1.08 adds bounded breadcrumbs around each lock-free queue's dummy-node
allocation and after all `CThreadPool` members finish construction. This splits
Source allocator readiness from mutex/event construction without changing the
runtime path.

The v1.08 trace completed the dummy-node allocation for all four priority
queues and stopped immediately afterward, before the `CJobQueue` member set
finished. Version 1.09 traces the following `CThreadMutex` construction around
pthread attribute initialization, recursive-type selection, and mutex creation.

The v1.09 trace completed the queue's pthread mutex construction, including
attribute initialization, recursive type selection, and mutex creation. It
then stopped before the `CJobQueue` members completed. Version 1.10 traces the
following manual event across its mutex attribute, mutex, and condition-variable
initialization calls.

The v1.10 trace completed the manual event's mutex, attribute teardown, and
condition-variable initialization. It stopped before the next pool mutex.
Version 1.11 marks the completed `CJobQueue` body and the empty thread-vector's
debug-pointer setup, the only non-trivial boundaries remaining between them.

The v1.11 run again logged `thread pool event complete` but never returned to
the `CJobQueue` body. Disassembly shows the next operation is the constructor's
stack-canary check. OpenOrbis's pthread runtime overwrote the undersized local
`pthread_mutexattr_t`. Version 1.12 removes that local attribute on PS4 and
initializes the event's non-recursive private mutex with default attributes.

The v1.12 run validates the fix: all pool members completed, static modules
registered, and `LauncherMain` passed logging, hardware-key, command-line, and
base-directory setup. It then stopped after `after base dir`. Version 1.13
splits the immediately following default `-game csgo` append and console-option
queries.

The v1.13 trace completed the default game append and all console-option
queries. Version 1.14 brackets the following executable-name handling, Steam
overlay probe, spurious-parameter cleanup, and POSIX single-instance mutex.

The v1.14 trace completed every platform probe and acquired the single-instance
mutex. Version 1.15 splits the remaining launch flags and current-directory
setup, construction of the Source/Steam application groups, and the first
`CAppSystemGroup::Run()` handoff.

The v1.15 trace reached `before application run` after both application-group
objects constructed. Version 1.16 traces the shared app-system lifecycle across
creation, dependency loading, connection, pre-initialization, main, and
shutdown.

The v1.16 trace entered the outer app-system lifecycle and stopped inside
`CSteamApplication::Create()`. Version 1.17 splits filesystem-name selection,
static cvar factory loading/registration, and filesystem module
loading/registration.

The v1.17 trace stops in `FileSystem_GetFileSystemDLLName()` before it returns
a module name. That helper attempted POSIX executable-directory discovery even
though PS4 loads modules from the static registry. Version 1.18 returns the
normalized `filesystem_stdio` registry identifier directly on PS4.

The v1.18 trace returns the static filesystem name, registers cvar, and then
fails cleanly because `filesystem_stdio` is not yet part of the monolith.
Version 1.19 adds the existing VPK and stdio-filesystem targets to the Orbis
build/link and registers the filesystem module name with the monolithic
`CreateInterface` factory.

The v1.19 trace still receives no filesystem interface. Although the archive is
linked, no referenced symbol forced its interface-exposure object into the
executable. Version 1.20 adds a filesystem-specific factory anchor beside the
`EXPOSE_SINGLE_INTERFACE_GLOBALVAR` declarations and registers that anchor.

The v1.20 trace validates the anchor and advances through creation, dependency
loading, connection, and pre-init. It stops during system initialization after
constructing the filesystem async pool. Version 1.21 splits file-tracker thread
startup, async-pool construction, CPU topology selection, and pool startup.

The v1.21 trace passes CPU topology selection and stops inside the filesystem
pool's `Start()`. Version 1.22 traces capacity allocation, each `CJobThread`
construction, the underlying worker start, its idle-event handshake, and final
distribution.

The v1.22 trace constructs the first `CJobThread` and stops inside its native
`CThread::Start()`. That POSIX path uses a stack-local `pthread_attr_t`, the
same OpenOrbis ABI mismatch that corrupted event initialization. Version 1.23
uses default pthread attributes on PS4 while preserving Source's thread-init
handshake.

The v1.23 trace remains inside `CThread::Start()` before the outer call returns.
Version 1.24 splits its object-lock acquisition, create-complete event
construction, raw `pthread_create`, and initialization wait.

The v1.24 trace reaches the raw `pthread_create` call but the process terminates
before the parent returns from it. Since a simple tracker pthread already
starts successfully, version 1.25 traces the `CThread` child trampoline through
thread-ID allocation, current-thread TLS assignment, and virtual `Init()`.

The v1.25 trace proves `pthread_create`, child entry, thread-ID/TLS setup, and
virtual `Init()` all succeed. It stops when the child signals the
create-complete `CThreadEvent`. Version 1.26 replaces PS4 pthread-cond events
with atomic polling events, avoiding the OpenOrbis condition-object ABI mismatch
for creation and idle handshakes.

The v1.26 trace still stops immediately after child `Init()`. Version 1.27
splits the only remaining handshake operations: writing the parent's init
success flag and atomically signaling the create-complete event.

The v1.27 trace completes both success publication and the atomic create signal.
Version 1.28 traces the child transition into virtual `Run()` and the job
worker's idle-count/event publication, separating child execution from the
parent's create-wait return.

The v1.28 trace reaches run dispatch but stops before virtual `Run()` while
calling the desktop `Plat_IsInDebugSession()` probe. Version 1.29 bypasses that
probe on PS4 and directly enters the worker run method.

The v1.29 run starts all three filesystem workers, completes filesystem init,
and reaches the child app group. Its currently incomplete module set fails
creation cleanly; the later crash occurs during outer shutdown. Version 1.30
traces each system shutdown plus filesystem async flush, pool stop, and tracker
thread teardown.

The v1.30 trace stops in `AsyncFlush()` before pool shutdown. No asynchronous
requests exist during this failed startup, but the generic flush still enters
`AbortAll()` and suspends every worker. Version 1.31 returns immediately for an
empty PS4 async queue, preserving `AbortAll()` when jobs actually exist.

The v1.31 trace skips the empty flush and stops inside `CThreadPool::Stop()`.
Version 1.32 traces each worker exit call, native join, deletion, and final pool
cleanup.

The v1.32 trace stops inside the first worker exit call. `CallWorker()` attempts
desktop priority boosting and its reply wait calls `Plat_IsInDebugSession()`,
the probe already proven unsafe during startup. Version 1.33 disables priority
boosting for PS4 exit calls and treats reply waits as non-debug sessions.

The v1.33 trace completes worker exit delivery, all joins/deletions, filesystem
and tracker teardown, and the outer app-system shutdown. It then stops after
`steamApplication.Run()` returns. Version 1.34 traces result-stage handling,
reslist continuation, command-line override cleanup, and loop completion.

The v1.34 trace completes all explicit result, reslist, and override cleanup,
then stops while leaving the launcher iteration scope. Version 1.35 marks entry
to each `CAppSystemGroup` destructor to distinguish Steam-application member
cleanup from child Source-group cleanup.

The v1.35 trace enters both app-group destructor bodies before failing in their
automatic member-container teardown. Version 1.36 gives PS4 app groups explicit
process-lifetime storage. Their normal `OnShutdown()` still releases systems,
filesystem workers, and modules; only the empty legacy containers are retained
until process exit.

The v1.36 trace reaches `app groups retained` and completes the launcher loop.
The PS4 source mutex release is a no-op, so the remaining path is a normal
`LauncherMain` return followed by unsafe legacy global destruction. Version
1.37 records that return and keeps the bootstrap alive, deferring global
teardown until an explicit PS4 shutdown path exists.

The stable v1.37 run makes the child app's first missing contract explicit:
`engine` must provide `VCvarQuery001` before the rest of the module list is
examined. Version 1.38 adds a PS4 engine-bootstrap app system implementing that
real cvar-linkability contract and registers it as the transitional `engine`
factory. The full engine factory will replace it as renderer/audio/network
dependencies are brought into the monolith.

Version 1.39 adds the existing input-system and math libraries to the Orbis
monolith, anchors `InputSystemVersion001` in the static registry, and excludes
the desktop Steam Controller implementation and proprietary Steam API link.
DualShock 4 device sampling remains a later `libScePad` backend unit.

The v1.39 hardware run remained stable and completed the entire launcher,
filesystem worker-pool shutdown, application cleanup, and `LauncherMain`
return path without a crash. This validates the input-system checkpoint and
isolates the next application-group dependency to the physics/material-system
sequence.

Version 1.40 links Kisak's open IVP/Havana physics implementation and registers
the `kisakvphysics` factory in the static module table. The PS4 build carries
the existing IVP SDK/Havana feature configuration, adds the x86-64
little-endian IEEE helper path, exposes `alloca`, and enables the portable
Havok math wrappers without defining the whole target as Linux.

The v1.40 hardware run remained stable and again completed the launcher and
filesystem shutdown path through `LauncherMain` return. This validates that
the linked IVP/Havana constructors and physics factory do not regress the
current monolithic lifecycle.

Version 1.41 links the Source material-system core, bitmap, VTF, and shaderlib
archives and registers the `materialsystem` static factory. It introduces a
platform-neutral D3D9 format identity header for the PS4 content/material
boundary and supplies the missing GNU atomic pointer exchange used by material
containers. This checkpoint deliberately does not link ToGL or initialize a
desktop ShaderAPI; OpenGNM device integration remains the renderer milestone.

The v1.41 hardware run remained stable and completed the full launcher and
filesystem teardown sequence through `LauncherMain` return. This validates the
material-core constructors and static factory while renderer initialization is
still correctly deferred.

Version 1.42 links the real Source datacache module and registers its shared
factory. The PS4 anchor retains both interface translation units so
`VDataCache003`, `MDLCache004`, and `VStudioDataCache005` are all visible from
the monolithic module registry. Legacy studio byte-swap metadata is compiled
with the same 64-bit narrowing compatibility used by the other Source data
tables; PS4 continues to prefer little-endian Kisak/PC content.

The v1.42 hardware run remained stable and completed the launcher, all three
filesystem worker joins, application cleanup, and `LauncherMain` return. This
validates the datacache constructors and all three anchored interfaces in the
current monolithic lifecycle.

Version 1.43 links Source's studio-render core and registers its
`VStudioRender026` factory. The PS4 target uses the established legacy metadata
narrowing compatibility for this module. GPU work remains deferred: studio
render now participates in static interface discovery, but material rendering
will not initialize until the OpenGNM ShaderAPI/device boundary is available.

The v1.43 hardware run remained stable through the complete launcher and
filesystem shutdown lifecycle. This validates the studio-render constructors
and `VStudioRender026` factory without prematurely entering GPU initialization.

Version 1.44 links the Source sound-emitter base system and registers its
`VSoundEmitter003` factory. This checkpoint brings sound-script manifest and
parameter parsing into the monolith; actual playback remains deferred until the
PS4 `libSceAudioOut` device is connected at Source's existing mixing boundary.

The v1.44 hardware run remained stable through the complete launcher and
filesystem shutdown lifecycle. This validates the sound-emitter singleton and
`VSoundEmitter003` factory while audio-device initialization remains deferred.

Version 1.45 links the Source VScript manager, Squirrel VM, standard library,
and bindings, and registers the `VScriptManager009` factory. The Orbis target
uses Source's generic console contract to exclude desktop remote-debug sockets
and unbuilt Lua support. Its console buffer fast path now retains full 64-bit
addresses when checking float and double write alignment.

The v1.45 hardware run remained stable through the complete launcher and
filesystem shutdown lifecycle. This validates the Squirrel runtime constructors
and `VScriptManager009` factory without creating a guest script VM at startup.

Version 1.46 links the Source VGUI core and surface-support archives and
registers the `VGUI_ivgui008` factory. The generic input implementation supplies
the panel/input contracts without a desktop window system. A PS4 font boundary
provides deterministic metrics and transparent glyph output for lifecycle
validation; real font rasterization and UI drawing remain deferred to the
RocketUI/OpenGNM integration milestone.

The v1.46 hardware run regressed during the manual constructor walk. It stopped
at `before ctor 128`; the `.kisak_ctors` relocation for that slot resolves to
`_GLOBAL__sub_I_system_posix.cpp`. The `CSystem` singleton was allocating its
registry `KeyValues` before the remaining runtime constructors completed.

Version 1.47 defers that registry allocation until the first registry call and
adds bounded constructor breadcrumbs around path setup and deferral. The VGUI
interface set is unchanged; only its pre-runtime initialization order changes.

The v1.47 hardware run remained stable through the complete constructor walk,
launcher lifecycle, and filesystem shutdown. This validates the deferred VGUI
registry allocation and restores the v1.45 stability baseline.

Version 1.48 compiles and links the renderer-neutral VGUI controls archive. It
includes the standard panel, menu, dialog, list, text, and navigation controls
with the established Orbis compatibility for legacy DMX metadata. This archive
has no standalone `CreateInterface` factory and remains dormant until a later
client/UI module calls `VGui_InitInterfacesList`.

The v1.48 hardware run remained stable through the complete launcher and
filesystem shutdown lifecycle. This confirms the controls archive adds no
unexpected runtime constructors or initialization side effects.

Version 1.49 links the DMX loader and material-system VGUI surface, registers
the `VGUI_Surface031` factory, and preserves the Source-required ordering before
`vgui2`. PS4 cursor activation is a controller-oriented no-op, and OS custom
font registration reports unsupported until the real glyph backend exists.
Actual drawing remains deferred until the material system has an OpenGNM
ShaderAPI/device.

The v1.49 hardware run remained stable through the complete constructor walk,
launcher lifecycle, and filesystem shutdown. This validates the material VGUI
surface singleton and both of its static interfaces without entering drawing.

Version 1.50 extends the transitional PS4 `engine` factory with an inert
`VENGINE_LAUNCHER_API_VERSION004` app system. This satisfies the final ordered
launcher contract and adds bounded connect, init, startup-info, run, shutdown,
and disconnect breadcrumbs. It does not replace the real engine: the existing
engine archive still requires staged removal of Steam, desktop renderer/audio,
and monolithic dependency assumptions before promotion.

The v1.50 hardware run remained stable, but none of the new engine-launcher
breadcrumbs ran. The child group still returned from `Create()` before system
connection, proving an earlier ordered `AddSystems` lookup remains unresolved.

Version 1.51 logs each requested static module and interface plus load, success,
and failure boundaries inside `CAppSystemGroup::AddSystems`. This diagnostic
checkpoint leaves subsystem behavior unchanged and will identify the exact
contract blocking child creation on the next hardware run.

The v1.51 hardware run remained stable and identified the blocker precisely.
`engine / VCvarQuery001` succeeded, then
`filesystem_stdio / QueuedLoaderVersion001` loaded the module but failed its
interface lookup.

Version 1.52 anchors the existing queued-loader translation unit through the
filesystem factory. This retains `CQueuedLoader` and its
`QueuedLoaderVersion001` exposure in the monolithic executable while preserving
the existing `filesystem_stdio` registry identifier and implementation.

The v1.52 hardware run remained stable. The ordered child-app interface trace
successfully resolved the engine cvar query, queued loader, input system,
physics, material system, all three data-cache interfaces, studio renderer,
sound emitter, VScript, VGUI material surface, VGUI2, and the engine launcher
API. Child creation then stopped at `rocketui / RocketUI001`.

Version 1.53 registers a transitional PS4 `RocketUI001` app system. Its
lifecycle and interface contract are available to the monolithic launcher, but
rendering, document loading, and input remain deliberately inert until the
RocketUI renderer is connected to OpenGNM. This advances startup without
linking RocketUI's existing POSIX OpenGL/ToGL implementation.

The v1.53 hardware run remained stable and resolved `rocketui / RocketUI001`.
All ordered child systems were created successfully, after which startup
returned during `LoadDependentSystems` before the connection stage. The app
then performed a complete orderly filesystem/thread-pool shutdown and returned
from `LauncherMain`; the engine launcher's `Connect`, `Init`, and `Run`
breadcrumbs did not execute.

Version 1.54 adds bounded dependency-owner, module, interface, factory-hit,
module-load, add-system, and ordering breadcrumbs to `LoadDependentSystems`.
This diagnostic checkpoint will identify the exact transitive app-system
contract preventing the child group from reaching `ConnectSystems`.

The v1.54 hardware run remained stable and identified the first transitive
dependency: `VGUI_Surface031` requires `localize / Localize_001`. The static
registry had no `localize` module, so dependency loading failed before system
connection and the app again shut down cleanly.

Version 1.55 adds the maintained `localize/localize.cpp` implementation to the
Orbis build, retains its factory in the monolithic link, and registers the
`localize` module. This supplies the current `ILocalize` ABI and real Source
token lookup, conversion, formatting, callback, and localization-file behavior;
the obsolete VGUI2 localized-string-table implementation remains excluded.

The v1.55 hardware run remained stable. `Localize_001` was already satisfied,
and dependency scanning advanced to `InputStackSystemVersion001` from the
existing `inputsystem` module. Its implementation was compiled into the static
archive but its unreferenced translation unit was not retained in the
monolithic executable.

Version 1.56 anchors the existing `CInputStackSystem` translation unit through
the PS4 input-system factory. This exposes the real input-context stack under
the existing module identity without duplicating registration or introducing
a placeholder implementation.

The v1.56 hardware run regressed to a crash, but proved the retained input-stack
contract itself resolves correctly. Dependency scanning satisfied localization,
input stack, material system, and VGUI dependencies, completed ordering, and
reached `ConnectSystems`. The log stopped immediately after `app before
connect`, before the first connection result, isolating the failure to an
app-system `Connect` call rather than static construction or dependency lookup.

Version 1.57 adds the interface name plus before-call, false-return, and
successful-return breadcrumbs around every `ConnectSystems` invocation. This
diagnostic build leaves connection order and subsystem behavior unchanged and
will identify the exact system whose connection path crashes.

The v1.57 hardware run crashed again and isolated the connection failure to
`VMaterialSystem080`. Cvar query, queued loader, input system, and physics all
connected successfully; the log stopped after `VMaterialSystem080` entered its
`Connect` call.

The material-system constructor initialized the shader module handle but left
`m_ShaderAPIFactory` indeterminate. Normal desktop sequencing calls
`SetShaderAPI` before connection, while the transitional monolithic child group
currently connects the material system directly. Version 1.58 initializes the
factory to null, checks it before any ShaderAPI interface lookup, and logs base
connection, filesystem, and shader-factory boundaries. A missing OpenGNM
ShaderAPI now produces a controlled startup failure and clean shutdown instead
of calling an undefined function pointer; renderer connection remains blocked
until the PS4 ShaderAPI factory is installed.

The v1.58 hardware run remained stable and followed the intended controlled
path: material-system base connection and filesystem discovery succeeded,
`materialsystem shader factory missing` was logged, connection returned false,
and the complete filesystem/thread-pool shutdown finished cleanly.

No PS4/OpenGNM ShaderAPI implementation exists in the Kisak tree yet. Version
1.59 therefore adds `shaderapiempty` to the Orbis static build as an explicitly
temporary lifecycle scaffold, registers its module/factory before the material
system, and installs it during PS4 material-system factory setup. It supplies
Source's ShaderAPI, device-manager, device, shadow, and hardware-config
interfaces but performs no rendering. The monolithic build shares the single
material-system-owned `g_pShaderUtil` global instead of retaining the original
DLL-local duplicate. This checkpoint is only for advancing initialization and
diagnostics; OpenGNM remains the sole intended PS4 graphics backend and must
replace the empty factory before rendering acceptance.

The v1.59 hardware run remained stable but still logged `materialsystem shader
factory missing`. The scaffold was present in the executable, while
`CMaterialSystem::CreateShaderAPI` continued to use `Sys_LoadModule`, bypassing
the static module registry used by the monolithic app-system loader.

Version 1.60 routes PS4 ShaderAPI creation through
`FindStaticModuleFactory`. Other platforms retain dynamic module loading. This
allows the temporary empty ShaderAPI to connect now and provides the same
factory-selection hook that the future OpenGNM ShaderAPI will use.

The v1.60 hardware run remained stable and confirmed the static loader fix:
`materialsystem shader factory ready` appeared. Material-system connection then
returned false before exposing which ShaderAPI interface lookup or final device
manager connection failed.

Version 1.61 adds bounded result markers around device-manager, hardware-config,
ShaderAPI, shader-device, shader-shadow, and RocketUI lookups plus the final
device-manager `Connect` call. It changes no renderer behavior and will identify
the exact interface contract the temporary scaffold still lacks.

The v1.61 hardware run remained stable and isolated the missing contract:
`ShaderDeviceMgr001` resolved, while
`MaterialSystemHardwareConfig013` did not. The empty backend implements the
hardware-config interface, but its PS4 module factory returned the process-wide
`CreateInterface` chain rather than a deterministic ShaderAPI-local mapping.

Version 1.62 gives the temporary ShaderAPI module a local factory that maps its
device manager, hardware config, ShaderAPI, shader device, and shader shadow
objects explicitly. This avoids monolithic global-factory ambiguity and
establishes the factory shape the OpenGNM backend will implement.

The v1.62 hardware run remained stable and completed all temporary ShaderAPI
lookups plus device-manager connection. Data cache, model cache, studio data
cache, studio renderer, sound emitter, and VScript also connected. Startup then
stopped when `VGUI_Surface031::Connect` returned false.

The VGUI2 static archive retained `vgui.cpp` for `VGUI_ivgui008`, while its
panel, internal-input, and POSIX-system interface registrations lived in
otherwise unreferenced translation units. Version 1.63 anchors those existing
real implementations through `KisakVGuiFactory`, preserving normal VGUI
dependency acquisition without adding interface stubs.

The v1.63 hardware run remained stable and advanced through every app-system
connection, including `VGUI_Surface031`, engine launcher, and RocketUI. Startup
then stopped in `CSourceAppSystemGroup::PreInit` after the `app before preinit`
marker. The desktop Steam environment/mount path was still being used even
though PS4 has no Steam runtime.

Version 1.64 adds a PS4-specific pre-init branch that skips Steam setup and
authentication, retains the Source tier-library registration, and applies the
platform filesystem search-path setup for the `/app0` and `/data` roots.

Reproduce the current cross-build with:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
export LLVM18_PREFIX=/opt/homebrew/opt/llvm@18

cmake -S . -B build-ps4-engine \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/OpenOrbis.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ps4-engine --target launcher_client --parallel 4
```

## Delivery plan and progress

1. **OpenOrbis bootstrap and logging — complete.**
   Hardware boot, writable data path, stable process, package validation.
2. **Static module registry — complete.**
   Name-to-factory lookup integrated into `CAppSystemGroup`.
3. **Compile core Source libraries — complete.**
   tier0–tier3, interfaces, vstdlib, appframework, and launcher archives build
   for `x86_64-ps4-elf`.
4. **Direct monolithic launcher startup — in progress.**
   The executable links, enables `KISAK_PS4_MONOLITHIC`, registers factories,
   and calls `LauncherMain(argc, argv)` without `dlopen`. The complete launcher
   and filesystem shutdown lifecycle is hardware validated at each checkpoint.
5. **Engine initialization — in progress.**
   Filesystem, input, Kisak physics, material-system core, all datacache
   interfaces, studio-render core, sound-emitter base, Squirrel VScript, and the
   VGUI core and material surface are linked and statically registered, and the
   VGUI controls archive compiles for Orbis. RocketUI integration and full
   engine/client/server modules remain; renderer and AudioOut device backends
   are still pending.
6. **Content filesystem — pending.**
   Layer `/app0` and `/data/kisak-strike` VPK/search paths using little-endian
   Kisak/PC content.
7. **OpenGNM renderer — pending.**
   Implement the PS4 D3D9 façade, memory pools, dirty-state PM4 emission,
   resource lifetime, EOP synchronization, and VideoOut presentation.
8. **Offline shader pipeline — pending.**
   Expand Source shader combinations, compile HLSL to SPIR-V, compile SPIR-V
   to `.sb` with `opengnm-psbc`, and generate strict binding manifests.
9. **PS4 services — pending.**
   Pad, AudioOut, sockets, timing, threads, events, virtual memory, assertions,
   and bounded diagnostics.
10. **Acceptance — pending.**
    RocketUI menu, BSP/world rendering, models/effects, audio/input, offline bot
    match, clean shutdown, 30-minute stability, and base-PS4 30 FPS gate.

## Multiplayer follow-on

The tracked companion plan [KISAK_PS4_MULTIPLAYER_PLAN.md](KISAK_PS4_MULTIPLAYER_PLAN.md)
governs networking work after the offline acceptance gate. Its sequencing is
part of this port plan: preserve Source UDP and `CNetChan`, validate OpenOrbis
BSD-socket loopback, then add LAN play, public dedicated/community servers, and
only afterward player-hosted Internet sessions through an ICE transport
adapter. Cloudflare STUN/TURN is an optional late fallback for NAT traversal;
it must not become a dependency for startup, offline play, LAN, or dedicated
servers.

PS4 builds remain Steam-free. Do not emulate `ISteamNetworking` or retain Steam
lobby assumptions. Any required online session behavior belongs behind narrow
PS4 datagram/session interfaces, with signaling and short-lived TURN
credentials supplied by a separate trusted service. The immediate monolithic
bring-up and offline bot-match milestones remain unchanged.

## Commit history

```text
81a97d33 ps4: establish monolithic module loading foundation
936887b4 ps4: build a bootable OpenOrbis bootstrap
82ba02a8 ps4: package and stage the bootstrap
b8f56b31 ps4: keep the bootstrap alive after startup
264849ed ps4: add unique static Source module factories
```

## Architectural constraints

- Do not reuse PS3 SDK code, PRXs, SPU objects, Cg/RSX binaries, or proprietary
  Sony components.
- Do not route PS4 rendering through ToGL/OpenGL.
- Preserve the Source app-system lifecycle and interface-version checks.
- Use `PLATFORM_PS4` for PS4 behavior and narrow `PLATFORM_CONSOLE` branches for
  behavior genuinely shared with consoles.
- Keep Source-specific D3D state and caching inside Kisak; extend OpenGNM only
  for generic missing PS4 ABI/PM4/resource operations.
- Users provide legally obtained game content; proprietary assets are not
  committed or redistributed.
- Commit generated build products, PKGs, FSELF files, runtime PRXs, and copied
  package assets only as external artifacts, never as source-controlled files.
