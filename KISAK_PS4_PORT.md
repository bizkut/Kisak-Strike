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

Latest staged monolithic package:

```text
Package: IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg
Version: 2.27
SHA-256: 59f381c968eabb0225932e04b4498812b08dd9f225ebee5835a95b341959b33a
Staged:  /data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg
```

Current hardware baseline:

- The monolithic executable reaches `LauncherMain`, initializes the registered
  Source app systems, runs RocketUI frame hooks, and shuts down cleanly.
- OpenGNM opens two 1920x1080 VideoOut buffers and completes repeated VSYNC
  flips. The v1.78 run sustains approximately 60 FPS without a crash.
- The presentation stress loop has reached at least frame 1200 with matching
  `before flip`/`flip complete` markers. The 1800-frame completion marker is the
  remaining confirmation for this bounded checkpoint.
- The current frame is a CPU-cleared buffer. `shaderapiempty` is still active;
  no Source draw call, PM4 draw packet, converted shader, texture, or world
  geometry reaches the GPU yet.
- External content mounting is hardware validated. A post-upload v1.80 launch
  read `gameinfo.txt`, the sound manifest, one VMT/VTF pair, and
  `maps/de_dust2.bsp` through the normal layered `GAME` search path. The sound
  manifest loaded successfully and the diagnostic run shut down cleanly after
  frame 1200.

Version 1.79 adds a host-tested PS4 content layout. It normalizes relative game
paths, rejects absolute and traversal inputs, mounts `/data/kisak-strike/csgo`
and `/data/kisak-strike` ahead of `/app0/csgo` and `/app0`, layers both
platform roots, and keeps writes under `/data/kisak-strike`. The next hardware
log will report either
`content gameinfo found` or `content gameinfo missing` through the normal
`GAME` search path.

Version 1.80 packages a non-proprietary sentinel at `/app0` and reads it
through the layered `GAME` search path. It also opens and reads, when supplied,
`gameinfo.txt`, the sound manifest, `materials/vgui/white.vmt`,
`materials/vgui/white.vtf`, and `maps/de_dust2.bsp`, with a distinct bounded
breadcrumb for every readable or missing asset. This validates package-root
mounting independently from user-content availability.

The 2026-07-11 post-upload hardware run validated the external half of that
layout as well. `/data/kisak-strike/csgo` contains `gameinfo.txt`,
`pak01_dir.vpk` and its numbered archives, while
`/data/kisak-strike/platform` contains `platform_pak01_dir.vpk` and its
numbered archives. All five representative loose-asset probes reported
`readable`, `sound manifest loaded` replaced the empty-table fallback, and the
launcher completed its bounded run without a crash. Content-path bring-up is
therefore complete; the next unresolved boundary is the minimum
OpenGNM-backed PS4 ShaderAPI/D3D9 draw path.

Version 1.81 introduces a distinct statically registered `shaderapips4`
module and makes it the PS4 launcher's default selection. Its interface factory
deliberately delegates to `shaderapiempty` while the first backend objects are
brought up, and the runtime log identifies that fallback explicitly. The new
`CPs4GnmDevice` and `CPs4GnmMemory` foundation provides aligned linear frame
allocation, two frames in flight, monotonically increasing submission labels,
and refusal to recycle an arena until its EOP label is complete. Host tests
cover invalid initialization, alignment, frame rotation, and premature reuse.
The monolithic OpenOrbis build links the new module successfully. Hardware
validation of module selection is the next checkpoint; no real draw is claimed
yet.

The v1.81 hardware run logged
`shaderapips4 selected with empty fallback`, kept all five external content
probes readable, reached at least frame 1200, and sustained the reported 60 FPS
without a crash. This validates PS4 module selection and preserves the stable
presentation/content baseline. The next checkpoint is to bind the two frame
arenas and their completion labels to real OpenGNM command submission; the
fallback remains active until a hardware clear and triangle pass.

Version 1.82 binds the frame-arena model to a bounded real OpenGNM submission
self-test before the presentation loop. It allocates and maps a separate 2 MiB
garlic direct-memory pool, emits default graphics state, a zero-count draw, and
a 64-bit EOP label write, then calls `sceGnmSubmitCommandBuffers` and
`sceGnmSubmitDone`. Three synchronous submissions exercise frame 0, frame 1,
and the first EOP-authorized reuse of frame 0. Allocation, mapping, submission,
and EOP timeout failures are logged and contained; the existing CPU-clear
VideoOut loop remains active. The mapped pool is released after the test. The
expected success marker is
`gnm submission two-frame EOP passed`.

The v1.82 hardware run produced that success marker with no allocation,
mapping, submission, `SubmitDone`, or EOP-timeout failure. All representative
content probes remained readable and the CPU-clear presentation loop sustained
the reported 60 FPS through at least frame 1200 without a crash. Real OpenGNM
command submission and GPU-completion visibility are therefore validated. The
next boundary is to retain these arenas for the presentation lifetime and emit
a GPU render-target clear into a VideoOut-compatible buffer.

Version 1.83 retains the two-frame command pool for the entire presentation
loop and replaces the per-frame CPU `memset` with an OpenGNM `DMA_DATA` fill of
the active linear VideoOut buffer. Every fill is followed by the existing
64-bit EOP write and bounded wait before flip; allocation, emission,
submission, or completion failure falls back to the CPU clear and logs once.
OpenGNM now exposes the generic bounded `sceGnmDrawCmdFillMemory` operation,
including packet-layout, invalid-argument, capacity, and multi-packet behavior
tests. The OpenGNM build wrapper was also corrected to run from its own source
root and select the native LLVM toolchain on macOS, preventing host objects
from contaminating PS4 archives. This is a GPU memory-clear checkpoint, not yet
a shader/render-target draw. Expected hardware success marker:
`GPU VideoOut clear and EOP passed`.

The v1.83 hardware run produced both the initial two-frame EOP marker and
`GPU VideoOut clear and EOP passed`, with no CPU-fallback, submission, timeout,
flip, or presentation failure. It completed all 1800 frames at the reported
60 FPS, released the submission pool, and returned cleanly. The visible output
was reported as grey rather than the intended dark packed color. GPU writes,
EOP ordering, scanout, and lifetime reuse are therefore validated, but color
interpretation is not. Before introducing the clear shader, emit distinct
primary-color regions to verify A8B8G8R8 byte order, pitch, and linear VideoOut
layout on hardware.

