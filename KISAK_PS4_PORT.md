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
Version: 1.03
SHA-256: 0aa00bece121fb2fdd9dd8c2a3a0537679a7c289cafdb813444fe083279e68bb
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
