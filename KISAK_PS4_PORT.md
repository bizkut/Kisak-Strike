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

## Renderer architecture decision

The production renderer is a direct Source backend:

```text
Source Material System
    -> IShaderDeviceMgr / IShaderDevice / IShaderAPI / IShaderShadow
    -> shaderapips4
    -> OpenGNM
    -> libSceGnmDriver + SceVideoOut
```

Do not route PS4 rendering through ToGL, a D3D9-to-OpenGL translation, or a
new OpenGL state emulator. Those layers are not linked into the current PS4
monolith and would add a second state/resource translation without closing any
Source contract. Use `materialsystem/shaderapips3` and the PS3 GCM code only to
understand Valve's console-side interface split, state caching, resource
ownership, and command-buffer lifecycle; do not translate its RSX/SPU
implementation line by line.

OpenGNM is the hardware-facing layer, not a Source renderer and not a shader
compiler. It supplies GNM command encoding, descriptors, address/surface
helpers, direct-memory support, VideoOut integration, PS4 shader-binary parsing,
fetch-shader generation, and the Orbis driver backend. `shaderapips4` must still
own Source buffer semantics, texture/render-target lifetimes, cached render
state, synchronization, queries, constants, material integration, and
presentation. State setters update a shadow state; only dirty state is emitted
before a draw.

PS4 vertex and pixel programs are offline build artifacts:

```text
restricted Source shader source/combos
    -> normalized PS4 source
    -> PS4 shader compiler/toolchain
    -> GCN program + PS4 metadata
    -> packaged shader manifest/binary
    -> shaderapips4/OpenGNM parse and bind
```

The first supported set is deliberately narrow: a solid/debug shader,
`UnlitGeneric` plus the Source UI texture path, `LightmappedGeneric`,
`VertexLitGeneric`, depth-only/basic shadow, basic particles, and sky. Add a
reduced water path later. Post-processing breadth, full HDR, SSAO, motion blur,
rare cinematic/editor shaders, and advanced refractive/custom-character
permutations are not first-match blockers unless hardware proves otherwise.

Cross-compilation, monolithic linking, and the diagnostic OpenGNM display loop
are completed foundations, not phases to restart. The active work is startup
closure followed by production runtime integration and restricted shader
coverage.

## Current hardware checkpoint