Version 1.84 replaces the ambiguous uniform fill with four equal horizontal
GPU-filled regions. The intended top-to-bottom order is red, green, blue, and
white under the documented A8B8G8R8 little-endian layout. All four fills share
one command buffer and one EOP completion label per frame. The CPU fallback and
bounded failure behavior remain unchanged. Expected success marker:
`GPU VideoOut RGBA bars and EOP passed`. Correct bands validate byte order,
pitch, and linear scanout; swapped colors isolate channel order, while broken
boundaries indicate a pitch or layout mismatch.

The v1.84 hardware run displayed all four horizontal color bars correctly and
logged `GPU VideoOut RGBA bars and EOP passed`. It then reached frame 1800,
released the GNM submission pool, closed VideoOut, and returned from
`LauncherMain` without a flip, presentation, EOP, or fallback failure. The
subsequent black screen is the expected result of the bounded diagnostic loop
ending while the outer bootstrap keeps the process alive; it is not a crash.
A8B8G8R8 byte order, 1920-pixel pitch, linear scanout, GPU fill, and EOP-before-
flip ordering are validated. The next runtime change should replace the fixed
1800-frame loop with a quit-aware engine loop that retains VideoOut and GPU
arenas until actual application shutdown.

Version 1.85 removes the fixed 1800-frame termination condition. The bootstrap
now retains VideoOut and the two-frame GPU submission pool while its engine
loop is running, and accepts `quit` or `exit` through `PostConsoleCommand` as a
clean shutdown request. Frame breadcrumbs retain the existing 60-frame detail
through frame 1200, then reduce to one marker every 3600 frames to bound log
growth during long soaks. No controller-specific quit shortcut is introduced;
that belongs to the real input/menu path. Hardware validation should show the
RGBA bars and 60 FPS continuing beyond frame 1800 with no `pool released`,
`videoout closed`, or `LauncherMain returned` marker during the active run.

The v1.85 hardware run retained the four correct GPU color bars at the reported
60 FPS through at least frame 3600. The log contains no CPU-fallback, flip,
presentation, or EOP failure and no `pool released`, `videoout closed`, or
`LauncherMain returned` marker. Persistent VideoOut ownership and long-lived
two-frame arena reuse are validated beyond the former 1800-frame boundary. The
next rendering checkpoint is a shader-driven fullscreen clear/triangle using
packaged `.sb` binaries and explicit binding metadata; the DMA bars remain the
diagnostic fallback.

Version 1.86 adds the first shader-driven draw over that fallback. Owned vertex
and fragment GLSL sources describe a three-vertex orange/yellow diagnostic
triangle. Package generation requires the corresponding externally generated
PS4 `.sb` files and copies them to `/app0`; generated binaries remain outside
source control. Runtime loading validates OpenGNM metadata and stage identity,
copies shader code into a reserved persistent 128 KiB GPU region, patches
program addresses, creates the fetch shader, binds a 1920x1080 linear
A8B8G8R8-sRGB render target, emits viewport/raster/depth state, and draws three
vertices over the DMA color bars before the existing EOP and flip. Shader-load
failure retains the bars and logs a bounded diagnostic. Expected success
markers are `diagnostic shader binaries loaded` and
`shader triangle over RGBA bars and EOP passed`; expected output is the four
bars with a centered warm-colored triangle.

The advertised `opengnm-psbc` build remains incomplete in its own repository,
so v1.86 consumes the already generated diagnostic binaries from the FreeGNM
example build. Replacing this temporary package input with a reproducible
Kisak-owned `glslc` plus `opengnm-psbc -4` build is still required before the
Source shader manifest milestone can be considered complete.

The v1.86 hardware run stayed stable at 60 FPS with the four DMA bars but no
triangle. Its log reported `diagnostic shader load failed; DMA bars retained`,
proving the draw path was never entered. The generated diagnostic binaries use
the legacy PS4 container form: a 0x24-byte PSSL wrapper precedes the `Shdr`
header. OpenGNM's strict metadata helper accepted only bare `Shdr` binaries at
offset zero. OpenGNM now detects and bounds-checks both bare and wrapped forms,
with a regression fixture in the seven-test helper suite. Version 1.87 rebuilds
against that parser fix; expected hardware behavior returns to the v1.86
triangle markers and visual target.

The next pulled log still contained the old generic load-failure marker and no
package identity, so it could not prove that the parser-fixed executable was
actually running. Version 1.89 adds build marker
`shader_loader_diag_v189` and a bounded `diagnostic shader detail` line. The
detail identifies file-read, wrapped-metadata, GPU-code allocation, fetch-size,
fetch-allocation, or fetch-creation failure and includes the relevant sizes and
result codes. A successful load reports the vertex, pixel, and fetch byte
counts. This turns the next hardware capture into a definitive package and
loader-stage check before further PM4 draw changes.

The v1.89 hardware run confirmed the new marker and isolated vertex metadata:
`result=2 type=1 stagebytes=44 filebytes=264`. OpenGNM was locating the wrapped
`Shdr` header but incorrectly using `headersizedwords` as the offset to the GNM
stage structure. That field is the stage-header size; the structure begins
immediately after the fixed 16-byte file header. The helper also used the
common size as executable length even though wrapped PSSL binaries include a
trailing `OrbShdr` record in that size. OpenGNM commit `a99926a` fixes both
layouts, reads the executable length from the bounded trailer, and updates the
wrapped-container regression test. Version 1.90 stages this fix. Its expected
next boundary is either `diagnostic shader detail ready ...` followed by the
triangle markers, or a precise fetch-shader failure after both programs load.

The v1.90 run remained stable at 60 FPS and confirmed both wrapped programs
load, then reported `fetch creation failed bytes=12`. The diagnostic vertex
shader is procedural and has zero vertex attributes. OpenGNM's size calculator
correctly reserved three DWORDs, but fetch creation entered its attribute-load
`do` loop once even with zero inputs and emitted a fourth DWORD. Commit
`6275f67` changes this to a guarded loop and adds an exact-size zero-input
regression test. Version 1.91 stages the fix; it should reach the first PM4
triangle submission rather than retaining the bars during shader setup.

The v1.91 hardware run reached `diagnostic shader detail ready`, loaded both
programs, completed the fetch shader, submitted the triangle command, and
observed its EOP label, while the display still showed only four bars. Version
1.92 aligns the diagnostic state with the proven FreeGNM triangle example: it
adds a CB0 acquire barrier after the DMA fills, hardware screen offset, and
full-screen guard bands. It also logs the center framebuffer pixel after EOP.
That readback will distinguish missing raster output from a VideoOut visibility
or cache-coherency problem instead of treating command completion as proof that
the triangle changed the render target.

