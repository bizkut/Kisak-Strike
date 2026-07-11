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

The build configures and begins compiling the Source libraries. It has not yet
produced `launcher_client.a` or a monolithic engine executable.

## Active compiler blockers

The current failure is in the platform compatibility layer, before launcher
logic:

1. `public/tier0/platform.h` uses `time_t` without a visible `<time.h>` include.
2. The same header uses `va_list` without a visible `<stdarg.h>` include.
3. `public/tier0/threadtools.h` selects a branch without a PS4/x86-64
   `ThreadInterlockedExchangeAdd64` implementation.
4. The build reaches its error limit while those declarations are included by
   appframework and tier2 sources.

These must be fixed behind `PLATFORM_PS4` or genuinely platform-neutral include
corrections. Do not define PS4 as Linux or PS3 to select an incompatible branch.

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
3. **Compile core Source libraries — in progress.**
   Fix PS4 time/varargs/atomics, then continue through tier0–tier3,
   vstdlib, appframework, and launcher compile errors.
4. **Direct monolithic launcher startup — pending.**
   Link the core archives, enable `KISAK_PS4_MONOLITHIC`, register factories,
   and enter `LauncherMain(argc, argv)` without `dlopen`.
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
