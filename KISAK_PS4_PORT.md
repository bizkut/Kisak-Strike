# Kisak-Strike PS4/OpenGNM Port

This is the active execution plan. The complete v1-v4 implementation and
hardware-test diary is preserved in
[KISAK_PS4_PORT_HISTORY.md](KISAK_PS4_PORT_HISTORY.md). UI-specific evidence is
in [KISAK_PS3_UI_CROSSREFERENCE.md](KISAK_PS3_UI_CROSSREFERENCE.md).

## Goal and source authority

Ship Kisak-Strike as one OpenOrbis executable using OpenGNM, with menu
navigation and a complete offline bot match as the first acceptance target.
The production target is the real Source engine/client/server lifecycle, not
the diagnostic presentation loop.

The PS4 runtime is authoritative. PC, Xbox 360, PS3, Steam, desktop, and
dynamic-module branches are implementation history, not blockers. Preserve
required behavior and ordering when they encode a real engine contract, but
replace either with a clearer or safer PS4-native implementation when useful.
Keep interface-version checks that express real compatibility; do not preserve
DLL/PRX ownership assumptions merely because the original code used them.

## Current hardware checkpoint

| Item | Value |
|---|---|
| Test | v4.83, 2026-07-16 |
| Package version | 3.49 |
| Package | `IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Package size | 103,284,736 bytes |
| Package SHA-256 | `8d8210b33b18a7eb3c1ea3cfb6372c3f43ac1d743d4f960c7b0f072bb7114be6` |
| FTP staging path | `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Candidate commit | `d0bb9c97` (`Instrument PS4 particle manager startup`) |
| Hardware-result commit | `a724a805` (`Record v4.83 particle parse boundary`) |

v4.83 closes the broad ParticleMgr boundary. The function-local manager was
already constructed during the ordinary constructor walk. Client cleanup,
precache-policy evaluation, shared `CParticleSystemMgr::Init`, and both builtin
operator registrations all complete.

The cumulative log is 55,987,679 bytes and 1,147,872 lines with SHA-256
`f86aa25d4ab0c240f75220aa18c637b3c4ab5542c2222c4ab53975268e9bbca2`.
The complete v4.82 log is an exact byte prefix. The v4.83 append is 3,086,354
bytes and 63,579 lines with SHA-256
`0607d73b3d0a0b6ed34e16ec603d727f3cf3e54ab0a6e1b07f4eb4d6c18c4725`.

The last seven markers are:

```text
particle mgr before core init
particle mgr core init ready
particle mgr before simulation operators
particle mgr simulation operators ready
particle mgr before rendering operators
particle mgr rendering operators ready
particle mgr before effect parse
```

The final marker is immediately before `ParseParticleEffects( true )` in
`game/client/particlemgr.cpp`; its completion marker is absent. The client
parse's existing detailed PS4 probe is intentionally disabled when sheets are
loaded, so this result does not yet prove entry into the function body or which
manifest/config/decommit step failed. Earlier in the same process, the server
parse completed the same 30-entry manifest and reached 1,062 definitions. The
client sets render-sheet loading true, so the same forced-precache `!` entries
enter sheet/material work that the server bypassed.

### v4.83 result and immediate v4.84 gate

Package version 3.49 and build marker `particle_mgr_boundary_v483` identify the
completed manual-install test. The serial client path records completion of
`COM_TimestampedLog`, evaluates `ParticleMgr()` separately, and fails before
downstream game systems if the accessor or `CParticleMgr::Init` fails.
`CParticleMgr` records constructor entry/completion and bounded boundaries
around `Term`, particle precache policy, shared-manager Init, builtin operator
registration, and `ParseParticleEffects( true )`. It validates all required
pointers and now propagates the previously ignored
`CParticleSystemMgr::Init` failure.