The v1.92 readback was `0xffff0000`, exactly the untouched blue band at the
framebuffer center. Rasterization therefore produced no color despite clean
submission and EOP completion. The matching FreeGNM procedural-triangle
example binds the zero-input vertex shader directly and does not create,
modify registers for, or bind a fetch shader. Version 1.93 follows that path:
zero-input programs retain their compiler-provided VS registers and report
`fetchbytes=0 procedural=1`; fetch generation remains enabled for shaders with
real vertex attributes. The center-pixel probe remains active for validation.

The v1.93 hardware run validates the first complete OpenGNM graphics pipeline.
It remained stable at 60 FPS, displayed the four DMA bars with the centered
orange gradient triangle, and read back center pixel `0xff00bcff` instead of
the prior blue-band value `0xffff0000`. The log confirms wrapped VS/PS loading,
direct procedural VS binding, PM4 state emission, raster output, render-target
visibility, EOP synchronization, and repeated VideoOut flips. The bootstrap
triangle milestone is complete. Further renderer work should now move behind
the PS4 D3D9-compatible device boundary and use this path as its hardware
smoke test rather than expanding the standalone diagnostic renderer.

Version 1.94 begins that migration. `CPs4GnmDevice` now owns submission-frame
reservation as one transaction: selecting a recyclable frame, resetting its
arena, allocating aligned command and EOP-label storage, reserving the next
monotonic label, committing it after successful submission, and cancelling
cleanly on allocation or driver failure. The bootstrap self-test, fill, and
bars-plus-triangle paths all use this device API instead of duplicating the
lifetime rules. Host tests cover successful commit, label progression,
oversized allocation rollback, and frame-open state; the complete OpenOrbis
build also passes. Hardware v1.94 is a regression gate for the same visible
triangle before device-owned draw state is added.

The v1.94 hardware run passed that gate: it remained stable at 60 FPS, retained
the four bars and orange gradient triangle, completed the two-frame EOP test,
and read back center pixel `0xff00bcff`. Device-owned submission reservation,
commit, and recycling are therefore hardware validated. The next renderer unit
is `CPs4GnmDrawState`, initially covering viewport/scissor, rasterizer,
depth/stencil, render-target mask, shader bindings, and dirty-state emission.

Version 1.95 adds the first `CPs4GnmDrawState` implementation. It owns cached
viewport, scissor, viewport transform, primitive/rasterizer setup,
depth/stencil control, DB render control, and render-target write mask. Setters
mark only changed groups dirty, `Apply` emits those groups through OpenGNM, and
`BeginCommand` forces a complete state image at every new command-buffer
boundary. The validated triangle now uses this component instead of issuing
those PM4 state calls directly. The complete OpenOrbis build passes; hardware
must retain the v1.94 image and `0xff00bcff` readback before shader and resource
bindings move into the same cache.

The v1.95 hardware run validates the first cached draw-state groups. It stayed
stable at 60 FPS, displayed the expected bars and orange gradient triangle,
passed the two-frame EOP test, and retained center pixel `0xff00bcff`. Viewport,
scissor, viewport transform, rasterizer, depth/DB, and color-mask emission can
therefore remain behind `CPs4GnmDrawState`. Add render-target and VS/PS binding
next, followed by blend state and the first indexed vertex-buffer diagnostic.

Version 1.96 extends `CPs4GnmDrawState` with cached color-render-target,
vertex-shader, and pixel-shader register images. Target index and VS modifier
are part of cache identity, and every new command buffer still forces complete
emission. The procedural triangle now binds its target and both shader stages
through the cache; generated PS input semantic linkage remains explicit because
it is metadata-derived rather than a fixed state register group. The complete
OpenOrbis build passes. Hardware must preserve the visible triangle and center
readback before adding blend state and indexed geometry.

The v1.96 hardware run stayed stable at 60 FPS, retained the visible orange
triangle, passed EOP synchronization, and read back `0xff00bcff`, validating
cached target and VS/PS register binding. Version 1.97 adds render-target-indexed
blend control to `CPs4GnmDrawState` and routes the diagnostic's explicit
disabled-blend state through the cache. This is the no-regression baseline for
the subsequent alpha-blend and indexed-geometry diagnostics.

The v1.97 hardware run passed its no-regression gate at 60 FPS with the same
visible triangle and `0xff00bcff` center readback. Version 1.98 changes the
triangle from `DrawIndexAuto` to a real 16-bit index buffer allocated in the
current device frame arena. `CPs4GnmDrawState` now caches index size and cache
policy, and the command emits `DrawIndex` for indices `{0,1,2}`. Because the
procedural VS consumes `gl_VertexIndex`, this isolates index allocation,
binding, and indexed packet execution without yet introducing vertex-fetch
shader or descriptor variables. The next gate adds fetched vertex attributes.

The v1.98 indexed draw passed on hardware at 60 FPS with the same visible
triangle and `0xff00bcff` readback. Version 1.99 packages the already generated
`eden-renderer-draw` position/color shaders, whose triangle data exactly matches
the existing visual oracle. Runtime setup now allocates persistent interleaved
position/color vertices, builds two `GnmBuffer` descriptors, generates a fetch
shader for the reflected input semantics, patches VS resource registers, and
binds fetch and vertex-buffer tables at VS user-data slots 0 and 2. The indexed
draw remains unchanged, isolating fetched attributes and descriptor binding.

The v1.99 hardware run validates fetched attributes: it remained stable at 60
FPS, reported two vertex inputs and a 44-byte fetch shader, displayed the same
gradient triangle, and read center pixel `0x8000bcff`. The packaged fragment
shader deliberately emits alpha 0.5, so version 2.00 enables cached
source-alpha RGB blending over the DMA bars while preserving source alpha via a
separate alpha equation. The next readback must contain a mixture of the prior
orange source and blue destination rather than either unblended value.

The v2.00 hardware readback is `0x80bc89bc`, confirming blend-unit output from
the half-alpha triangle over the blue bar. Version 2.01 adds cached depth-target
bind and unbind state to `CPs4GnmDrawState` and routes the diagnostic through an
explicit unbound-depth baseline. This keeps the image unchanged while proving
the new PM4 state group before a tiled depth allocation and shader-based clear
are introduced for overlapping depth-tested geometry.

The v2.01 explicit depth-unbound baseline passed hardware validation at 60 FPS
with blended center pixel `0x80bc89bc`. Version 2.02 moves pointer user-data
bindings into `CPs4GnmDrawState`. The cache keys each pointer by shader stage
and starting SGPR slot, supports eight active bindings, re-emits all active
bindings at command-buffer boundaries, and rejects capacity overflow. The
current fetch shader and vertex-descriptor table at VS slots 0 and 2 no longer
bypass the state cache. PS semantic linkage is now the only direct shader-state
call remaining in the diagnostic path.