| Item | Value |
|---|---|
| Last hardware result | v5.05, 2026-07-17 |
| Staged candidate | None; v5.06 engine-pointer ownership repair is in development |
| Package version | 3.71 |
| Package | `IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Package size | 103,481,344 bytes |
| Package SHA-256 | `35325810ef119e50a2ae89c83a218f32753db1831706dfca2b16d28f71d4fe6c` |
| FTP staging path | `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` |
| Candidate commit | `84dd5c34` (`Trace PS4 duplicate ConVar linking`) |
| Hardware-result commit | Pending this documentation checkpoint |

v5.00 proves list construction and the model-cache lock are healthy through
client game-system index 28. `CInventoryManager` is index 27, auto-list entry
14; every shared-object registration and cache-directory operation completes,
followed by its paired successful `after init`. The exact final marker remains
`kisak-ps4: client game system before init index=28 name=unnamed value=46`.
The missing paired `after init` places the hard fault inside that system's
`Init()` body. It is auto-registration list entry 15 because explicitly added
systems occupy indices 0 through 12.

The fresh log was last modified at 2026-07-17 03:54:03 UTC; it is 543,092 bytes
and 13,507 lines with SHA-256
`342597e0f8e2be42896afe46b04e97a06538e93492f40bb002de3b0cd9d12d6a`.
The earlier constructor-order inference assigned entry 15 to
`CInventoryManager`; the stable runtime name corrects that attribution to entry
14. Entry 15 is the immediately following global auto system and must now be
identified from final-binary order before its `Init()` dependencies are changed.

### v4.95 candidate: attribute info-panel construction

Package version 3.61 and build marker `info_panel_construction_v495` identify
this attribution-only candidate. It does not bypass a control or change panel
behavior. PS4-only breadcrumbs bracket the `info` instance's `Frame` body,
visibility and popup creation, base-frame setup, and constructor completion.
The `CTextWindow` body then brackets its scheme/state operations, `TextEntry`,
optional `CMOTDHTML`, both labels, button, command, multiline state, content
type, mouse-input disable, and constructor return.

The hardware decision is bounded: absence of the first `info frame` marker
keeps the failure in base construction; a final `before popup` marker isolates
`surface()->CreatePopup`; a completed frame followed by `before html allocation`
isolates the optional Steam-oriented HTML control; otherwise the final paired
marker names the next exact operation. Only after hardware isolates
`CMOTDHTML` may the next candidate omit that optional control and retain the
plain-text MOTD path.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target links, the retention verifier passes,
  and `create-fself` completes with the OpenOrbis toolchain environment set;
- the executable is 126,554,488 bytes with SHA-256
  `bcd58a2d53f6feee1ebd9a1d5bcc28bcafdbe136d452c0765e9c08be6979c957`;
- the OELF is 136,334,216 bytes with SHA-256
  `5fca9f54d9d4bd0852de1cf2c0220b3f3b873fafaad2d52be3b7e026217e159b`;
- the SELF input is 83,397,872 bytes with SHA-256
  `8813792445aed7b53e7e8e8add537f941f2642924c64487191ff865ae94eb5e0`;
- binary strings contain the v4.95 marker plus the frame-popup, HTML-allocation,
  and constructor-completion boundary families; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Candidate commit `6ecbee92` produces a 103,481,344-byte package with SHA-256
`cf2180640940140d995291e391c7ada0293a81796995aa304bbfd9d44fb18ad9`.
The embedded SFO reports `APP_VER` and `VERSION` 3.61. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-16 23:11:51 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance remains pending manual
installation and launch.

### v4.96 candidate: retain the plain-text MOTD path on PS4

Package version 3.62 and build marker `ps4_text_motd_v496` identify this
single-variable candidate. The client build still defines
`ENABLE_CHROMEHTMLWINDOW` on non-Orbis platforms, but the PS4 client omits it
after platform detection. The existing disabled branch sets `m_pHTMLMessage` to
`NULL`; its guarded URL/update paths remain compiled out, while the plain-text
`TextEntry` path and the v4.95 attribution breadcrumbs remain intact.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target reaches 100%, including its retained
  interface verification and SELF generation;
- generated PS4 client flags contain `ENABLE_STUDIO_SOFTBODY` and do not contain
  `ENABLE_CHROMEHTMLWINDOW`;
- the executable is 126,552,912 bytes with SHA-256
  `9c50494f0ac2e0693919510944ee05986177969f26751dd5bee3a3749be63f55`;
- the OELF is 136,332,640 bytes with SHA-256
  `2d2695751b5b49f3dc325c6b1e43c90691e84bd22a903c34d95a9be99a9f2dda`;
- the SELF input is 83,397,872 bytes with SHA-256
  `8ea7536361f197d041a1e06b36c762d853b0a0460c0b60637f7fdc0d3982f9b7`;
- binary strings contain the v4.96 marker, `text window constructor html
  disabled`, and `text window constructor complete`; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires the disabled-HTML and constructor-completion
markers, followed by successful `info` panel allocation and viewport insertion.
Any later crash must be attributed from the next final breadcrumb; this
candidate does not authorize renderer expansion before that boundary is known.

Candidate commit `8bf3cba8` produces a 103,481,344-byte package with SHA-256
`b8d4169510a4d5d39f4df9437938dcf0ed56147b11c12756697f9fa21e80aae4`.
The embedded SFO reports `APP_VER` and `VERSION` 3.62. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-16 23:35:22 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance remains pending manual
installation and launch.

### v4.97 candidate: attribute client-mode initialization

Package version 3.63 and build marker `client_mode_init_v497` identify this
attribution-only candidate. It does not skip a listener, guard the camera
singleton, or alter message-hook behavior. PS4-only breadcrumbs distinguish
normal from fullscreen client-mode initialization and bracket every remaining
`C_HLTVCamera::Init` listener, reset, state, and convar operation. They continue
through the shared VGUI message hooks and the derived CS mode's match-framework
subscription, event/message registrations, user-message binds, HUD render
groups, color-correction/post-process setup, and screenshot setup.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,552,912 bytes with SHA-256
  `fb986aca9bae2903daeddd5f116f226403115e34b43307daad925e2c34012477`;
- the OELF is 136,332,640 bytes with SHA-256
  `83f9c309d823a923f401edcee9ad2c20f88bbede43010016be1b5a804606dc46`;
- the SELF input is 83,397,872 bytes with SHA-256
  `16cc493324918d24f5c6144c04c6c537e9dd298df21602ce349a8abd51959c70`;
- binary strings contain the v4.97 marker, both normal/fullscreen caller tags,
  all three post-`hltv_status` listener boundaries, camera completion, both
  shared message-hook completions, and the derived-mode tail boundaries; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance is attribution, not necessarily a successful boot. The
last completed before/after pair must identify whether the failure is in the
camera tail, a shared message hook, the normal derived mode, or the second
fullscreen pass. Only then may the next candidate change lifecycle behavior.

Candidate commit `328fe8a2` produces a 103,481,344-byte package with SHA-256
`dd41b07110cdfacf83bf8a2efca96123eb43af0a01f8fd5fef5e0edaa3926152`.
The embedded SFO reports `APP_VER` and `VERSION` 3.63. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 01:13:48 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance remains pending manual
installation and launch.

### v4.98 candidate: defer PS4 color-correction lookup allocation

Package version 3.64 and build marker `defer_color_correction_v498` identify
this single-behavior candidate. On PS4, `ClientModeCSNormal::Init` leaves the
four constructor-initialized color-correction handles invalid instead of
creating and downloading 32x32x32 procedural lookup textures. Existing runtime
weight setters already ignore invalid handles. Non-PS4 behavior is unchanged,
and post-process parameter loading remains active and bracketed.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,552,912 bytes with SHA-256
  `dc175b68b2b1b21556db00d98a118a202ac62c444142902e9b7778966017489e`;
- the OELF is 136,332,640 bytes with SHA-256
  `e70495d5828a724f5b29a6d393b7e68e18fc85b7df7dec9eb4e831ab9bf617e6`;
- the SELF input is 83,397,872 bytes with SHA-256
  `fb796d7327d4bf6723480b417c33166047d0a609160210d0c989921098de758e`;
- binary strings contain the v4.98 marker, color-correction defer/completion
  markers, active post-process before/after markers, and both normal/fullscreen
  caller tags; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires the defer and color-correction completion markers.
If post-process loading or a later normal/fullscreen stage fails, attribute that
new boundary independently rather than expanding this bypass.

Candidate commit `cf682ee2` produces a 103,481,344-byte package with SHA-256
`7956b86934f820efbe8db382c4c08bfde3e3add32bb8bb0df0d7a153d442516a`.
The embedded SFO reports `APP_VER` and `VERSION` 3.64. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 03:19:35 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance completed: both client
modes return, including active post-process loading, and startup reaches
`IGameSystem::InitAllSystems()`.

### v4.99 candidate: trace client game-system initialization

Package version 3.65 and build marker `trace_client_game_systems_v499` identify
this attribution-only candidate. `IGameSystem::InitAllSystems()` now exposes its
existing PS4 list-walk, model-cache-lock, per-system `Init()`, and failure-unwind
breadcrumbs in both the client and server copies. The role, system name, index,
and list count/return value are retained; registration order and lifecycle
behavior are unchanged.

The independent ACP audit and local source inspection agree that this is the
smallest discriminating test. The explicitly added client systems run first,
followed by linker-ordered auto systems. The last `before init` without a paired
`after init` will identify a hard fault; a paired false result plus unwind will
identify a normal initialization rejection. The pre-existing per-frame-list
clear typo is deliberately left for a separate behavior change.

Validation before the candidate commit:

- `git diff --check` passes;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,552,968 bytes with SHA-256
  `b848a4bbb6b300cf5412326bf728978c416a93ffa295961dcf887391afc70ac9`;
- the OELF is 136,332,696 bytes with SHA-256
  `86e239fac7f9c5139ccb9903c1ef3df82d3d2a6a5be33c7943fd38df980e6762`;
- the SELF input is 83,397,872 bytes with SHA-256
  `6d76379978968bd3fa01edf1cc771d3e7265cd34806d8d7df2377fcc48713787`;
- binary strings contain the v4.99 marker, client entry marker, role-aware trace
  format, and every registration/init phase; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires the final trace to name the exact failing client
game system and whether the fault occurs in list construction, model-cache lock,
or the system's `Init()` body. No system is bypassed by this candidate.

Candidate commit `cc76c903` produces a 103,481,344-byte package with SHA-256
`a4ff1c2632510ccf69fce456d65efac87b9d784b6371e93e33a0be3cffb8efd9`.
The embedded SFO reports `APP_VER` and `VERSION` 3.65. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 03:32:24 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance completed: the probe
identifies a hard fault inside client game-system index 28, auto-list entry 15,
whose inherited name is `unnamed`.

### v5.00 candidate: trace inventory-manager initialization

Package version 3.66 and build marker `trace_inventory_manager_init_v500`
identify this attribution-only candidate. `CInventoryManager` now supplies its
stable class name to `CAutoGameSystem`; PS4 client breadcrumbs bracket entry,
each of the `CEconItem`, default-equipped, game-account, and coupon shared-object
registrations, generated-icon cache-directory creation, and normal completion.
Registration and filesystem behavior are unchanged.

The stable runtime name is authoritative over the earlier constructor-order
inference: `CInventoryManager` is hardware auto-list entry 14. Its source
`Init()` body remains the boundary under test, so the final paired markers
separate a `CSharedObject::RegisterFactory` fault from a filesystem fault.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,552,968 bytes with SHA-256
  `a6d047b33deca376498f102788b44fd7e37cd97e3905d50e7223c14ed7b9fa2e`;
- the OELF is 136,332,696 bytes with SHA-256
  `c20f98ed3c6829368c2f19b889fdc7a926c80560c6be446dc39d480aaec3475f`;
- the SELF input is 83,397,872 bytes with SHA-256
  `30334f5ff6533b5941b1e4570425efdee940d2a6ffbec27edbff4e5851986b0d`;
- binary strings contain the v5.00 marker, inventory-manager entry marker, and
  the final cache-directory completion marker; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires a last `before` marker that names the exact
operation, or full inventory-manager completion followed by the existing
per-system `after init` marker. No registration or filesystem operation is
bypassed by this candidate.

Candidate commit `0b6d8525` produces a 103,481,344-byte package with SHA-256
`846c1fb7d60cb3d62fd391ba510ba75b2ccc94e4884ac87e2b4539dceb86f2e5`.
The embedded SFO reports `APP_VER` and `VERSION` 3.66. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 03:51:00 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance completed: all inventory
registrations and filesystem work return successfully, and the crash is in the
next unnamed client auto system at index 28, auto-list entry 15.

### v5.01 candidate: trace competitive-ConVar callbacks

Package version 3.67 and build marker `trace_competitive_cvars_v501` identify
this attribution-only candidate. Final-binary symbols and constructor order make
`CCompetitiveCvarManager` the next candidate after `CInventoryManager`; its
constructor now supplies a stable game-system name so hardware can confirm that
identity directly. Its `Init()` retains the original callback-install loop while
emitting the list count and paired before/after breadcrumbs for every ConVar.
The macro-generated objects expose their compile-time ConVar token solely for
those stable diagnostic names.

This probe tests the concrete risk in the source: every callback installation
casts `ConVarRef(name).GetLinkedConVar()` and dereferences it. A missing or
foreign-owned monolithic ConVar can therefore fault before returning. The last
paired breadcrumb will identify the exact token without bypassing callback
installation or treating a desktop ConVar requirement as immutable.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,553,896 bytes with SHA-256
  `c04d0830d9033640af4101810f68dd82dbb9a3b5c6f85e80aab7832b8b3b0bb1`;
- the OELF is 136,333,912 bytes with SHA-256
  `9449f1c34f0952b38fb17ba3fca8b62761e9769f80229cc86317411112e0b807`;
- the SELF input is 83,398,160 bytes with SHA-256
  `d1e0472e673df66ece9458bef5fda49f97fefbd1b133b0aa5b682f2004e82ced`;
- binary strings contain the v5.01 marker, stable manager name, and per-ConVar
  trace format; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires index 28 to report `CCompetitiveCvarManager` and
either name the exact callback that faults or complete the manager and advance
to the next system. No competitive callback is bypassed by this candidate.

Candidate commit `c663b8a2` produces a 103,481,344-byte package with SHA-256
`4a3d15cefe79bd3988a204e14ea52dbe4a5577cfef5040bdc75f519471f136b9`.
The embedded SFO reports `APP_VER` and `VERSION` 3.67. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 04:02:27 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance completed: index 28 is
`CCompetitiveCvarManager`, its list has 11 entries, and the exact final marker
is `competitive cvar before callback index=0 name=fps_max`. No paired callback
completion marker is emitted.

The v5.01 log was last modified at 2026-07-17 04:05:35 UTC; it is 543,244 bytes
and 13,509 lines with SHA-256
`808f77e57ef8c1765b368af94ae8ca451711113d5421f93e2f5c7cc13f3005b5`.
The remaining ambiguity is inside the generated `fps_max` installation method:
`ConVarRef` lookup, callback-vector insertion, and the default immediate callback
invocation still share one trace boundary. Because ConVar lookup is a shared
monolithic contract, v5.02 must distinguish those phases before choosing a
PS4-only bypass or registry repair.

### v5.02 candidate: trace `fps_max` lookup and immediate callback

Package version 3.68 and build marker `trace_fps_max_callback_v502` identify
this attribution-only candidate. The generated competitive-ConVar method now
brackets `ConVarRef` construction, validity, linked-object access, and callback
installation. The generated callback brackets its `sv_competitive_minspec`
read and enforcement call; `EnforceCompetitiveCVar` brackets lookup, value read,
and both clamped and unchanged writes. Original callback insertion and default
immediate invocation remain enabled.

This is the smallest probe that separates a shared monolithic registry failure
from optional desktop competitive enforcement. A successful lookup followed by
callback entry proves registry ownership is sufficient at this boundary; a
fault before callback entry instead keeps callback-vector/ConVar object state
as the active architecture issue.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,570,280 bytes with SHA-256
  `357763781e1603b2b7c43b0ee763ba832c372a04f89de733994209099393d129`;
- the OELF is 136,350,296 bytes with SHA-256
  `820efaf3d8376379a6c92f75076c6c1f77d66245ebea3b4df7aa34f55dd8b530`;
- the SELF input is 83,414,576 bytes with SHA-256
  `f3e572ffb28b9b2db77a6fb08924df253028e1cc6f90e74c9bc05663a15cbe9b`;
- binary strings contain the v5.02 marker and lookup, minspec-read, and
  enforcement-write phase markers; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires a last marker that assigns the fault to lookup,
linked-object access, callback insertion, immediate invocation, minspec state,
or enforcement read/write. No callback is skipped by this candidate.

Candidate commit `dab1a35f` produces a 103,481,344-byte package with SHA-256
`a58ff7b2809d893ddcae0a70c8b46bf94dfb3926c67e8c2f91dc1f01562203ac`.
The embedded SFO reports `APP_VER` and `VERSION` 3.68. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 04:12:21 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance completed: after the
outer `fps_max` callback marker, the exact final marker is
`competitive cvar install before lookup index=-1 name=fps_max`. No lookup
validity marker is emitted, placing the hard fault inside the `ConVarRef`
constructor before `g_pCVar->FindVar("fps_max")` returns.

The v5.02 log was last modified at 2026-07-17 04:30:31 UTC; it is 543,315 bytes
and 13,510 lines with SHA-256
`4efb2c319bea9ab97ce8e7fc678d94ce63caf652706a1e975b7bad0caecb0505`.
Earlier startup proves `CCvar::Connect` assigned `g_pCVar` to the live `CCvar`
singleton and completed `ConVar_Register`. The next gate must therefore separate
virtual dispatch, `CCvar::FindVar`, VProf instrumentation, and
`m_CommandHash.FindPtr`; skipping the competitive manager would conceal a
shared registry defect required by later Source systems.

### v5.03 candidate: trace the shared ConVar registry lookup

Package version 3.69 and build marker `trace_cvar_find_v503` identify this
attribution-only candidate. `ConVarRef::Init` now brackets its `g_pCVar` branch
and virtual `FindVar` call. The non-const `CCvar::FindVar` path brackets entry,
`FindCommandBase`, and completion; `FindCommandBase` separately brackets VProf
and `m_CommandHash.FindPtr`. Lookup behavior and return values are unchanged.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and generates the SELF;
- the executable is 126,570,280 bytes with SHA-256
  `0be4abe5def8704b650b1c73c322c8d42c0a98b3b13b4c3660553ed7b54ca05a`;
- the OELF is 136,350,296 bytes with SHA-256
  `02ce5b3f44c2e0340a9e45d3abad48d32fae30a29239ea7dd77a49579e9b6000`;
- the SELF input is 83,414,576 bytes with SHA-256
  `e88afd384533b7fd52545e5d37a3a7e6600e2563f33f9334cdcb56428f4c3fc6`;
- binary strings contain the v5.03 marker and both sides of the virtual call and
  command-hash lookup; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance required a last marker that selected virtual dispatch,
VProf, or the command hash as the failing boundary. No alternate lookup or
registry bypass was introduced by this candidate.

Candidate commit `c8bf8f75` produces a 103,481,344-byte package with SHA-256
`bf4c420de4e9962f7eaf32ed952ad7ca278e3543c002c836fd342fdbd587c4a8`.
The embedded SFO reports `APP_VER` and `VERSION` 3.69. PkgTool validation has no
`[FAIL]` entries and reports every digest/signature check `[OK]`. FTP metadata
reports the same 103,481,344-byte size and a 2026-07-17 04:38:58 UTC modified
time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. The Linux
package script now derives the extracted Scaleform root from the repository
parent, replacing the stale `/home/bizkut/CSGO` default encountered during this
build.

Hardware acceptance completed with a fresh 969,556-byte, 23,465-line log,
last modified at 2026-07-17 04:43:27 UTC and having SHA-256
`a129c350dbb00059b13a349df51cf8f0d8fb7f4b634e8aaf071146e42886be1a`.
The `fps_max` lookup enters `CCvar::FindVar`, completes VProf entry/exit and
`m_CommandHash.FindPtr`, returns from `FindCommandBase`, returns from
`FindVar`, and reaches `ConVarRef::Init`'s `after FindVar` marker. It does not
emit the non-null `FindVar complete` marker or `convar ref init complete`.
Therefore the hash lookup safely returns null and the hard fault is in the
missing-ConVar fallback, most narrowly its warning/fallback block; it is not a
virtual-dispatch, VProf, hash-table, or `IsCommand` failure.

The same log contains no initial registration entry for `fps_max`, while all
later module-level `ConVar_Register` calls report `skipped already registered`.
The final OELF retains one constructed `fps_max` object and its constructor in
`.kisak_ctors`, so the next repair targets monolithic static-ConVar composition
instead of weakening `CCompetitiveCvarManager` or substituting a lookup path.

### v5.04 candidate: register from a PS4 construction manifest

Package version 3.70 and build marker
`convar_construction_manifest_v504` identify this composition repair. The
hardware log and final binary prove that the `sys_engine.cpp` constructor runs,
but `fps_max`, `engine_no_focus_sleep`, `mat_powersavingsmode`, and
`sleep_when_meeting_framerate` are all absent from the first registration
traversal. Re-running `ConVar_Register` cannot reconstruct a chain that was
already lost: after the shared accessor exists, genuinely late ConVars register
immediately from `ConCommandBase::Create`.

The PS4 tier1 path now records every unique constructed `ConCommandBase` pointer
in a fixed-capacity, zero-initialized side manifest. Duplicate constructors for
the same monolithic address are deduplicated. The first registration traverses
this manifest in reverse construction order, matching the original pending-list
ordering without depending on mutable `m_pNext` links; desktop behavior remains
unchanged. The existing `CCvar::RegisterConCommand` duplicate handling, hash,
lookup, flags, split-screen expansion, and queued material-thread processing are
preserved.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and `create-fself` completes;
- the executable is 126,570,488 bytes with SHA-256
  `b2dbaf952e5315bc2b919995982fcc601baf69036311ca41410d73eae1554d88`;
- the OELF is 136,350,504 bytes with SHA-256
  `0a1a93264ca093747ec1b68615f24e33dc37d6678c1f861b37d48492013c018a`;
- the SELF input is 83,414,576 bytes with SHA-256
  `05b6d541e57bbf79bcdd3fdb538f9f1dea6add84e1abf7792fd11aaa472954c8`;
- binary strings contain the v5.04 marker, construction-manifest selection, and
  bounded overflow marker; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires the initial registration output to contain
`fps_max`, the competitive manager to resolve each required ConVar and finish
callback installation, and client game-system initialization to advance beyond
index 28. A manifest overflow or another missing `sys_engine.cpp` neighbor is a
candidate failure, not permission to bypass the manager.

Candidate commit `6ad1c731` produces a 103,481,344-byte package with SHA-256
`451da200b20032c71af79a1ec5c8984fbec4c6f8d37176bf4e087fd265df7930`.
The embedded SFO reports `APP_VER` and `VERSION` 3.70. A separate verbose
PkgTool validation reports every limit, digest, and signature check `[OK]` and
no `[FAIL]` entry. FTP metadata reports the same size and a
2026-07-17 05:00:23 UTC modified time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance is pending manual
installation and launch.

Hardware acceptance completed with an 898,490-byte, 23,937-line log, last
modified at 2026-07-17 05:30:57 UTC and having SHA-256
`c53beb47e4d22cc704b2f57520feb5f3b654d02161c208b5ba991eac2f978781`.
The duplicate passes child/parent type dispatch, a live `ICvarQuery`,
`AreConVarsLinkable`, parent assignment, and flag absorption. The exact final
marker is `cvar register before callback transfer`.

Final-OELF inspection identifies the corruption rather than a callback-policy
failure. `sv_noclipduringpause` has one 144-byte ConVar object and one 8-byte
object. The 8-byte definition is the engine's `ConVar *` cache in
`engine/sys_dll.cpp`; its unmangled variable name collides with the client
ConVar in `game/client/in_main.cpp` under global
`--allow-multiple-definition`. The client constructor therefore writes a full
ConVar over pointer-sized engine storage and adjacent BSS. The server-isolated
ConVar remains correctly sized. v5.06 must rename the engine cache and all of
its engine references, then verify two distinct 144-byte client/server ConVars
plus one uniquely named 8-byte engine pointer in the final OELF.

Hardware acceptance completed with an 858,333-byte, 23,012-line log, last
modified at 2026-07-17 05:02:59 UTC and having SHA-256
`28d3ff23072b8a4c5aa1a274829a3f838037e0dbc16779151c4592853395bbf9`.
The construction-manifest marker appears and there is no overflow. The initial
registration now contains `sleep_when_meeting_framerate`,
`mat_powersavingsmode`, `fps_max`, and `engine_no_focus_sleep`, proving the lost
`sys_engine.cpp` segment is restored.

The new final item is the second `sv_noclipduringpause` registration. The first
copy registers at log line 6,561; the client/server peer reaches registration at
line 23,007, completes the initial `FindCommandBase` VProf and hash lookup, and
then faults before the next manifest item. Source defines matching replicated
cheat ConVars in `game/server/player.cpp` and `game/client/in_main.cpp`, so this
is a duplicate-parent integration failure inside `CCvar::RegisterConCommand`,
not a missing registrar. The v5.05 probe must bracket command/type checks,
`ICvarQuery::AreConVarsLinkable`, parent assignment, callback transfer, and
diagnostic-warning branches before changing duplicate semantics.

### v5.05 candidate: attribute duplicate-ConVar linking

Package version 3.71 and build marker `trace_duplicate_convar_v505` identify
this attribution-only candidate. The duplicate path in
`CCvar::RegisterConCommand` now brackets child and parent type dispatch,
`ICvarQuery` presence and `AreConVarsLinkable`, parent assignment, flag
absorption, callback transfer, help-string comparison, flag comparison, and
each diagnostic warning. The original short-circuit ordering for `IsCommand`
is preserved, and no duplicate-parent or warning behavior is bypassed.

Validation before the candidate commit:

- `git diff --check` and both packaging-script syntax checks pass;
- the full `kisak_ps4_monolithic` target links and `create-fself` completes;
- the executable is 126,570,488 bytes with SHA-256
  `e09520b1401ac7b39da35a6bf51cd32a5faca0704d757e3e87c72275b6f7a92e`;
- the OELF is 136,350,504 bytes with SHA-256
  `da629ab03315b0d99f427c5ee97c16f30cb3a496528202c307c1cb3795e62465`;
- the SELF input is 83,414,576 bytes with SHA-256
  `6e311ee5b28e801f59371c870f2f5cf4bba9357f3ce1f93e369b9167c936b2eb`;
- binary strings contain the v5.05 marker and every duplicate-link attribution
  boundary; and
- the host suite remains 11/14, with only the three documented Linux
  high-address OpenGNM fixture failures (`ps4_gnm_device`, `ps4_gnm_buffer`, and
  `ps4_gnm_constants`).

Hardware acceptance requires a final marker within the second
`sv_noclipduringpause` duplicate path. If a `before ... warning` marker is last,
repair PS4 diagnostic output before changing duplicate ownership. If parent or
callback state fails first, validate both objects' final-binary identity and
construction state before choosing the monolithic owner.

Candidate commit `84dd5c34` produces a 103,481,344-byte package with SHA-256
`35325810ef119e50a2ae89c83a218f32753db1831706dfca2b16d28f71d4fe6c`.
The embedded SFO reports `APP_VER` and `VERSION` 3.71. A separate verbose
PkgTool validation reports every limit, digest, and signature check `[OK]` and
no `[FAIL]` entry. FTP metadata reports the same size and a
2026-07-17 05:07:56 UTC modified time. The package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`; two complete remote
readbacks match the local SHA-256. Hardware acceptance is pending manual
installation and launch.