The Linux OpenOrbis monolithic build completes. The OELF is 136,086,560 bytes
with SHA-256
`70adf01cf7f4a4a7825d4950fcbe65b35f37174dd9ef06d835f4be0d31e5bdff`;
the SELF input is 83,192,528 bytes with SHA-256
`b717624775407d3e1e41dbeb7a35340a6d9b5bb483a8bef702a73529bcd8ea07`.
Eleven of fourteen host tests pass. The unchanged three OpenGNM descriptor
tests use Linux stack addresses above the PS4 descriptor's 44-bit address
range and fail their pointer round-trip checks; this pre-existing host-fixture
defect does not exercise the changed client/particle code.

Candidate commit `d0bb9c97` produces a 103,284,736-byte package with SHA-256
`8d8210b33b18a7eb3c1ea3cfb6372c3f43ac1d743d4f960c7b0f072bb7114be6`.
Verbose PkgTool validation reports every limit, digest, and signature check
`[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete FTP
readbacks match the local size and SHA-256.

Static and final-OELF inspection also expose the next ownership question: the
monolith has one `g_pParticleSystemMgr` but two `g_pParticleSystemQuery`
objects. Server startup initializes the shared manager first, and its Init
only adopts a non-default query once. v4.84 must first enable the already
bounded detailed parse probe for the client and distinguish sheet policy,
manifest loading, each config/definition, forced precache, and final decommit.
Do not attribute this crash to query ownership until a query-dependent call is
actually reached.

### v4.84 candidate instrumentation

Package version 3.50 and build marker `particle_client_parse_v484` identify the
next manual-install candidate. The client now enables the existing particle
parse probe before the first body operation. Separate markers distinguish
function entry, `MEM_ALLOC_CREDIT`, sheet-policy setup, manifest loading, each
config, and final temp-memory decommit.

Detailed markers reset for every config, remain capped at 256 entries, and emit
one explicit `detail limit reached` marker when the cap is exhausted. Forced
precache now brackets material initialization, proxy handling, sheet loading,
material modulation, fallback recursion, renderer precache, and child
precache. `FindOrLoadSheet` separately brackets its sheet-policy/cache checks,
material and `$basetexture` lookup, texture/error checks, sheet resource data,
`CSheet` construction, and definition-cache update. These changes do not alter
shared particle-query ownership, manifest order, forced-precache policy, or
sheet-loading behavior; v4.84 is an attribution candidate, not a speculative
particle fix.

The Linux OpenOrbis monolithic build completes. The OELF is 136,086,560 bytes
with SHA-256
`07d8cac5afc51e696cf38a000574080bcc72793e8a4b2edf6b1add2db0adf930`;
the SELF input is 83,192,528 bytes with SHA-256
`8b007ed0dca06a0db7565f683e33e72f98d50caa4460ef63b5f685965c1d1ea9`.
Eleven of fourteen host tests pass. The same three OpenGNM descriptor tests
fail only because their Linux stack pointers exceed the PS4 descriptor's
44-bit address field; they do not exercise this particle-parser change.

The gate remains open until hardware proves `ClientDLL_Init` completion,
successful EngineVGui/GameUI hookup, and one complete production
`eng->Frame()`.

## Runtime topology: two tracks, one production authority

`KisakRegisterStaticModules` registers both tracks in
`appframework/Ps4StaticModules.cpp:54-83`:

| Registered runtime | Factory | Purpose | Acceptance status |
|---|---|---|---|
| `presentation_engine` | `KisakEngineBootstrapFactory()` | Diagnostic OpenGNM/Scaleform/input loop and rollback target | Hardware validated, but not a production Source-frame result |
| `engine` and `source_engine` | `KisakSourceEngineFactory()` | Real Source app systems, server, client, host, and frame lifecycle | Hardware reaches client ParticleMgr initialization; no first frame yet |

The launcher requests production `engine` (`launcher/launcher.cpp:773-820`).
The presentation loop owns VideoOut and calls `RenderMenuFrame` and
`RenderHUDFrame` directly (`ps4/engine_bootstrap.cpp:40-54,137-207`). The
production loop consumes `eng->Frame()` in
`engine/sys_dll2.cpp:1200-1259`. A presentation feature is not complete until
the production loop reaches and renders it on hardware.

The production wiring is intentionally partial. The launcher selects
`shaderapips4` before app-system connection (`launcher/launcher.cpp:921-940`),
but that wrapper still delegates many interfaces and `Present()` to
`shaderapiempty`; the native VideoOut/GNM runtime remains presentation-owned.
PS4 also forces both `USE_SCALEFORM` and `USE_ROCKETUI` off
(`CMakeLists.txt:166-171`). The normal Source `IScaleformUI` queries and
`_Host_RunFrame` advance are therefore compiled out, while the diagnostic path
uses the separate `Ps4ScaleformUI001` bootstrap interface. DualShock is the one
presentation-era capability already wired into the production input pump.

Use these terms consistently:

- **Implemented**: code and focused host/static validation exist.
- **Source-integrated**: the production Source lifecycle has the required call
  path and ownership, whether or not hardware has reached it.
- **Hardware-accepted**: the production path completed its explicit gate on a
  PS4. Presentation-only evidence is labelled separately.

## Capability and acceptance matrix

| Capability | Implemented | Source-integrated | Hardware-accepted | Next production proof |
|---|---|---|---|---|
| External content and layered filesystem | Yes | Yes | Yes | Preserve while loading `cs_office` |
| OpenGNM VideoOut, flips, depth, textures, indexed draws, constants | Yes | No native Source frame/present yet | Presentation only | Source owns begin/draw/present for one frame |
| PS4 ShaderAPI buffers, mesh locks, bindings, draw state | Partial | Wrapper selected; individual paths still delegate to empty | Presentation/host tests only | A Source material emits and presents a native draw |
| Source world/model materials | Existing Source render paths; PS4 shader coverage incomplete | Not end-to-end | No | Present a `LightmappedGeneric` world surface and `VertexLitGeneric` model in one `cs_office` frame |
| Source particle rendering | `.pcf` parser, simulation operators, and dynamic-mesh generation exist; PS4 particle shaders are missing | Loading path reached; native render path incomplete | No | Load sheets and present one translucent `SpriteCard` particle effect from a Source frame |
| Scaleform Legals -> StartScreen -> MainMenu, movies, vector/image/text rendering | Yes | No: Source Scaleform is disabled and the PS4 bootstrap uses a separate interface | Presentation only | Same boot/menu sequence from Source frames |
| DualShock polling and Source `InputEvent_t` translation | Yes | Yes in code | Presentation UI only | Navigate Source-owned menu and drive a user command |
| Main-menu action routing | Partial: 1 of about 20 | Diagnostic only | Offline-dialog action in presentation | Complete actions needed for boot, options, offline launch, pause, and quit |
| Source server/client startup | Yes, still being closed | Yes | Through client ParticleMgr boundary | Client Init + EngineVGui + first `eng->Frame()` |
| Offline request parser and queue | Yes | Split: diagnostic menu produces it; the production MainLoop consumer is compiled but not reached | Presentation producer only | Add a Source-owned producer, then hardware-prove the consumer through `Host_NewGame` |
| Listen server, local client, spawn, input, world frame | Existing Source path, not PS4 accepted | Not end-to-end | No | One controllable player and one presented `cs_office` frame |
| Audio | Existing engine path, PS4 completion pending | Partial | No combined acceptance | Audible offline frame without destabilizing render/input |

The UI movie, ActionScript, and controller-navigation layers are already
implemented. The remaining UI work is primarily C++ action routing, plus
secondary motion-controller gating and the pause/buy Scaleform-vs-RocketUI
choice (`KISAK_PS3_UI_CROSSREFERENCE.md:203-221`). Do not put renderer,
Scaleform boot, or DualShock back on the plan as greenfield tasks.

## Active critical path

### 1. Monolithic composition and lifecycle closure

The monolith does not currently preserve DLL-like ownership. Client, server,
and engine factories ultimately return `Sys_GetFactoryThis()`
(`engine/sys_dll2.cpp:541-548`, `game/client/cdll_client_int.cpp:1080-1092`,
`game/server/gameinterface.cpp:728-735`). `CreateInterfaceInternal` searches one
process-wide prepend-only `InterfaceReg` list and returns the first matching
name (`tier1/interface.cpp:56-98`). Constructor and link order therefore decide
which duplicate owns a version.

This is already an observed architecture problem:

- `render_to_rt_helper.cpp` was compiled but discarded until v4.82 added an
  ad-hoc client-factory reference.
- Server lifecycle bodies, `sv_cheats`, `sv_maxreplay`, and protobuf
  registrations have previously collided across client/server composition.
- The current final OELF contains 66 copies of the
  `CServerGameTags` registrar because exposure is declared in
  `game/server/gameinterface.h:266-272`.
- The link still permits arbitrary first-definition wins with
  `--allow-multiple-definition` (`CMakeLists.txt:365-370`).

Replace incidental anchors with an explicit PS4 monolithic composition model:

1. Add module/source ownership metadata to `InterfaceReg` on PS4. Registrar
   macros should record the source and a compile-time module owner.
2. Expose registry enumeration, cardinality checks, and module-scoped lookup.
   Make each `Kisak*Factory` return only interfaces owned or explicitly shared
   by that logical module.
3. Add a manifest with rows for module, interface version, expected owner,
   exact/allowed cardinality, startup phase, preflight safety, required vs
   optional status, provider translation unit, and a stable retention token.
4. Give every registrar-only translation unit one external retention token and
   reference tokens from the manifest assembly function. Remove the old
   `KisakGameClientFactory` anchors after manifest coverage is proven.
5. Move `CServerGameTags` exposure into one `.cpp` immediately; never expose a
   registrar from a multiply included header.
6. Add a post-link ownership audit over actual link inputs, isolated server
   objects, and the final OELF. Check registrar providers, lifecycle bodies,
   exported globals, ConVars/ConCommands, protobuf descriptors, constructor
   sections, and manifest retention tokens.
7. Treat the server-localized overlap set as a reviewed allowlist and fail on
   drift. Keep the seven source-identical protobuf reuses, but do not
   canonicalize arbitrary client/server objects whose compile definitions can
   change semantics.
8. Remove global `--allow-multiple-definition` after ownership is explicit, or
   temporarily replace it with a checked, shrinking allowlist.

Run preflight in stages:

- after constructors: verify retention, owners, and raw registry cardinality;
- before app-system Connect: validate every mandatory manifest row, but
  instantiate/query only interfaces marked preflight-safe; defer all others to
  their declared lifecycle phase;
- after cvar registration: reject duplicate names with incompatible type,
  flags, defaults, or owner;
- before client Init: preflight all `CHLClient` requirements;
- after client Init: preflight post-client and EngineVGui/GameUI requirements.

Preflight must emit one bounded report and stop before partial initialization.
It complements the post-link audit; a successful first-match lookup alone does
not prove uniqueness.

Make lifecycle results explicit. A failed Load, Connect, or Init must return a
failure through every caller, leave its initialized flag false, unwind only
completed stages, and prevent all later callbacks. Do not rely on `Sys_Error`
or `Plat_ExitProcess` terminating on PS4. Priority repairs include
`ClientDLL_Load`, void/ignored `ClientDLL_Connect`, void
`CEngineVGui::Connect`, and void `SV_InitGameDLL`.

Track the successfully connected and initialized extent in `CAppSystemGroup`.
`ConnectSystems` stops at the first false result, but `OnStartup` leaves the
group at the connection stage and `OnShutdown` then skips disconnection, so the
successful connection prefix is never disconnected. `InitSystems` already
unwinds its successfully initialized prefix correctly
(`appframework/AppSystemGroup.cpp:667-727,886-994`). Preserve that Init unwind,
add exact connected-prefix cleanup, and make both paths idempotent without
double shutdown.

Host/static gates for this phase:

- extend `ps4_static_modules_test` so an interface cannot leak through a
  foreign module factory;
- add a tiny client/server manifest test with duplicate versions and
  `--gc-sections`, proving retention, scoped ownership, and cardinality;
- fail the build on a missing required provider or an unexpected duplicate;
- prove with a lifecycle-failure test that no downstream callback runs after
  any failed Load/Connect/Init.

Hardware gate: preflight passes, the ParticleMgr boundary is closed,
`ClientDLL_Init` completes, EngineVGui/GameUI connect, and the first real Source
frame completes. This phase is not done merely because a registrar is found.

### 2. Presentation-to-Source convergence

Keep `presentation_engine` as a diagnostic rollback target, not a second
product runtime. Move the validated pieces into the production lifecycle:

1. Complete the already selected `shaderapips4` wrapper so the PS4
   ShaderAPI/device owns OpenGNM frame begin, command emission, synchronization,
   VideoOut submission, and present from Source. Its current
   `CShaderDevicePs4::Present()` delegates to `shaderapiempty`
   (`materialsystem/ps4gnm/shaderapips4.cpp:121-150`), while real VideoOut
   submission is presentation-owned.
2. Add a production Scaleform bridge deliberately. Either finish the normal
   `ScaleformUI002` PS4 app-system/HAL and enable `INCLUDE_SCALEFORM`, or adapt
   the focused `Ps4ScaleformUI001` manager to Source-owned UI/render phases.
   The guarded `_Host_RunFrame` call site exists
   (`engine/host.cpp:4522-4534`) but is not part of the current PS4 build.
3. Preserve existing Source DualShock polling and GameUI dispatch
   (`engine/sys_dll2.cpp:1077-1084`, `engine/sys_mainwind.cpp:403-506`). Do not
   reimplement input; prove it drives UI and gameplay from production frames.
4. Record each capability independently as implemented, Source-integrated, and
   hardware-accepted.

#### Source/OpenGNM rendering closure checklist

Complete these gates in the order the first production `cs_office` frame
actually requires them. Track each gate independently; do not treat a working
diagnostic or fallback draw as proof that the corresponding Source path is
integrated.

1. **Frame ownership:** move OpenGNM command-buffer begin/end, synchronization,
   VideoOut submission, and present into the production ShaderAPI/device
   lifecycle. Required Source draws must not fall through to
   `shaderapiempty`.
2. **Mesh submission:** close native static and dynamic vertex/index-buffer
   creation, lock/unlock, vertex declarations, primitive conversion, indexed
   and non-indexed draws, and per-frame buffer lifetime. Prove one static world
   mesh and one dynamic mesh in the same presented frame.
3. **Textures and targets:** support the VTF formats encountered by the first
   map, texture upload and layout, sampler state, sRGB policy, render targets,
   depth targets, resolves, and safe missing-resource fallbacks.
4. **Constants and material state:** map Source matrices, vertex/pixel
   constants, material variables, blend/depth/cull/color-write state, and
   texture bindings to native OpenGNM state with deterministic defaults.
5. **World and model shaders:** add `LightmappedGeneric`,
   `VertexLitGeneric`, and `UnlitGeneric` PS4 shader combinations first, then
   only the additional static/dynamic combinations observed while loading and
   rendering `cs_office`. Include required lightmaps, vertex lighting, fog,
   model skinning, alpha test, and translucent variants.
6. **Particles:** keep Source responsible for `.pcf` parsing and simulation;
   add PS4 `SpriteCard`, `ParticleLitGeneric`, and any observed particle
   material variants, including VTF sheet data, dynamic quad/rope meshes,
   constants, samplers, depth policy, blending, and translucent ordering.
7. **Scene effects:** close the first-map decal, overlay, projected-texture,
   and translucent-sort paths only when observed. Use explicit PS4-native
   substitutes or bounded fallbacks when the desktop effect is unnecessary for
   the first playable match.
8. **Acceptance:** record shader/material families separately as implemented,
   Source-integrated, and hardware-accepted. The minimum renderer gate is one
   presented `cs_office` frame containing world geometry, a model, a dynamic
   draw, and a translucent particle without required-path delegation to the
   empty backend.

Acceptance: production boot shows the authentic Scaleform sequence, reaches
MainMenu, accepts DualShock navigation, and presents repeated Source-owned
frames without invoking the presentation loop.

### 3. Offline-match skeleton

Reuse the existing narrow path rather than designing another launcher:

- Scaleform `OnOk` submits the parsed request
  (`ps4/scaleform_gfx_manager.cpp:958-976`), but this producer currently runs
  only in `presentation_engine`.
- Production `CEngineAPI::MainLoop` accepts
  `classic/casual/mg_cs_office` and queues
  `game_type 0; game_mode 0; bot_difficulty N; map cs_office`
  (`engine/sys_dll2.cpp:1208-1236`).

Those halves are mutually exclusive in the current runtime: the diagnostic
loop observes but never consumes the pending request, while production has no
menu producer. Use a temporary deterministic, Source-owned one-shot request or
startup command to validate gameplay independently; replace it with the
Source-integrated menu producer before UI/offline acceptance.

Instrument and accept the remaining path in this order:

1. queued command executes through `Cbuf_Execute`, then `map cs_office`
   reaches `Host_Map_Helper` (`engine/host.cpp:4129-4142`,
   `engine/host_cmd.cpp:1237-1350`);
2. `CHostState::State_NewGame` validates `cs_office` and `Host_NewGame`
   succeeds (`engine/host_state.cpp:414-454`);
3. `Host_NewGame` opens the listen socket, spawns and activates the server, and
   queues `connect localhost:<port>` (`engine/host.cpp:6226-6380`);
4. the local client preserves the listen server, connects, and completes
   signon (`engine/cl_main.cpp:898-983`);
5. a CS player entity is created and spawned;
6. DualShock input reaches `ClientDLL_ProcessInput`/`CL_Move`;
7. `Host_UpdateScreen` emits and presents one minimal world frame;
8. bots spawn and the match remains playable.

Use PS4-native branches freely for Steam, Workshop/GC, desktop, plugin, or
dynamic-module assumptions. Defer multiplayer, inventory-icon/model-to-texture
readback, replay, achievements, and other facilities not required by this
chain. Keep required behavior such as map state transitions and signon ordering.

### 4. Feature completion from observed blockers

After the skeleton runs, add only what the next real map/frame requires:

1. the world/model/particle entries in the Source/OpenGNM rendering-closure
   checklist, adding shader combinations, material states, texture formats,
   and render states in observed-failure order;
2. remaining menu actions needed for options, offline launch, pause/resume,
   disconnect, and quit, then noncritical actions;
3. PS4 audio and a combined render/input/UI/audio soak;
4. broader maps and offline modes;
5. multiplayer only after offline acceptance.

Do not block the first match on speculative renderer breadth or optional UI
facilities.

## Definition of the first accepted offline match

All of the following must be true in one production `engine` run on PS4:

- composition/preflight reports no missing required provider and no unapproved
  duplicate owner;
- every failed lifecycle stage is fail-stop and no downstream stage runs;
- Source owns OpenGNM presentation;
- Scaleform menu and DualShock navigation run from Source frames;
- Offline With Bots reaches `cs_office` through the existing request path;
- listen server and local client complete signon;
- the player spawns, moves, and renders a world frame;
- at least one bot participates;
- audio is functional or is explicitly isolated as the only remaining
  acceptance exception;
- a bounded soak completes with balanced frame/present markers and no crash.

## Hardware-test and commit discipline

- Build full `.pkg` candidates and let the user install and launch them
  manually. Do not use ps4debug ELF delivery.
- Give every hardware candidate a unique version/marker and record OELF, SELF,
  PKG, and log hashes.
- Commit code and documentation before each hardware test; after the result,
  commit the log interpretation before starting the next candidate.
- Compare cumulative logs by exact prefix and append hash, then map the final
  marker to source without claiming the next call was entered.
- Run the narrowest host/static tests first, expand when shared interfaces or
  composition change, inspect the final diff, and re-index the repository after
  each committed checkpoint.
- Preserve unrelated untracked files and external content.

## Deferred work

- Multiplayer and online service integration.
- Inventory/model-to-texture readback quality beyond safe fallbacks.
- Workshop/GC, Steam, dynamic plugins, replay, achievements, and desktop-only
  facilities unless an observed offline blocker requires a PS4 replacement.
- Full UI parity beyond actions needed for the first match.