Version 2.03 replaces the inherited FreeGNM example icon with the user-supplied
512x512 RGB CS:GO logo at `ps4/sce_sys/icon0.png`. Packaging now defaults to
that tracked asset while retaining `KISAK_PS4_ICON_PATH` as an explicit build
override. Runtime graphics behavior is unchanged from v2.02; the hardware gate
is the same fetched, indexed, blended triangle plus the new icon in the PS4
launcher and package information views.

Version 2.04 moves PS input semantic linkage into `CPs4GnmDrawState`. The cache
tracks the VS export and PS input table identities and counts, invalidates when
the active shader pair changes, and re-emits the linkage at every command-buffer
boundary. The fetched, indexed, blended diagnostic now has no direct shader
state or user-data calls outside the cache. The CS:GO package icon remains the
v2.03 tracked asset; runtime rendering should remain unchanged.

Version 2.05 expands the diagnostic direct-memory pool from 2 MiB to 16 MiB,
reserving 10 MiB for persistent resources and 6 MiB for the two frame arenas.
It creates a 1920x1080 tiled Z32 depth target, calculates and aligns its memory,
sets read/write addresses, and binds it through cached state with depth testing
still disabled. This isolates depth allocation/layout/binding before clear and
writes. The build also removes synchronous `before flip`/`flip complete` file
logging from every frame: those markers now cover the first three flips and a
once-per-3600-frame heartbeat. This reduces avoidable hot-path I/O behind the
reported 58-62 FPS sampling jitter while retaining VSYNC pacing and failures.

The v2.05 hardware run validated the 8,294,400-byte Z32 allocation and binding,
retained the blended `0x80bc89bc` center pixel, and stayed stable at 60 FPS with
only the first three flip breadcrumbs. Version 2.06 clears the entire tiled
depth allocation with the uniform IEEE-754 `1.0f` bit pattern using bounded GPU
fills, waits for DB visibility, and enables Z writes with `LESS_EQUAL` testing.
The existing triangle emits Z=0.5 and must remain visible. This validates depth
clear, target binding, comparison, and writes before overlapping geometry is
introduced.

The v2.06 hardware run kept the depth-enabled triangle visible at 60 FPS,
validating the clear/write baseline. Version 2.07 turns that into an occlusion
test without changing shaders or resources: it draws the half-alpha indexed
triangle at viewport depth 0.25, then submits the same triangle at depth 0.75.
`LESS_EQUAL` must reject the second draw against the first draw's Z writes. The
center pixel should remain the single-blend value `0x80bc89bc`; a changed value
means the farther draw leaked through and isolates depth comparison/write state.

The v2.07 center pixel remained `0x80bc89bc`, proving the farther half-alpha
draw was rejected and completing the initial depth occlusion gate. Version 2.08
adds `CPs4GnmTexture`, which owns a GNM texture descriptor plus aligned backing
range, size, and alignment bookkeeping without owning the surrounding pool. It
validates dimensions and mip count, queries layout before allocation, aligns
within the supplied persistent range, and resets atomically on failure. Startup
uses it to create a persistent 4x4 linear RGBA checker resource after the depth
allocation and reports its byte size. Sampling is the next hardware gate;
v2.08 should preserve the existing image.

The v2.08 hardware run created the 256-byte diagnostic texture, preserved the
depth-tested blended triangle and `0x80bc89bc` center pixel, and stayed stable
at 60 FPS. Version 2.09 moves primitive topology into `CPs4GnmDrawState`, with
dirty tracking and command-boundary re-emission. `DrawIndex` is now the only
direct draw operation after cached fixed pipeline, shaders, semantics, resource
pointers, topology, and index format. Texture sampling remains next; its
resource/sampler table layout will be derived from shader input-usage metadata.

Version 2.10 packages and loads the generated texture-sampling pixel shader.
`psb-dis` confirms one `PTR_INDIRECTRESOURCETABLE` at PS SGPR slot 0; its ISA
loads the 8-DWORD texture descriptor at offset 0 and the 4-DWORD sampler at
offset `0x20`. Startup builds that exact combined table from
`CPs4GnmTexture` plus a point/clamp sampler and fills the full padded texture
range with a four-color pattern. Cached PS shader, semantic, and pointer state
select the sampled path. The triangle should become an opaque sampled checker
color rather than the prior orange blend, while depth still rejects the farther
second draw.

The v2.10 hardware run displayed an opaque purple sampled triangle and read
center pixel `0xffff6363`, validating texture descriptor, combined table,
sampler, shader binding, and sampling. Version 2.11 adds pitch-aware linear
uploads to `CPs4GnmTexture`. It validates source rows against descriptor pitch
and allocation size, copies each row to the padded destination stride, and
zeros row padding deterministically. The checker upload now uses this API rather
than writing raw allocation bytes. The sampled purple result should remain
unchanged and becomes the upload-layout regression oracle.

The v2.11 hardware run stayed stable at 60 FPS and sampled opaque red with
center pixel `0xff6363ff`. This is the intended checker texel at normalized
coordinate `(0.5, 0.5)` after rows are placed at the descriptor's padded pitch;
the earlier purple value came from tightly writing allocation bytes as though
the texture had no row padding. Pitch-aware upload is therefore validated. The
next texture milestone is a color target and texture view sharing one backing
allocation for render-to-texture and resolve/copy testing.

Version 2.12 adds a color-target view to `CPs4GnmTexture` over the same backing
allocation as its texture descriptor. Creation requires matching dimensions,
queries the render-target layout, and rejects any view whose size or alignment
exceeds the texture allocation. Startup creates a 4x4 RGBA target view over the
sampled checker resource. No offscreen draw is submitted yet, so the expected
image and red `0xff6363ff` readback remain unchanged; this is the descriptor
compatibility gate before render-to-texture writes and sampling barriers.

The v2.12 hardware run retained the bars but lost the triangle. Its bounded
diagnostic reported `texture color target view failed`, so shader readiness was
correctly disabled before drawing. The view validator compared the texture and
render-target *required alignment values* rather than testing whether the
actual shared backing address satisfies the render-target alignment. A pool can
over-align an allocation beyond the texture descriptor's minimum, making that
comparison invalid. Version 2.13 checks the real address against the requested
alignment while retaining the size bound. The red sampled triangle must return
before any offscreen write is attempted.