## Runtime topology: two tracks, one production authority

`KisakRegisterStaticModules` registers both tracks in
`appframework/Ps4StaticModules.cpp:54-83`:

| Registered runtime | Factory | Purpose | Acceptance status |
|---|---|---|---|
| `presentation_engine` | `KisakEngineBootstrapFactory()` | Diagnostic OpenGNM/Scaleform/input loop and rollback target | Hardware validated, but not a production Source-frame result |
| `engine` and `source_engine` | `KisakSourceEngineFactory()` | Real Source app systems, server, client, host, and frame lifecycle | Hardware completes both client modes and game systems 0-27, then hard-faults in `CCompetitiveCvarManager` while installing the `fps_max` callback; no first frame yet |

The launcher requests production `engine` (`launcher/launcher.cpp:773-820`).
The presentation loop owns VideoOut and calls `RenderMenuFrame` and
`RenderHUDFrame` directly (`ps4/engine_bootstrap.cpp:40-54,137-207`). The
production loop consumes `eng->Frame()` in
`engine/sys_dll2.cpp:1200-1259`. A presentation feature is not complete until
the production loop reaches and renders it on hardware.

The renderer-selection contract is currently broken before the wrapper can be
used. `KisakMaterialSystemFactory()` preselects `shaderapiempty` during static
module registration (`materialsystem/cmaterialsystem.cpp:647-653`). The launcher
later requests `shaderapips4` (`launcher/launcher.cpp:921-940`), but
`CMaterialSystem::SetShaderAPI` rejects that second request after its factory is
set (`materialsystem/cmaterialsystem.cpp:793-797`). Thus `shaderapips4` is linked
and registered but is not the selected production backend.

