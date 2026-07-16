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
| Test | v4.94, 2026-07-16 |
| Package version | 3.60 |
| Package | `IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Package size | 103,481,344 bytes |
| Package SHA-256 | `1486680143f635748c84602d28c2fb24e59f3e3ac4d3a9853e05ba06efbe3081` |
| FTP staging path | `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Candidate commit | `79664c15` (`Retain PS4 vphysics surface database`) |
| Hardware-result commit | This v4.94 result record (`Record v4.94 info-panel crash`) |

v4.94 clears the complete server-physics gate. The retained
`VPhysicsSurfaceProps001` resolves, the manifest loads, and
`scripts/surfaceproperties_cs.txt` parses. `CPhysicsHook::Init`, every traced
server game system, `CServerGameDLL::DLLInit`, and the engine game-DLL boundary
complete. The client also clears the former `hltv_status` crash: descriptor
lookup, callback allocation, both listener insertions, scoreboard construction,
and scoreboard viewport insertion all complete.

The crash is now in the next default viewport panel. `CreatePanelByName("info")`
enters the info-panel allocation, constructs its cursor controls, and completes
a mouse-input update, but never returns from the allocation. The exact last line
is `kisak-ps4: panel set mouse input complete`. The fresh log was last modified
at 2026-07-16 22:46:04 UTC; it is 502,648 bytes and 12,797 lines with SHA-256
`185c6b9fe21cc1c9957dbd9739210eb1be30407bbdcc2d53e41fb68340a12592`.

The immediate v4.95 gate is the concrete `info`/text-window constructor after
its last mouse-input operation, not renderer expansion. Attribute the remaining
constructor stages and any registrar/ownership dependency, then require the
panel allocation and insertion to return before advancing toward the first real
Source frame.

### v4.93 result and immediate v4.94 gate

v4.93 no longer crashes. It proves the direct PS4 physics factory resolves
`VPhysics031` and `VPhysicsCollision007`, then fails to resolve
`VPhysicsSurfaceProps001`. The new lifecycle rule works as intended:
`CPhysicsHook::Init` returns false, the initialized game-system prefix unwinds,
`CServerGameDLL::DLLInit` and `SV_InitGameDLL` propagate failure, and `Host_Init`
stops before client/render initialization. The application performs an orderly
shutdown, leaving the observed black screen rather than entering a partial
Source runtime.

The fresh log was last modified at 2026-07-16 22:31:58 UTC. It is 255,797 bytes
and 8,508 lines with SHA-256
`afd98f2ebeb0e3401509e84129dea076fd03add6b4d30c0ec03e30e7cf34dba0`.
Its final causal sequence is surface-properties lookup failure, game-system
unwind, DLL/host fail-stop, orderly app/filesystem shutdown, and
`LauncherMain returned`.

The immediate v4.94 gate is monolithic retention of the real vphysics surface-
properties provider. `vphysics/physics_material.cpp` defines
`g_SurfaceDatabase` and exposes `VPhysicsSurfaceProps001`, so archive and final-
OELF evidence must determine whether its registrar-only member was discarded.
Retain that provider through the explicit composition manifest, verify unique
ownership, then require `CPhysicsHook::Init` to parse surface data and complete.
Do not weaken the fail-stop behavior or substitute a dummy surface database.

### v4.94 candidate: retain the vphysics surface database

Package version 3.60 and build marker
`vphysics_surface_retention_v494` identify the next manual-install candidate.
Archive inspection proves `libkisakvphysics_client.a` contains
`physics_material.cpp.obj`, which uniquely defines `g_SurfaceDatabase`, while
the v4.93 monolithic ELF omits that symbol. All linked archives were inspected;
there is no second owner. The similarly named `physprops` is deliberately not
an anchor because engine and vphysics both define it and the monolithic link
allows duplicate definitions.

The composition manifest now has a distinct vphysics ownership bucket retaining
`g_SurfaceDatabase`. The same symbol is included in the post-link verifier, so
the build fails if the provider is ever discarded again. Pulling that archive
member also retains its `EXPOSE_SINGLE_INTERFACE_GLOBALVAR` constructor and
therefore registers the real `VPhysicsSurfaceProps001` implementation with the
shared `InterfaceReg` chain.

Independent GLM 5.2 and Kimi 2.7 ACP audits agree on the retention diagnosis and
symbol choice. The forward audit identified one remaining attribution gap:
`PhysParseSurfaceData` collapsed manifest loading and every referenced file into
one boundary. PS4-only breadcrumbs now bracket the manifest and record each
listed filename and successful parse without changing the existing fatal-error
policy. The first hardware gate is `CPhysicsHook::Init` completion; existing
game-system and `hltv_status` traces cover the downstream path.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target links and the post-link retention check
  runs successfully;
- the generated link includes `--undefined=g_SurfaceDatabase`, and final-ELF
  inspection finds exactly one defined `g_SurfaceDatabase`;
- the executable is 126,554,488 bytes with SHA-256
  `cf8ea058beb84860619cd47fa37958f9c1252b6cedb46c5890c9e4289ebbf012`;
- the OELF is 136,334,216 bytes with SHA-256
  `4d1ddd82cc6d2ede2ac7062683d216a413e36fd4577ec26c4aebfa029698205f`;