The v2.13 run still reported `texture color target view failed`, proving actual
address alignment was not the only rejected condition. `CPs4GnmTexture` stored
only the texture descriptor's 256-byte used size even though initialization
received the entire remaining persistent-pool capacity. A color-target layout
may require a larger footprint over the same aligned base. Version 2.14 tracks
aligned capacity separately, validates new views against that capacity, and
expands the shared resource's used size to the maximum compatible descriptor
footprint. Table placement follows that expanded size. The red triangle must
return before offscreen writes proceed.

The v2.14 hardware run restored the opaque red sampled triangle, retained
center pixel `0xff6363ff`, and reported a 512-byte shared texture/target
footprint. This proves the render-target view expanded the original 256-byte
texture footprint safely without overlapping the following combined descriptor
table. Shared descriptor compatibility is complete; the next gate writes a
known color through the 4x4 render target, inserts CB-write-to-texture-read
synchronization, and samples that result on the display triangle.

Version 2.15 performs the first render-to-texture pass. Each frame clears the
shared 4x4 backing to black, binds its color-target view, switches to a 4x4
viewport/scissor with depth and blending disabled, and draws the indexed
gradient using the non-sampling pixel shader. A CB visibility barrier follows;
the command then restores the 1080p color/depth state, binds the sampling pixel
shader and combined table, and samples the offscreen center onto the displayed
triangle. The prior static red checker should be replaced by opaque orange from
the offscreen gradient. The farther display draw remains depth rejected.

The v2.15 hardware run displayed the expected opaque orange offscreen sample,
read center pixel `0xff00daff`, and stayed stable at 60 FPS. This validates
render-target binding over shared backing, the 4x4 viewport/scissor transition,
offscreen color writes, CB-write-to-texture-read synchronization, restoration
of the 1080p color/depth state, and subsequent sampling. The minimum
render-to-texture gate is complete. Next, copy the offscreen backing into a
separate texture allocation and sample the copy to validate copy/resolve and
resource-transition behavior without aliasing.

Version 2.16 adds generic synchronized GPU memory copies to OpenGNM commit
`551b5df`, including argument, command-capacity, multi-packet, and exact PM4
layout coverage. Kisak allocates a second non-aliased 4x4 texture with the same
512-byte compatible footprint and its own combined descriptor table. After the
offscreen draw and CB barrier, a DMA_DATA copy transfers the full source
footprint, a destination visibility barrier follows, and the display pass binds
only the copied texture. The opaque orange `0xff00daff` result must remain;
doing so proves copy ordering and sampling without shared-backing aliasing.

The v2.16 hardware run retained the opaque orange triangle and exact center
pixel `0xff00daff` at 60 FPS while sampling only the separate destination
texture. This validates synchronized GPU memory copy, the full 512-byte shared
layout footprint, destination visibility, descriptor-table separation, and
non-aliased sampling. The minimum renderer's clear, indexed fetch, blend,
depth, texture upload/sample, render-to-texture, and copy/resolve hardware gates
are now complete. The next architectural step is exposing these validated
operations through the PS4 D3D9-compatible resource/device façade rather than
adding more standalone bootstrap draws.

Version 2.17 begins that façade migration at the module boundary. Previously
`KisakShaderApiPs4Factory()` returned the `shaderapiempty` factory pointer
verbatim, so the PS4 module had no interface-level interception point. It now
returns a stable PS4 factory trampoline with distinct identity. Unknown or not
yet ported interface versions forward transparently to the empty backend,
preserving `CAppSystemGroup` lifecycle and version checks, while native PS4
device, ShaderAPI, shadow, and hardware-config objects can replace individual
lookups without another module-loading change. Runtime graphics remain on the
validated bootstrap path for this no-regression gate.

The v2.17 hardware run passed that gate: the PS4-owned factory completed engine
initialization, the copied offscreen texture remained opaque orange with center
pixel `0xff00daff`, EOP synchronization passed, and presentation stayed stable
at 60 FPS. The factory trampoline is therefore the validated interception point
for the first native interface. Replace `IShaderDeviceMgr` first while
forwarding device/API/shadow/config lookups until its adapter enumeration and
`SetMode` lifecycle are stable.

Version 2.18 replaces the first interface lookup with a PS4-owned object.
`CShaderDeviceMgrPs4` implements the complete `IShaderDeviceMgr` and
`IAppSystem` surface, delegates adapter/mode behavior to the established empty
manager for now, returns itself from `QueryInterface`, and forwards callbacks
and dependent-object registration. Crucially, successful `SetMode` returns the
PS4 factory rather than the empty factory, keeping subsequent device/API
lookups inside the interception boundary. Other interfaces still forward
unchanged. The copied-texture renderer remains the no-regression hardware gate.

The v2.18 hardware run validated that boundary: shader binaries loaded, the
triangle-over-bars EOP gate passed, and the copied offscreen texture remained
opaque orange at 60 FPS without a crash. Version 2.19 adds a PS4-owned
`IShaderDevice` object covering presentation, resource lifetime, view, shader,
mesh, and buffer entry points. Every operation still delegates to the empty
device in this transition build, so behavior is unchanged while device calls
now have a PS4 interception point. The manager, ShaderAPI, shadow, and hardware
configuration lookups retain their existing behavior.

The v2.19 hardware run validated the PS4-owned device wrapper: shader binaries
loaded, the triangle-over-bars EOP gate passed, and the opaque orange copied
texture remained stable at 60 FPS. Version 2.20 adds a complete PS4-owned
`IShaderShadow` boundary for depth, blend, alpha, culling, texture, fog, vertex
format, and static shader state. Calls still delegate unchanged in this gate;
the next step after hardware validation is replacing individual shadow-state
methods with the native OpenGNM draw-state cache.

The v2.20 hardware run validated the shadow wrapper with the opaque orange
triangle stable at 60 FPS and the shader/EOP gate passing. Version 2.21 adds
the first `CPs4GnmDrawState` cache behind that boundary. It tracks dirty depth,
color, blend, raster, texture, and shader categories plus the corresponding
Source shadow values. Calls still reach the delegate for identical output;
native PM4 emission can now consume only dirty categories without changing the
public `IShaderShadow` contract.

The v2.21 hardware run passed at 60 FPS with the opaque orange triangle and
shader/EOP gate intact. Review then found that the Source-facing shadow cache
had reused the established native emitter's `CPs4GnmDrawState` name in a
different translation unit. Version 2.22 corrects that type-level ODR hazard:
the Source-facing values now live in `CPs4SourceShadowState`, while the existing
`CPs4GnmDrawState` remains the sole OpenGNM/PM4 cache and emitter. The intended
pipeline is explicit: Source shadow state is translated into native GNM state,
then only dirty PM4 categories are emitted.