Even after that ordering defect is fixed, the backend is intentionally partial:
the PS4 device manager/device and shadow-state wrapper exist, but there is no
native PS4 `IShaderAPI` implementation, `CShaderDevicePs4::Present()` delegates
to `shaderapiempty`, and production code owns no GNM submission or VideoOut
flip. Driver submission is confined to `ps4/gnm_submission_bootstrap.cpp`, and
VideoOut open/flip is confined to `ps4/videoout_bootstrap.cpp`. PS4 also forces
both `USE_SCALEFORM` and `USE_ROCKETUI` off
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
| OpenGNM command, memory, descriptor, shader-binary, VideoOut, and driver primitives | Yes | Diagnostic bootstrap only | Presentation only | Production Source owns one begin/submit/flip cycle |
| `shaderapips4` backend selection | Backend is linked and registered | No: `shaderapiempty` is selected first and the later PS4 request is rejected | No | Exactly one PS4 backend selection before material-system Connect; no silent empty fallback |
| Native PS4 `IShaderAPI` and production present | Missing; device/shadow wrappers are partial | No | No | Source frame emits a known draw, submits it, and presents it |
| PS4 vertex/index buffers, mesh locks, descriptors, and draw-state translation | Partial | Partial; several paths still delegate to empty | Host/presentation evidence only | One static and one dynamic Source mesh use native buffers in a presented frame |
| PS4 textures and render targets | 2D texture and color-target views are partial; no Source depth-target lifecycle | No end-to-end Source ownership | Presentation only | Upload one encountered VTF, sample it, and render with a native depth target |
| Offline Source-to-PS4 shader build/package pipeline | Missing; binary parser, handle manifest, and diagnostic binaries exist | No Source material binaries | Diagnostic binaries only | Reproducibly build, package, load, and bind the first restricted Source shader |
| Source world/model materials | Source paths exist; PS4 material shader combinations are missing | Not end-to-end | No | Present `LightmappedGeneric`, `VertexLitGeneric`, and `UnlitGeneric` in one `cs_office` frame |
| Source particle rendering | `.pcf` parsing, simulation, and dynamic-mesh generation exist; PS4 particle shaders are missing | Loading path reached; native render path incomplete | No | Present one translucent `SpriteCard` effect from a Source frame |
| Scaleform Legals -> StartScreen -> MainMenu, movies, vector/image/text rendering | Yes | No: Source Scaleform is disabled and the PS4 bootstrap uses a separate interface | Presentation only | Same boot/menu sequence from Source frames |
| DualShock polling and Source `InputEvent_t` translation | Yes | Yes in code | Presentation UI only | Navigate Source-owned menu and drive a user command |
| Main-menu action routing | Partial: 1 of about 20 | Diagnostic only | Offline-dialog action in presentation | Complete actions needed for boot, options, offline launch, pause, and quit |
| Source server/client startup | Yes, still being closed | Yes | Server DLL init and scoreboard insertion; current crash is in `CTextWindow`/`info` construction | Info panel + ClientDLL Init + EngineVGui + first `eng->Frame()` |
| Offline request parser and queue | Yes | Split: diagnostic menu produces it; the production MainLoop consumer is compiled but not reached | Presentation producer only | Add a Source-owned producer, then hardware-prove the consumer through `Host_NewGame` |
| Listen server, local client, spawn, input, world frame | Existing Source path, not PS4 accepted | Not end-to-end | No | One controllable player and one presented `cs_office` frame |
| Audio | Source mixer and desktop OpenAL sink exist; native PS4 sink is missing | No accepted `SceAudioOut` submission path | No | Submit mixed interleaved PCM through PS4 AudioOut in an offline frame |
| Fonts | FreeType exists; desktop selection depends on Fontconfig | Bundled-font PS4 selection pending | Presentation text only | Load the required UI fonts from packaged assets without Fontconfig/system-font dependence |

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

