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
Version: 1.26
SHA-256: b2ce919cab2733b82132e938cd88bebf4af45d21802120e09fd23ec3953ebe01
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
   and calls `LauncherMain(argc, argv)` without `dlopen`. Hardware validation
   of the first launcher boundary is next.
5. **Engine initialization — pending.**
   Expand static registration to filesystem, input, material system, engine,
   client, server, physics, and UI modules.
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