The v2.22 hardware run validated the corrected two-layer model at 60 FPS with
the opaque orange triangle and shader/EOP gate intact. Version 2.23 adds pure,
host-tested translation from Source depth functions, blend factors and
operations, color write masks, culling, and polygon offset into OpenGNM control
structures. Runtime emission is intentionally unchanged for this gate; the
tested translators are the next input to the existing `CPs4GnmDrawState` PM4
cache.

The v2.23 hardware run passed at 60 FPS with the opaque orange triangle and
shader/EOP gate intact. Version 2.24 connects the Source-facing shadow cache to
the existing native `CPs4GnmDrawState`: depth, blend, render-target write mask,
culling, and polygon-offset changes now populate the matching OpenGNM controls
and dirty categories. Submission remains delegated for this gate; the native
cache is populated but not yet applied by the material-system draw path.

The v2.24 hardware run validated live Source-to-GNM cache population at 60 FPS
with the opaque orange triangle and shader/EOP gate intact. Version 2.25 adds
the first bounded material-system PM4 apply bridge. It restricts the shadow
cache's initial dirty mask to the four owned categories—depth/stencil, blend,
render-target mask, and primitive setup—then emits those controls before the
diagnostic draw state deliberately overwrites them. The log records the exact
emitted mask, proving that the PS4 shader-shadow path reaches OpenGNM command
encoding without yet changing the final diagnostic image.

The v2.25 hardware run logged native shadow mask `0x00000458`, exactly the
primitive, depth/stencil, render-target-mask, and blend categories, before the
triangle/EOP pass remained opaque orange at 60 FPS. Version 2.26 gives the
material-system shadow path sole ownership of the diagnostic render-target
write mask. The bootstrap cache excludes that dirty category and no longer
emits either duplicate `0xf` mask; all other diagnostic controls remain on the
existing path. A preserved triangle therefore validates the first state
category sourced exclusively through `IShaderShadow` and OpenGNM PM4.

The v2.26 hardware run preserved the opaque orange triangle at 60 FPS, proving
the render-target write mask is now sourced exclusively through the native
shadow path. Version 2.27 moves primitive setup as the second owned category.
The diagnostic requests disabled culling through the actual `IShaderShadow`
interface, the Source-to-GNM translator produces the solid-fill CCW primitive
control, and the bootstrap draw cache excludes its duplicate primitive command.
Depth and blend remain deliberately duplicated until their per-pass state
changes are routed through the shadow interface.

The detailed version-by-version bring-up record remains below. The active
boundary is no longer boot, module loading, VideoOut, or content mounting. It
is the minimum OpenGNM-backed D3D9 draw path.

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

All three host tests currently pass, including PS4 content-path normalization
and layout coverage.

## Current implementation attempt

The Orbis build now produces one monolithic executable with a static Source
module registry. Its currently linked runtime graph includes:

```text
platform/tier libraries
  → filesystem + input + physics
  → material system + shaderapiempty
  → datacache + studiorender + sound emitter + VScript
  → VGUI + RocketUI
  → launcher bootstrap + OpenGNM VideoOut
```

Former shared-library targets become static archives on Orbis. Proprietary
Steam API linkage and Linux `dl`/`pthread` linkage are excluded. OpenOrbis has a
dedicated `/orbis` library output directory. The SDK's libc++ headers are added
explicitly because Clang did not discover `${OO_PS4_TOOLCHAIN}/include/c++/v1`
from the PS4 sysroot automatically.

The sibling OpenGNM archive is rebuilt before every monolithic link. Its native
Orbis VideoOut path is hardware validated. The remaining graphics work is to
replace `shaderapiempty` with a PS4 D3D9-compatible façade and issue real PM4
draws while preserving Source material-system behavior.

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

Link closure is complete for the current module set. `kisak_ps4_monolithic`
combines the bootstrap, Source archives, libc++, libc++abi, libc, libkernel,
OpenGNM, GnmDriver, and VideoOut. The PS4 build enables `cmpxchg16b` so Source's
128-bit lock-free list operation does not require a missing `libatomic`
runtime. Former DLL skeleton memory overrides are omitted so tier0 remains the
single allocator owner.

The active compiler/link boundary is the next set of real engine/client/server
modules and the replacement PS4 ShaderAPI, not the bootstrap executable.

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

The v1.64 hardware run crashed after the PS4 pre-init branch succeeded:
`engine launcher bootstrap startup info` and `app after preinit` appeared. The
generic init markers showed many successful system initializations but did not
identify the final system before the crash.

Version 1.65 adds the interface name and completion/failure boundaries around
each `InitSystems()` call. Initialization order and rollback behavior are
unchanged; the next run will isolate the failing subsystem init.

The v1.65 hardware run initialized through `VStudioDataCache005` and stopped
inside `VSoundEmitter003::Init()`. The sound emitter immediately loads the
desktop game-sounds manifest, but PS4 content mounting is not yet complete.

Version 1.66 keeps the existing sound-emitter loader for mounted content and
adds a PS4-only existence guard: when `scripts/game_sounds_manifest.txt` is not
available under the `GAME` search path, the service reports an empty emitter
table and returns `INIT_OK` instead of entering the unavailable asset path.

The v1.66 hardware run remained stable. The sound emitter reported an empty
table for the missing manifest, all systems initialized successfully, and the
engine launcher reached `Run()` before clean shutdown.

Version 1.67 establishes the first PS4 engine-frame handoff: the launcher calls
the RocketUI `RunFrame(0)` and menu-render hooks once when the interface is
available, with bounded begin/complete breadcrumbs. RocketUI remains inert and
OpenGNM rendering is still pending; this checkpoint validates the runtime call
boundary before introducing a continuous frame loop.

The v1.67 hardware run reached both first-frame breadcrumbs and then returned
normally because the launcher was single-shot.

Version 1.68 adds a bounded 120-frame PS4 loop. Each frame polls the Source
input system, advances RocketUI at a 60 Hz timestep, invokes the menu-render
hook, and yields for approximately 16 ms. First-frame and frame-60 markers
bound the loop. This is a timing/input validation loop only; production
shutdown signaling and OpenGNM presentation remain follow-up work.

The v1.68 hardware run reached the frame-60 marker and completed its bounded
input/UI loop without a crash.