Immediate v5.06 hardware gate: give the engine's cached
`sv_noclipduringpause` pointer a unique symbol, prove the final OELF no longer
aliases pointer and ConVar storage, and let the normal client/server duplicate
link complete. v5.04 already proves the independent construction manifest
restores `fps_max` and its `sys_engine.cpp` neighbors; retain that repair and do
not skip either the shared registry or `CCompetitiveCvarManager`.

Phase hardware gate: preflight passes, every default viewport panel completes,
`ClientDLL_Init` completes, EngineVGui/GameUI connect, and the first real Source
frame completes. This phase is not done merely because a registrar is found.

### 2. Direct Source/OpenGNM convergence

Keep `presentation_engine` as a diagnostic rollback target, not a second
product runtime. Do not begin the renderer-selection change until the v5.04
ConVar-composition gate closes the current Source-lifecycle blocker:
changing the active backend is broad and would destroy the current crash
boundary.

#### Source/OpenGNM rendering closure checklist

Complete these gates in order, committing and hardware-attributing each broad
behavior change separately:

1. **Backend-selection contract:** remove the eager `shaderapiempty` selection
   or otherwise select `shaderapips4` exactly once before material-system
   Connect. Fail startup if the required PS4 interfaces are absent; never
   silently fall back to the empty backend. Add a focused test for first/second
   selection and log the selected factory/interface cardinality.