- the SELF input is 83,397,872 bytes with SHA-256
  `317fadd822aba5ddc45763e5e36d97e4b42a8b453b2e54b2ac6e1861fe9d5b76`;
- binary strings confirm the v4.94 marker, retained surface-interface success
  branch, and manifest/per-file boundary families; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `79664c15` produces a 103,481,344-byte package with SHA-256
`1486680143f635748c84602d28c2fb24e59f3e3ac4d3a9853e05ba06efbe3081`.
The embedded SFO reports `APP_VER` and `VERSION` 3.60, verbose PkgTool
validation reports no `[FAIL]` entries and every digest/signature check `[OK]`,
and FTP metadata reports the same 103,481,344-byte size. The package is staged
at `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance remains pending manual
installation and launch.

### v4.92 result and immediate v4.93 gate

v4.92 clears the complete scoreboard resource construction path. `SysMenu` and
`ServerName` factories return, all Panel parent/input/context/sibling-pin stages
complete, existing `PlayerList` settings apply, `BuildGroup` returns, and the
scoreboard finishes its post-resource size, visibility, and spectator state.
The crash is inside the first `ListenForGameEvent( "hltv_status" )` call.

The fresh v4.92 log was last modified at 2026-07-16 17:15:32 UTC. It is
482,699 bytes and 12,501 lines with SHA-256
`c3ccf6fdf2d3a5dc3c039ce4260345ea522324ee03903ec8be47f2d916e3f3f3`.
It is neither capped nor inherited from the prior run, so its tail is a valid
constructor boundary.

The final milestones are:

```text
kisak-ps4: scoreboard build panel apply ready key=ServerName control=Label
kisak-ps4: scoreboard build existing control apply ready key=PlayerList control=SectionedListPanel
kisak-ps4: scoreboard build apply complete key=Resource/UI/ScoreBoard.res control=missing
kisak-ps4: scoreboard build load complete key=Resource/UI/ScoreBoard.res control=missing
kisak-ps4: scoreboard constructor control settings ready
kisak-ps4: scoreboard constructor desired height ready
kisak-ps4: scoreboard constructor player list hidden
kisak-ps4: scoreboard constructor spectator counts ready
kisak-ps4: scoreboard constructor before hltv event listen
```

Scoreboard Panel/factory/resource behavior is no longer the active blocker. A
review of the same bounded log also exposes an earlier lifecycle violation:
server `CPhysicsHook::Init` returns false, but `CServerGameDLL::DLLInit` ignores
the result and startup continues into the client. v4.93 must first give the
server the real PS4 physics-module factory and make every server-init failure
stop and unwind. If physics succeeds, the same candidate brackets the client
`hltv_status` registration through its manager lock, descriptor lookup,
callback allocation, and listener insertions. Do not bypass scoreboard events
or continue from a partially initialized server.

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

Candidate commit `9344e42e` produces a 103,284,736-byte package with SHA-256
`ce977ddc82105aaefcac08d1c55a24a1bf2d20719ffb608441d9511a3d7ccca7`.
The embedded SFO reports version 3.50, and verbose PkgTool validation reports
every limit, digest, and signature check `[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete FTP
readbacks match the local size and SHA-256.

### v4.84 result and immediate v4.85 gate

The manual-install run proves the particle subsystem was not the v4.83 crash
source: both bounded parse probes reach `complete`, and the client ParticleMgr
and several later client-system phases finish. The next candidate is restricted
to bounded `CCSModeManager::Init` and panel-metaclass-loader attribution. A
missing function-entry marker will implicate the `modemanager` dispatch or
ownership; a present marker will isolate the split-screen client-mode setup or
`scripts/vgui_screens.txt` loading.

### v4.85 candidate instrumentation

Package version 3.51 and build marker `client_mode_manager_v485` identify the
next manual-install candidate. `CCSModeManager::Init` now records virtual-body
entry, each split-screen guard's construction and release, the active-slot and
normal-client-mode lookup, the panel-manager accessor, and the definition-file
call. The client-mode detail probe is enabled only for this Init call.

The panel-metaclass path records singleton readiness, allocation credit,
existing-definition lookup and cleanup, `KeyValues` allocation/dictionary
insertion, `scripts/vgui_screens.txt` loading, list traversal, panel-type
lookup, and metaclass dictionary insertion. Detail is capped at 512 records
plus one explicit sentinel. Call order, split-screen behavior, file-loading
policy, parser return values, and dictionaries are unchanged; the legacy
single-byte copyright header was mechanically normalized to ASCII so the file
could be edited safely.

The Linux OpenOrbis monolithic build completes. The OELF is 136,086,944 bytes
with SHA-256
`679d35e42f4ba47c22c364478a8ad4b5467ae4d6b37a46ef9339895a40c27a25`;
the SELF input is 83,192,528 bytes with SHA-256
`ce43676b91be5d26427b71761dfa3633b36289268db2e6f8482fcdd7eddad744`.
The focused client target also compiles. Eleven of fourteen host tests pass;
the same three Linux-address OpenGNM descriptor fixtures fail outside the
changed client-mode/metaclass code.

Candidate commit `548faf46` produces a 103,284,736-byte package with SHA-256
`0f9e308e6574ad9280bb6908cc8b72d002e0598232538cf49a62037bf07e2ec3`.
The embedded SFO reports version 3.51, and verbose PkgTool validation reports
every limit, digest, and signature check `[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete FTP
readbacks match the local size and SHA-256.

### v4.85 result and immediate v4.86 gate

The manual-install run reaches the real `CCSModeManager::Init` body and clears
normal-client-mode creation, panel-manager access, KeyValues allocation,
filesystem loading, and the first metaclass traversal. It fails when
`ParseSingleMetaClass("c4_panel")` cannot find the `c4_panel` factory installed
by the static object declared in `game/client/cstrike15/vgui_c4panel.cpp`.

The v4.86 gate is monolithic composition closure for the required VGUI panel
factories. Inspect the client archive and final OELF before editing; if the
constructor-only object is present only in the archive, retain it through the
explicit client interface-retention manifest and preflight every type required
by `scripts/vgui_screens.txt`. Do not treat suppressing the legacy `Warning` as
the fix: the required factory must be registered, uniquely owned, and usable
before `CCSModeManager::Init` may report success.

### v4.86 candidate: retain and verify client static providers

Package version 3.52 and build marker `panel_factory_retention_v486` identify
the next manual-install candidate. Archive and final-OELF inspection confirms
the v4.85 failure was static archive extraction: all five VGUI factory objects
exist in `libclient_client.a`, but the v4.85 OELF contains only
`g_CVGuiScreenPanelFactory`. In particular, `vgui_c4panel.cpp.obj` defines
`g_CC4PanelFactory`, its constructor, and the complete `CC4Panel` body, but no
ordinary undefined reference caused the linker to extract that member.

`cmake/ps4_monolithic_retention.cmake` is now the explicit client/engine/server
retention manifest. It retains both earlier ad-hoc client providers and the
complete known VGUI screen-factory set with linker undefined roots. The old
volatile references were removed from `KisakGameClientFactory`. A post-link
verification step fails the build if any manifest symbol is absent, converting
silent registrar discard into a build-time composition failure.

The Linux OpenOrbis monolithic build and post-link verification complete. The
OELF is 136,251,888 bytes with SHA-256
`e9881d55294027f173dcf9ecf74673009b3992a424b7ea4b9123b40ab0290ab3`;
the SELF input is 83,330,304 bytes with SHA-256
`60998ad798b9f61d836b2b198767d01c1df2cb510db69c5259e0303cfb9a0b84`.
The final OELF contains all five factory globals and their exact registration
strings: `c4_panel`, `c4_view_panel`, `movie_display_screen`,
`slideshow_display_screen`, and `vgui_screen_panel`. Eleven of fourteen host
tests pass; the same three Linux high-address OpenGNM fixtures fail outside the
changed link-composition path.

Candidate commit `47fb2b0f` produces a 103,415,808-byte package with SHA-256
`2b588685815d7640697a921dffea20964b3b7bb56a66be12ef2d598c05ada208`.
The embedded SFO reports version 3.52, and verbose PkgTool validation reports
every limit, digest, and signature check `[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete FTP
readbacks each return 103,415,808 bytes with the same SHA-256.

### v4.86 result and immediate v4.87 gate

The manual-install run validates the first production use of the retention
manifest: both required C4 panel factories register, both metaclasses parse,
and the real mode-manager body completes. Client startup proceeds into
`LoadSchemeFromFileEx("resource/ClientScheme.res", "ClientScheme")` and clears
all bounded work through font and glyph setup.

The last marker is immediately before the object-creation loop in
`CScheme::LoadBorders`. Because the earlier SourceScheme border load completes
in the same process, do not replace or disable the border subsystem globally.
The v4.87 candidate must first record the ClientScheme entry name, data type,
border type, allocation, name assignment, and settings-application boundaries.
If settings application is reached, continue the bounded probe through
`Border::ApplySchemeSettings` and `ParseSideSettings`; retain the real border
behavior unless hardware evidence identifies a PS4-specific incompatibility.

The gate remains open until hardware proves `ClientDLL_Init` completion,
successful EngineVGui/GameUI hookup, and one complete production
`eng->Frame()`.

### v4.87 candidate instrumentation

Package version 3.53 and build marker `client_scheme_borders_v487` identify the
next manual-install candidate. The ClientScheme-only border probe now records
the first subkey, every entry's data type, list insertion, border type and
allocation, name lookup/assignment, settings application, and final list
storage. Detail resets for this scheme and is capped at 2,048 records plus one
explicit sentinel. The ordinary SourceScheme load keeps only the existing
coarse milestones.

The scalable-image implementation separately brackets surface availability,
texture-ID creation, every resource-field lookup, image-path allocation and
formatting, texture-file binding, and texture-size lookup. This covers the first
specialized border type in the shipped `ClientScheme.res`; the outer probe will
still identify an ordinary or image-border failure before any additional
implementation-specific instrumentation is needed. Border traversal, object
selection, allocation order, settings, and resource-loading behavior are
unchanged.

The focused `vgui2_client` target and full Linux OpenOrbis monolithic target
compile, and the post-link retention verifier passes. The OELF is 136,252,064
bytes with SHA-256
`4625b10a72acafb19c2b0caef2033c312aadde0f718ac78047752e06794b683e`;
the SELF input is 83,330,304 bytes with SHA-256
`261b02ffa225f02a66c755f483a3288940eaa483d572e6816245f76f2a2f0864`.
Eleven of fourteen host tests pass; the same three Linux high-address OpenGNM
descriptor fixtures fail outside this VGUI scheme-loading path.

Candidate commit `53c5d052` produces a 103,415,808-byte package with SHA-256
`da63191f79fb32b4bb3c3f39e6cf372c1c487c9599b74790fc826006a7394dc5`.
The embedded SFO reports version 3.53, and verbose PkgTool validation reports
every limit, digest, and signature check `[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete FTP
readbacks each return 103,415,808 bytes with the same SHA-256.

### v4.87 result and immediate v4.88 gate

The manual-install run clears all ordinary ClientScheme borders before reaching
entry 19, `LoadoutItemBorder`, whose `bordertype` is `scalable_image`.
`ScalableImageBorder` receives a live surface, creates texture ID 5, reads and
scales its corner settings, resolves a nonempty image, allocates and formats the
25-byte `vgui/store/store_item_bg` path, and enters
`ISurface::DrawSetTextureFile`. That call does not return.

The immediate v4.88 gate is the concrete texture-file binding implementation,
not another general renderer feature. Resolve the actual `ISurface` owner in the
monolith, then bound `DrawSetTextureFile`, texture-dictionary lookup/binding,
material lookup or creation, and filesystem access. Confirm the texture asset's
runtime availability and search path. Preserve normal Source behavior unless
the observed failing operation depends on an unsupported desktop/dynamic-module
contract; if the border is optional for the first frame, any temporary deferral
must be PS4-specific, explicit, and leave a later restoration gate.

Source and content inspection narrows that gate further. Texture ID 5 proves
the provider is `CMatSystemSurface`: its texture dictionary returns low linked-
list indices, while `CWin32Surface` starts IDs at 2700. The requested
`materials/vgui/store/store_item_bg.vmt` is absent from the loose PS4 content
and all three shipped VPK indexes. That absence is not itself an error in the
port: `CMaterialSystem::FindMaterial` is designed to return `g_pErrorMaterial`
after a negative lookup. The candidate therefore traces the negative lookup
and the lazy error-material precache rather than adding or inventing a renderer
asset.

The audit also found a separate lifecycle defect: the monolithic material-
system factory selects `shaderapiempty` before the launcher requests
`shaderapips4`, and `SetShaderAPI` rejects the second selection. Correcting the
single-owner backend selection remains required, but is deliberately isolated
from v4.88 because enabling a different backend changes broad renderer behavior
and would destroy attribution for this crash.

### v4.88 candidate instrumentation

Package version 3.54 and build marker `vgui_material_lookup_v488` identify the
next manual-install candidate. Exact-name PS4 probes bracket
`CMatSystemSurface::DrawSetTextureFile`, texture-ID validation and dictionary
binding, material dictionary lookup, VMT allocation/loading, KeyValues open,
each VPK and search-path miss, regular-file fallback, error-material return,
reference ownership, mapping-size queries, and every stage of lazy error-
material precaching. The pointer overload records entry before dereferencing
the returned material so a bad queue-friendly pointer remains distinguishable
from failure inside precache.

The probes do not change material selection, ownership, search order, border
construction, or rendering. If hardware reaches the error-material mapping
path and fails there, the next PS4-native unblock may bind no material for this
optional missing VGUI image, which the existing texture dictionary renders via
its white fallback. If the lookup and fallback complete, the final probe
distinguishes `DrawFlushText` from the bound-texture assignment inside
`DrawSetTexture`.

The focused `tier1_client`, `filesystem_stdio_client`,
`materialsystem_client`, `vgui2_client`, and `vguimatsurface_client` targets
compile, followed by the full Linux OpenOrbis monolithic target. Both the
post-link retention hook and an explicit verifier pass. The OELF is 126,490,968
bytes with SHA-256
`b83483c535c690ee270c5a5d59282f51b61e01a0a5978a9ac442cc0b04e49be0`;
the SELF input is 83,346,720 bytes with SHA-256
`ca348b966e9c347676edd408ac248b6aa712e8bc9e339ee4bfdf68e9861ca057`.
Eleven of fourteen host tests pass; the same three Linux high-address OpenGNM
descriptor fixtures fail outside the VGUI material lookup path.

Candidate commit `f30f3272` produces a 103,415,808-byte package with SHA-256
`400ef8e2b159e4a8ab67264d5d502a13b2f3de063ae917027e7a49e59bc248a9`.
The embedded SFO reports version 3.54, and verbose PkgTool validation reports
every limit, digest, and signature check `[OK]`. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; the FTP server reports
103,415,808 bytes, and two complete readbacks return the same SHA-256.

### v4.88 result and immediate v4.89 gate

The manual-install run clears every negative filesystem lookup boundary and
returns the real queue-friendly error material. It also clears the error check:
`IsErrorMaterial` returns true. The next expected marker, `material file before
pointer set`, is absent. Between those two markers the original code only emits
`Msg( "--- Missing Vgui material %s\n", pFileName )`; the OSX-only `printf`
branch is compiled false for PS4.

This is a diagnostic-output failure, not evidence against scalable borders,
the texture dictionary, material lookup, or error-material precaching. The
v4.89 gate is therefore a narrow PS4-only suppression of that exact optional
message for `vgui/store/store_item_bg`. Do not skip `SetMaterial(pMaterial)`:
the next run must prove reference acquisition, cleanup, mapping dimensions,
lazy error-material precache, texture binding, and the pending text flush.

### v4.89 candidate repair

Package version 3.55 and build marker `vgui_missing_message_v489` identify the
next manual-install candidate. On PS4 only, and only when the exact
`vgui/store/store_item_bg` lookup returns the error material, the VGUI texture
dictionary records `material file missing message suppressed` instead of
calling the failing `Msg` diagnostic. All other missing materials and all
non-PS4 platforms retain the original complaint behavior.

The repair deliberately continues through `SetMaterial(pMaterial)` with the
real queue-friendly error material. The retained v4.88 probes will therefore
locate any next failure in reference acquisition, old-material cleanup, lazy
precache, mapping dimensions, dictionary return, or `DrawFlushText` without
combining those stages with another renderer change.

Validation before the candidate commit:

- the focused `vgui2_client` target and full `kisak_ps4_monolithic` target
  compile successfully;
- the post-link retention hook and an explicit
  `verify_ps4_retained_symbols.cmake` run pass;
- the OELF is 126,490,968 bytes with SHA-256
  `f30f43707ca86e645eaef143ddde45902539080573e8f2a085324c67e8af0a78`;
- the SELF input is 83,346,720 bytes with SHA-256
  `395fe216182d87ae88676a2fc5e7142268352061b9f8d57ee5896a35f767aad3`;
- binary string inspection confirms the v4.89 marker, suppression marker,
  pointer-set probe, and raw error-material mapping probe; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `e4f69fa5` (`Bypass PS4 missing VGUI material message`) was
packaged as monolithic version 3.55. PkgTool validates every reported size,
digest, and signature check. The package is 103,415,808 bytes with SHA-256
`3ed21d69e652b71569c0db1ba043e7602c14d99e12e4889064e96059c60cb34c`.
It is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two independent complete
FTP readbacks produced the same SHA-256, and the server reports the same
103,415,808-byte content length.

### v4.89 result and immediate v4.90 gate

The manual-install run proves every downstream stage that v4.89 was designed
to preserve. The real `___error` material acquires its reference, precaches
under its lock, produces valid 32-by-32 dimensions, binds to texture ID 5, and
survives the pending text flush. `LoadoutItemBorder` then completes its texture
size, paint-first, color, list storage, and ClientScheme entry lifecycle.

Entry 20 is another `scalable_image` border named
`LoadoutItemMouseOverBorder`. Its nonempty image produces a 35-byte allocation,
formats the `vgui/` path, and enters `DrawSetTextureFile` with texture ID 6;
that call does not return.

Direct extraction of the authoritative `resource/clientscheme.res` entry from
the staged PS4 `pak01_dir.vpk` and `pak01_068.vpk` content identifies the image
as `store/store_item_bg_highlight`, producing material path
`vgui/store/store_item_bg_highlight`. The main VPK index contains only
`materials/vgui/store/store_zoom.vmt`; the low-violence and Perfect World VPK
indexes contain no store VMTs, and neither staged loose-content root has a
store-material directory. The highlighted background is therefore confirmed
absent, together with the other stale store-border images referenced by the
scheme.

An independent read-only ACP Router audit confirmed that this is the same
`CMatSystemTexture::SetMaterial` diagnostic failure and recommended suppressing
the missing-VGUI-material `Msg` for every PS4 error-material result at this one
site. Its resource-name inference was superseded by the direct VPK extraction;
its source-path and policy conclusions agree with the hardware and content
evidence.

### v4.90 candidate repair

Package version 3.56 and build marker `vgui_missing_message_v490` identify the
next manual-install candidate. On PS4, every VGUI lookup that has already
resolved to the error material now emits the low-level
`material file missing message suppressed` breadcrumb instead of entering the
crashing `Msg` diagnostic. Non-PS4 behavior is unchanged.

The filename overload still calls `SetMaterial(pMaterial)` unconditionally.
All PS4 missing-material results now also set the existing pointer-probe target
around that call, so the log records the exact missing filename plus reference,
precache, mapping-dimension, and completion boundaries without adding another
content-family exception.

Validation before the candidate commit:

- the focused `vgui2_client` target and the full OpenOrbis
  `kisak_ps4_monolithic` target compile successfully with the pinned toolchain;
- the post-link retention hook and a separate explicit retention-manifest run
  pass;
- the OELF is 126,490,968 bytes with SHA-256
  `86ed33ff5c137f06702aafaf4b79d552cf76cf2f65e4608ef4531a98aaa69e6a`;
- the SELF input is 83,346,720 bytes with SHA-256
  `e306ac001d3bcfaf827c59bc0c603392dbb9e4cab90937bd154ad4a80752fdb1`;
- binary string inspection confirms the v4.90 build marker, the generalized
  suppression breadcrumb, pointer-set boundary, and raw mapping probe; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `10a0e035` (`Handle all PS4 missing VGUI materials safely`)
was packaged as monolithic version 3.56. PkgTool validates every reported size,
digest, and signature check. The package is 103,415,808 bytes with SHA-256
`1195622c2db572f399b29b0dabbe58194a8870ca47310e6e9d80ecaff277c9e5`.
It is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two independent complete
FTP readbacks produced the same SHA-256, and the server reports the same
103,415,808-byte content length.

### v4.90 result and immediate v4.91 gate

The manual-install run proves that the generalized PS4 error-material policy
works across every stale VGUI store-border image without weakening real
material binding. All 49 ClientScheme border entries complete, followed by the
object and reference passes, base-border resolution, scheme-data load, and
`LoadSchemeFromFileEx` return.

Client game systems then enter viewport initialization. The log continues
through thousands of panel, VPanel, and material-surface boundaries. It proves
the base viewport allocation completes and records many later parented child
controls. Its final recorded operation is a complete
`Panel::SetMouseInputEnabled` call: exhaustive popup visibility scanning
returns, the input context is enabled, the cursor unlocks, and
`surface()->CalculateMouseVisible()` returns.

That tail is not a reliable crash boundary. The remote file is exactly
4,194,320 bytes, `OpenStartupLog` used append mode for every run, and breadcrumb
writes ignored I/O errors. Prefix counts also show that cleared particle,
ClientScheme, Panel, VPanel, and material-surface probes consumed most of the
log. The absent `client game systems viewport init ready` marker therefore does
not prove the enclosing call failed before returning.

The next candidate must attribute the first normal viewport path rather than
add renderer policy. Add bounded per-slot boundaries around normal and
fullscreen `InitViewport`, then distinguish `ClientModeCSNormal` allocation,
`CounterStrikeViewport::Start`, `CBaseViewport::Start`, default-panel creation,
panel factory dispatch, and panel registration. A name-aware boundary at the
completed mouse-input call may be used to identify the last constructed panel;
do not change viewport behavior until the failing caller statement is proven.

### v4.91 candidate instrumentation

Package version 3.57 and build marker `viewport_start_boundary_v491` identify
the next manual-install candidate. The startup log is truncated once at process
entry, while later breadcrumbs continue to append. The central breadcrumb sink
filters only high-volume diagnostic families whose subsystem gates are already
closed: particle parse, pending ConVar registration, Panel/VPanel construction,
material-surface internals, scheme-font helpers, ClientScheme border detail,
scalable-border detail, material detail, and VGUI texture detail. The mouse
input completion markers and all new viewport boundaries remain enabled.

The first normal viewport path now distinguishes each split-screen guard,
normal and fullscreen dispatch, base client-mode initialization, viewport
allocation, `CounterStrikeViewport::Start`, `CBaseViewport` construction and
startup, background/event/scheme/default-panel stages, each named panel factory
and registration operation, animation-controller setup, and HUD-animation load.
The fullscreen path has equivalent base-only allocation/start boundaries.

An independent read-only audit corrected the initial tail interpretation. The
base viewport's null-parent input sequence completes hundreds of lines before
the cap, followed by many named child panels. The capped tail instead matches a
parented `RichTextInterior` keyboard/mouse sequence, plausibly under the
`CTextWindow` `TextEntry` allocation. v4.91 keeps this as a hypothesis: the
factory and allocation-return markers will prove or reject it without changing
RichText or viewport behavior.

Validation before the candidate commit:

- `git diff --check` passes;
- the focused `client_client` and `engine_client` archives compile;
- the full `kisak_ps4_monolithic` target links, and its retention-manifest hook
  passes;
- the executable is 126,491,672 bytes with SHA-256
  `3387b6780c72caf67b98367e8afed28b9f38387b4b9186b0419a2cee56aa2bd8`;
- the OELF is 136,269,512 bytes with SHA-256
  `67d0da6f83ebf9acd2af5c9f4ea0d4fd262a515b3409b939c5157553a043e679`;
- the SELF input is 83,346,720 bytes with SHA-256
  `7f6d906c9ac90fcdf216474ead907e819a22e27584478d23da0bb2dcb8485f89`;
- binary string inspection confirms the v4.91 marker and constructor,
  normal/fullscreen allocation, CS start, and named panel-add boundaries; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `505b7ab1` (`Instrument PS4 viewport startup`) was packaged as
monolithic version 3.57. PkgTool reports every size, digest, and signature check
`[OK]`. The package is 103,415,808 bytes with SHA-256
`689723ddbf469d02ab6223ba2a3fdf6df14196e164f15b77dd93b1814a6fd71c`.
The embedded SFO reports 3.57. It is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; the FTP server reports
the same size, and two independent complete readbacks produced the same
SHA-256.

### v4.91 result and immediate v4.92 gate

The fresh-log policy works: the hardware run produced a bounded 453,233-byte,
12,054-line log rather than another 4 MiB append cap. Marker
`viewport_start_boundary_v491` confirms the tested binary. ClientScheme returns,
the normal viewport allocation and base startup complete through background,
event, scheme, and proportional setup, and default-panel creation begins.

The team and buy factory misses are expected and safe. The first scoreboard
factory then records:

```text
kisak-ps4: viewport panel factory entered name=scores
kisak-ps4: viewport panel factory before scoreboard allocation name=scores
... seven complete Panel::SetMouseInputEnabled sequences ...
kisak-ps4: panel set mouse input complete
```

No `panel factory scoreboard allocation ready` marker follows. This proves the
failure is contained by `new CClientScoreBoardDialog( this )` in
`game/client/game_controls/baseviewport.cpp`; it is not the earlier
`RichTextInterior`/text-window hypothesis. The constructor in
`game/client/game_controls/ClientScoreBoardDialog.cpp` creates its base panel,
sets input and scheme state, allocates/configures `SectionedListPanel`, loads
`Resource/UI/ScoreBoard.res`, then completes size, visibility, event, image-list,
and avatar-map initialization. Existing mouse markers do not distinguish those
statements.

The v4.92 gate is a focused scoreboard-construction trace. It must bracket the
constructor body and each stage above, then bracket `EditablePanel`/
`BuildGroup::LoadControlSettings`, resource-file load, root `ApplySettings`, and
each resource child dispatch only if the outer resource-load boundary fails to
return. Initialize members that resource or scheme callbacks can observe before
the resource load; in particular, `ApplySchemeSettings` reads and may delete
`m_pImageList`, while the current constructor assigns it only after
`LoadControlSettings`. Keep that ordering repair separate and explicit in the
trace rather than treating desktop construction order as authoritative.

### v4.92 candidate instrumentation and ordering repair

Package version 3.58 and build marker
`scoreboard_scrollbar_boundary_v492` identify the next manual-install candidate.
The trace scope is enabled only while `CreatePanelByName( "scores" )` constructs
`CClientScoreBoardDialog`, so the additional Panel, factory, BuildGroup, Label,
sectioned-list, and scrollbar boundaries cannot recreate the earlier global log
flood.

Direct inspection of the deployed `csgo/pak01_dir.vpk` confirms the scoreboard
resource's top-level order: `ClientScoreBoard`, `SysMenu` (`Menu`), `ServerName`
(`Label`), then the existing `PlayerList` (`SectionedListPanel`). Matching that
order against popup semantics corrects the seven-call map:

1. the scoreboard explicitly disables its own mouse input;
2. `PlayerList`, its scrollbar, slider, top button, and bottom button inherit
   parent state; and
3. resource-created `ServerName` inherits the scoreboard's disabled state.

`SysMenu` is a popup, so parenting it skips the non-popup input-state sync. The
v4.91 tail therefore proves `ServerName` reaches and completes its mouse sync,
but it does not distinguish the remaining `Panel::SetParent` context propagation
and sibling-pin update, the Label's resource `ApplySettings`, existing
`PlayerList` settings, or resource-load finalization.

v4.92 records those exact boundaries. `BuildGroup` logs each resource key,
factory request, parent operation, and settings application; the factory helper
identifies the requested control class; `Panel::SetParent` separately brackets
IPanel parenting, proportional/keyboard/mouse synchronization, message-context
query and propagation, and sibling-pin update. Constructor-specific boundaries
remain around the sectioned list, scrollbar slider/buttons, Label text image,
and the outer scoreboard stages.

The candidate also closes one definite ordering defect without depending on
desktop timing: `m_pPlayerList`, `m_pImageList`, and `m_pViewPort` are initialized
at the start of the scoreboard constructor, before `SetScheme` or
`LoadControlSettings` can expose them to callbacks. The historical trailing
`m_pImageList = NULL` assignment was too late because `ApplySchemeSettings`
tests and may delete that pointer.

Validation before the candidate commit:

- `git diff --check` passes;
- the focused `client_client` and `engine_client` archives compile;
- the full `kisak_ps4_monolithic` target links, and its retention-manifest hook
  passes;
- the executable is 126,508,728 bytes with SHA-256
  `126b09ded8e0d5ef238854399f77b82902d0ae006745f22bc8283332b2aedfe8`;
- the OELF is 136,286,568 bytes with SHA-256
  `293e096ece2034fae1d65b8ba06a11f82913a5b3203f5c313d49516fefd02b65`;
- the SELF input is 83,363,136 bytes with SHA-256
  `00da6a0004c574eb71c33277ba31dbf12c96a68813a982378a3c8ef48883cc5e`;
- binary string inspection confirms the v4.92 marker and scoreboard constructor,
  Panel, BuildGroup, factory, Label, sectioned-list, and scrollbar trace families;
  and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `ec3ad15c` (`Trace PS4 scoreboard resource construction`) was
packaged as monolithic version 3.58. PkgTool reports every size, digest, and
signature check `[OK]`. The package is 103,481,344 bytes with SHA-256
`5d5f01852d90b98e6ac2b7bcfc86152eb4c566dd1416a5b3b7188883ad44d40e`.
The embedded SFO reports 3.58. It is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; the FTP server reports the
same size and a 2026-07-16 17:09:41 UTC modification time. Two independent
complete readbacks produced the same SHA-256.

### v4.92 result and immediate v4.93 gate

The manual-install run clears every scoreboard resource and post-resource
constructor stage instrumented by v4.92. `SysMenu` and `ServerName` are created
and applied, the existing `PlayerList` settings return, the outer BuildGroup
load returns, and the scoreboard reaches its desired-height, player-list
visibility, and spectator-count initialization. The exact final line is:

```text
kisak-ps4: scoreboard constructor before hltv event listen
```

The corresponding `scoreboard constructor hltv event listen ready` marker is
absent, so the active failure is inside
`ListenForGameEvent( "hltv_status" )`, not in Panel parenting, control factory
selection, `ScoreBoard.res`, or the scoreboard's scalar initialization.

Static final-OELF inspection rules out the most immediate monolithic ownership
hypothesis for this call. The scoreboard constructor calls the client inline
body at `0x14f4180`, that body passes `bServerSide = false` and reads the client
`gameeventmanager` global at `0x4b45a58`, and `CHLClient::Init` stores its
successful `INTERFACEVERSION_GAMEEVENTSMANAGER2` factory result into that same
global. The separate server inline body and server global are not selected.

The complete log adds an earlier, authoritative dependency to that gate. Server
game-system initialization reaches `CPhysicsHook` at index 1 and records a
false return, after which the `server game systems init all ready` marker is
absent. `CServerGameDLL::DLLInit` stores that failure in `bInitSuccess` but then
continues through debug overlay, game types, and its successful return. Because
`CHLTVDirector::Init` occurs later in the aborted game-system list and loads
`resource/hltvevents.res`, `hltv_status` is probably unknown when the client
scoreboard asks for it. Descriptor absence should return safely, so this
explains invalid lifecycle state but does not yet attribute the final crash.

### v4.93 candidate: physics factory closure, fail-stop, and listener trace

Package version 3.59 and build marker
`server_physics_event_boundary_v493` identify the next manual-install candidate.
On PS4, `SV_InitGameDLL` now passes `KisakVPhysicsFactory()` as the physics
factory instead of substituting the app-system aggregate for the desktop
physics-module factory contract. `CPhysicsHook::Init` separately records the
factory and the `VPhysics031`, `VPhysicsCollision007`, and
`VPhysicsSurfaceProps001` queries, surface-data parsing, and completion.

The candidate also enforces the active plan's fail-stop rule. A failed game
system Init unwinds the successfully initialized prefix and clears the game-
system initialized flag. `CServerGameDLL::DLLInit` returns false immediately
when `InitGameSystems` fails; `SV_InitGameDLL` now returns a real status; and
`Host_Init` stops before log, HLTV, client, material, or model initialization
when that status is false. The demand-load new-game path also rejects a failed
server DLL initialization instead of using partially initialized interfaces.

If the direct PS4 physics factory clears that earlier failure, bounded
`hltv_status` markers then identify the client inline body, manager presence,
public and recursive manager locks, event-map and descriptor-vector lookup,
callback allocation, global-listener insertion, descriptor-listener insertion,
and the unknown-event warning boundary. Descriptor indices are validated before
vector access, and listeners are marked registered only after a successful
manager registration. Normal event delivery is unchanged.

Review hardening keeps that instrumentation safe at the crash boundary: event
name matching stops at the first mismatch or terminator, both descriptor lookup
paths validate vector indices, and the trace flag is read under the manager
mutex. A failed game-system Init now releases the model-cache lock scopes before
unwinding already initialized systems.

Validation before the candidate commit:

- `git diff --check` passes;
- the focused `engine_client`, `client_client`, and `server_client` targets
  compile;
- the full `kisak_ps4_monolithic` target links and its retention-manifest check
  passes;
- the executable is 126,529,952 bytes with SHA-256
  `98ad3048103bc9de637a458b183be55cf2cc691f0d525a5ad7fee8a8552458ee`;
- the OELF is 136,307,792 bytes with SHA-256
  `74a35d84d3a169ecb1064a55087b6ecb8c131eb81c9f6803d5a6abe39892af4f`;
- the SELF input is 83,379,552 bytes with SHA-256
  `924dc46bc71d22881853028e48ebdfb8e1931a7a13f8df2dc1712b71082797e6`;
- binary string inspection confirms the v4.93 build marker, all three physics
  interface outcomes, fail-stop marker, and event-manager boundary families;
  and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`,
  and `ps4_gnm_constants`).

Candidate commit `a0bbfa0a` produces a 103,481,344-byte package with SHA-256
`76f1b250d28772f2daaa0a1877c3e242fe468e2d3abae9f42fa0fee9851a9527`.
The embedded SFO reports `APP_VER` and `VERSION` 3.59, verbose PkgTool
validation reports no `[FAIL]` entries and every digest/signature check `[OK]`,
and FTP metadata reports the same 103,481,344-byte size. The package is staged
at `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance remains pending manual
installation and launch.

## Runtime topology: two tracks, one production authority

`KisakRegisterStaticModules` registers both tracks in
`appframework/Ps4StaticModules.cpp:54-83`:

| Registered runtime | Factory | Purpose | Acceptance status |
|---|---|---|---|
| `presentation_engine` | `KisakEngineBootstrapFactory()` | Diagnostic OpenGNM/Scaleform/input loop and rollback target | Hardware validated, but not a production Source-frame result |
| `engine` and `source_engine` | `KisakSourceEngineFactory()` | Real Source app systems, server, client, host, and frame lifecycle | Hardware completes ClientScheme and base viewport startup, then reaches first scoreboard default-panel construction; no first frame yet |

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
| Source server/client startup | Yes, still being closed | Yes | Through client mode-manager call boundary | Client Init + EngineVGui + first `eng->Frame()` |
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