Version 1.69 integrates OpenGNM's existing PS4 VideoOut helper into the
monolithic target. It initializes two 1920x1080 direct-memory buffers, registers
them with VideoOut, clears and submits one VSYNC flip on the first frame, and
closes the helper after the loop. OpenGNM/VideoOut failures are logged and kept
non-fatal for this first presentation checkpoint; the temporary empty ShaderAPI
still supplies no GPU draw commands.

The v1.69 hardware run stayed stable but logged `videoout open failed` before
the first frame. The Orbis OpenGNM helper returned one aggregate error, so the
failure could not be separated between `sceVideoOutOpen`, direct-memory
allocation, buffer registration, equeue creation, or flip-event registration.

The v1.70 hardware run classified the failure as the OpenGNM layout stage,
before any VideoOut service call. Version 1.71 checks the layout directly in
Kisak and distinguishes invalid arguments, arithmetic overflow, and unsupported
platform behavior before attempting `sceGnmVideoOutOpen`.

The v1.71 hardware run still reached the generic open-layout failure, while the
direct pre-check did not report an error. Version 1.72 extends OpenGNM's
`GnmVideoOut` diagnostic state with the raw helper error code and logs a
successful direct layout check plus any repeated-open layout error. This tests
for an ABI/stale-archive mismatch before changing VideoOut parameters.

The v1.72 hardware run confirmed the mismatch: the direct layout check logged
success, but `sceGnmVideoOutOpen` still returned the helper's generic layout
failure. The source and Orbis archive timestamps also showed that the archive
was stale, so the concurrent OpenGNM track rebuilt `libopengnm.a` and committed
`cc8315a`, preserving early invalid-argument diagnostics before the layout
path. Kisak v1.73 packages that rebuilt archive for the next PS4 run.
OpenGNM commit `ed3df0e` adds a host regression test for both invalid
create-info forms and their stage/code breadcrumbs.

The v1.74 hardware run confirmed the new marker and reported
`videoout open stage=1 code=0`, while still completing the frame loop cleanly.
That code was the helper's unrecorded `GNM_ERROR_UNSUPPORTED` fallback: the
Orbis Makefile selected `driver_orbis.c` but did not define `OPENGNM_ORBIS`, so
the native VideoOut SDK branch in `helpers.c` was compiled out. OpenGNM commit
`8f9def5` enables that definition from the tracked Makefile and records the
fallback error; Kisak v1.75 stages the resulting native helper path.

Version 1.70 extends `GnmVideoOut` with a diagnostic open-stage field and makes
the helper classify those six boundaries. Kisak logs the stage-specific result
while preserving non-fatal startup behavior. The change is in OpenGNM's shared
VideoOut helper and applies to its standalone hardware smoke path as well.

Reproduce the current cross-build with:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
export LLVM18_PREFIX=/opt/homebrew/opt/llvm@18
./build-ps4-monolithic.sh
```

The monolithic build refreshes the sibling OpenGNM archive before configuring
Kisak, keeping source and archive changes synchronized. Set
`KISAK_OPENGNM_BUILD=0` only when intentionally supplying a prebuilt archive.

For a direct CMake build after that archive refresh:

```sh
cmake -S . -B build-ps4-engine \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/OpenOrbis.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ps4-engine --target launcher_client --parallel 4
```

## Selective Tupoy improvements

The sibling `Kisak-Strike-tupoy-ya` repository is a reference source, not an
integration base. A tracked-file comparison found 20,511 common files, 247
modified common files, and 585 Tupoy-only files. Most of that divergence is
HL2/Portal integration or its later reversion: 223 of the 247 modified common
files are governed by those changes. Do not merge, rebase, or cherry-pick a
commit range from that repository. Port individually reviewed fixes with PS4
tests and preserve this plan's CS:GO-only, monolithic, Steam-free architecture.

Adopt the following improvements in priority order:

1. **VPK close accounting.** Change `CPackFile` destruction to use
   `Trace_FClose` instead of bypassing the filesystem tracker with
   `FS_fclose`. Add a host test that mounts and releases a VPK repeatedly and
   verifies balanced tracked handles. Complete this before relying on long map
   reload or shutdown soaks.
2. **Empty `CUtlBuffer` correctness.** Accept a zero-length external buffer by
   asserting `nSize >= 0`, while continuing to reject negative sizes. Cover
   empty and non-empty construction in a tier1 host test; this affects asset,
   network, and manifest parsing.
3. **Deterministic VGUI text state.** Initialize `TextEntry::_dataChanged` in
   the constructor. Exercise construction and first-frame editing before the
   RocketUI/menu navigation gate.
4. **Search-path-safe debug materials.** Replace hard-coded
   `//platform/materials/...` debug-material names with normal
   `debug/...` material names so the layered `PLATFORM` roots and platform VPK
   resolve them. Validate every `CStudioRender::InitDebugMaterials` lookup
   before model rendering is enabled.
5. **Bot hitbox targeting.** Port the focused `CCSBot::ComputePartPositions`
   correction, but validate it against the actual CS:GO player hitbox set
   instead of assuming model-independent numeric indexes. This is part of the
   offline bot-match acceptance gate.
6. **Complete the x86-64 pointer audit.** Use `intptr_t`/`uintptr_t` for
   collision-set hash values, KeyValues pointer payloads, VGUI panel handles,
   mesh cookies, and other pointer/integer round trips. Never import Tupoy's
   remaining `unsigned` pointer truncations. Add compile-time size assertions
   and focused round-trip tests where possible.
7. **Mine client/server CMake only as inventory.** Use Tupoy's expanded source
   lists to check the real engine/client/server static targets for omitted
   CS:GO files and dependencies. Reconstruct lists deliberately; do not copy
   its targets wholesale.

Explicitly reject the following Tupoy changes for this port:

- HL2, episodic, Portal 2, and template-game source additions.
- The documented nonworking Jolt VPhysics backend; retain Kisak IVP physics.
- Steam overlay, GC, Workshop, inventory, Scaleform, and desktop dynamic-module
  dependencies.
- Unbounded filesystem worker counts, tcmalloc/ASAN runtime integration, and
  threaded bone setup before the PS4 thread/job and memory budgets are proven.
- `unsigned` pointer casts, Linux `dlopen` assumptions, broad module renames,
  and large revert/merge commits.

Sequence this backlog around the main port gates: items 1-3 may land as small
host-tested correctness patches now; item 4 lands before model rendering; item
5 lands with the offline server/bot slice; item 6 is performed incrementally as
each subsystem enters the monolithic link; item 7 informs the engine/client/
server registration milestone. Every imported change must remain an isolated
commit with its originating Tupoy commit recorded in the commit message body.