2. **Production frame ownership:** move command-buffer begin/end, fence/label
   synchronization, VideoOut backbuffer acquisition, driver submission, flip,
   and present into the Source ShaderAPI/device lifecycle. First use one known
   diagnostic PS4 shader binary and a fixed triangle/clear to separate frame
   ownership from Source shader breadth.
3. **Offline shader artifact pipeline:** make a reproducible build step that
   compiles one restricted Source shader to PS4 GCN code plus metadata, validates
   it, writes a deterministic manifest entry, packages it, and lets the existing
   binary parser load it. Prove a solid shader, then `UnlitGeneric`/UI texture;
   OpenGNM parsing and fetch-shader generation are consumers, not compilation.
4. **Native `IShaderAPI` slice:** implement the PS4 interface instead of
   delegating to `shaderapiempty`. Close the matrices, vertex/pixel constants,
   texture/sampler bindings, primitive topology, viewport/scissor, and
   blend/depth/stencil/cull/color-write state needed by the first shader. Cache
   Source state and emit only dirty GNM state immediately before draws; compile
   alpha test into relevant shader combinations.
5. **Source buffer semantics and mesh submission:** finish static/dynamic
   vertex and index buffers, discard/no-overwrite behavior, a per-frame ring
   allocator, CPU-visible versus GPU-optimal memory policy, alignment/cache
   flushes, vertex declarations/fetch shaders, primitive conversion, and
   indexed/non-indexed draws. Prove one static and one dynamic Source mesh in a
   single presented frame.