Implementation status (2026-07-11):

- **Complete:** `CPackFile` now closes through `Trace_FClose` (Tupoy
  `4dff8b6e`), preserving VPK handle accounting.
- **Complete:** zero-length external `CUtlBuffer` construction is accepted
  while negative sizes remain invalid (Tupoy `2c1eb858`).
- **Complete:** `TextEntry::_dataChanged` is initialized in the constructor
  (Tupoy `5a583902`).
- **Complete:** all `CStudioRender::InitDebugMaterials` names use normal
  `debug/...` lookup through mounted platform search paths (Tupoy `e293e29f`).
- **Implemented, runtime validation pending:** CS bot part targeting selects
  the correct old/new animation-state hitbox indexes (Tupoy `1cd07256`). The
  real server target is not yet in the PS4 monolithic graph, so the offline bot
  slice must validate these indexes against loaded player models.
- **First x86-64 audit pass complete:** VPhysics collision-set indexes use
  `intptr_t`, VGUI draw-tree panel payloads use `KeyValues` pointer accessors,
  `HPanel` is pointer-width, and filesystem KV keys are zero-extended through
  `uintptr_t` before handle conversion (Tupoy `b886cb25` and `8ff28d8c`). The
  cross-build no longer reports the filesystem integer-to-pointer warning at
  that conversion. Continue this audit as engine/client/server files enter the
  PS4 target.
- **Inventory complete:** the maintained Kisak client/server CMake lists are
  the CS:GO baseline. Tupoy's lists add a large HL2/episodic graph plus
  Steam/UGC dependencies and provide no safe target-level import. Use them only
  to investigate a specific missing CS:GO translation unit during link closure.

Commits `c535b0b6` and `5110b51c` contain the isolated imports. All four host
tests and the complete current OpenOrbis monolithic cross-build pass after the
changes. Version 1.88 stages those linked fixes for hardware regression
coverage while preserving the shader-triangle diagnostic introduced in v1.87.

## Revised delivery plan and progress

1. **Boot, static modules, and monolithic launcher — complete.**
   The package boots on hardware, writes diagnostics, resolves Source modules
   without `dlopen`, enters `LauncherMain`, initializes the current app-system
   graph, and completes its shutdown lifecycle.
2. **OpenGNM VideoOut presentation baseline — complete.**
   Two 1920x1080 direct-memory buffers open, flip, recycle, and close correctly.
   Repeated presentation is stable and reaches approximately 60 FPS. This
   validates presentation only; it is not the Source renderer.
3. **Content filesystem and persistent engine loop — next.**
   Layer packaged `/app0` bootstrap content with writable/external
   `/data/kisak-strike` content, normalize Source paths, mount VPKs, and replace
   the finite diagnostic loop with a quit-aware engine loop. Exit gate: load
   `gameinfo.txt`, the sound manifest, one VMT/VTF pair, and one BSP through
   normal `GAME` search paths, then shut down cleanly.
4. **Complete monolithic engine/client/server registration — in progress.**
   The supporting systems are linked, but the real engine, client, and server
   factories and lifecycles must replace the bootstrap launcher stand-in. Exit
   gate: reach the real menu state without missing interface versions or
   proprietary runtime modules.
5. **Minimum PS4 D3D9/OpenGNM renderer — in progress.**
   Add `CPs4GnmDevice`, draw-state, texture, memory, and flip components beside
   the PS3 architecture. First implement aligned pools, two frame arenas, EOP
   labels, deferred destruction, vertex/index buffers, declarations, viewport,
   clear, indexed draw, depth, blend, texture sampling, render targets, copy,
   and resolve. The standalone hardware clear and procedural triangle are now
   validated at 60 FPS; migrate that command path into `CPs4GnmDevice` next.
   Exit gate: hardware clear, triangle, indexed texture, depth,
   blend, and render-to-texture tests pass without timeout or memory growth.
6. **Offline shader conversion and manifest — pending.**
   Generate the minimum UI/world combinations from Source shader metadata,
   compile HLSL to SPIR-V and then PS4 `.sb`, preserve Source register numbers,
   and emit strict binding metadata. Missing referenced combinations fail the
   package; diagnostic builds display an error shader and log the combo key.
7. **RocketUI plus DualShock 4 input — pending.**
   Replace the no-device PS4 input backend with `libScePad` sampling, button and
   analog mapping, reconnect handling, and rumble. Route RocketUI through the
   real ShaderAPI. Exit gate: navigate the menu using only a DualShock 4 for a
   30-minute soak.
8. **World and gameplay rendering — pending.**
   Load a BSP, then add static props, models, skinning, decals, particles,
   shadows, and required post-processing in that order. Unsupported states or
   formats must fail visibly rather than render silently black.
9. **Audio and offline-match acceptance — pending.**
   Feed Source's mixer into stereo 48 kHz `libSceAudioOut` on a dedicated
   submission thread, then complete an offline listen-server bot round with
   graphics, input, audio, saves, and clean shutdown. Gate: 30 minutes without
   memory growth, command-buffer overflow, EOP timeout, GPU hang, or audio
   starvation; base PS4 must sustain 30 FPS. Optimize the full game toward
   60 FPS only after this correctness gate.
10. **Multiplayer follow-on — blocked on offline acceptance.**
    Preserve Source UDP/netchannel, then ship loopback, LAN, and public
    community/dedicated-server support in that order. Player-hosted Internet
    sessions and optional ICE/TURN remain last. Steam login and Steam P2P are
    not PS4 runtime dependencies.

## Immediate implementation slice

1. **Complete:** add host-tested PS4 path normalization and root selection for
   `/app0` and `/data/kisak-strike`.
2. **Complete:** hardware validated external loose/VPK roots and replaced the
   sound-manifest fallback with a successful normal asset load.
3. **Complete:** validated representative VMT, VTF, and BSP reads through the
   normal `GAME` search path.
4. **In progress:** the PS4 ShaderAPI target, static module selection, minimal
   device object, and diagnostic `shaderapiempty` delegation are hardware
   validated at 60 FPS. Next replace delegated device/resource interfaces
   through clear and triangle.
5. **Complete:** two command/constant frame arenas submit real OpenGNM command
   buffers and gate reuse on GPU-written EOP labels; the three-submit hardware
   test passed at 60 FPS. Next retain the arenas for the runtime GPU-clear path
   instead of releasing the test pool.

Each slice must update this document with the package version, hash, hardware
evidence, and the next unresolved boundary. Avoid broad renderer or gameplay
changes until the preceding exit gate is proven.

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