6. **Textures and targets:** add an explicit Source/VTF-to-GNM format table
   covering the formats actually encountered first (`A8R8G8B8`, BC1/2/3,
   required float formats, and depth/stencil), including swizzles, sRGB, mip and
   cube layout, linear/tiled policy, uploads/staging, sampler state, color/depth
   targets, resolves, cache barriers, and bounded readback fallbacks.
7. **Restricted material families:** add combinations in observed map-load
   order: `LightmappedGeneric`, `VertexLitGeneric`, `UnlitGeneric`, depth/basic
   shadow, sky, and then `SpriteCard`/`ParticleLitGeneric`. Include only required
   lightmap, fog, skinning, alpha-test, translucent, sheet-data, and dynamic-mesh
   variants. Add decals, overlays, projected textures, water, and other scene
   effects only when the first match reaches them.
8. **Production UI bridge:** after repeated Source-owned frames work, either
   finish the normal `ScaleformUI002` PS4 app-system/HAL and enable its Source
   frame hook, or adapt the focused `Ps4ScaleformUI001` manager to Source-owned
   UI/render phases. Preserve existing DualShock polling and GameUI dispatch
   (`engine/sys_dll2.cpp:1077-1084`, `engine/sys_mainwind.cpp:403-506`); prove it
   drives both the Source-owned menu and a gameplay user command.
9. **Acceptance accounting:** record every backend slice and shader/material
   family independently as implemented, Source-integrated, and
   hardware-accepted. A diagnostic draw, an empty-backend fallback, or a loaded
   binary that is never submitted is not integration evidence.

The minimum renderer integration gate is a repeatedly presented Source-owned
frame containing a native shader, constants/state, a texture, and both static
and dynamic mesh work with no required-path delegation to `shaderapiempty`. The
first-map renderer gate adds recognizable `cs_office` world geometry, a model,
and a translucent particle.

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
3. keep Source decoding/mixing on the CPU and add a native PS4 AudioOut sink for
   final interleaved PCM; do not make a full OpenAL port, HRTF, or advanced
   voice handling a first-match prerequisite;
4. use packaged fonts through FreeType when Source UI reaches font selection;
   do not require Fontconfig or undocumented PS4 system fonts;
5. run a combined render/input/UI/audio soak;
6. broaden maps and offline modes;
7. enable multiplayer only after offline acceptance.

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
