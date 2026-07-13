# Kisak-Strike PS4/OpenGNM Port

## Goal

Port Kisak-Strike to PS4 homebrew as a monolithic OpenOrbis executable. Keep
Source's D3D9-compatible material-system boundary and use the PS3 implementation
as the console architecture reference, while replacing every Cell, SPU, PRX,
RSX/GCM, PS3 audio, input, filesystem, and presentation dependency.

OpenGNM is the only graphics backend. Linux ToGL/OpenGL and PS3 shader binaries
are not part of the PS4 runtime. The first acceptance target is menu navigation
and a complete offline bot match.

## Current status — 2026-07-12

The OpenOrbis bootstrap is hardware validated. Title `KISK00001` starts on PS4,
creates `/data/kisak-strike/startup.log`, and remains stable in a one-second
kernel sleep loop. The first package returned from `main()` after writing all
markers; the PS4 shell reported that normal return as an application crash.
Commit `b8f56b31` fixed the behavior by keeping the bootstrap alive.

Latest staged monolithic package:

```text
Package: IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg
Version: 3.04
SHA-256: e3b03bef8e2a2263140a96915d14d417fd7426680e1f9777af044272436a8066
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
- The current frame is a four-band OpenGNM diagnostic with a textured,
  navy-framed spinning 3D cube. It is hardware validated at 60 FPS with depth,
  indexed drawing, texture sampling, shader constants, and EOP-gated two-frame
  reuse.
- `shaderapiempty` remains the delegated API surface, but PS4 overrides now
  provide native Source vertex/index buffers, canonical vertex descriptions,
  dynamic mesh locks, device bindings, and indexed draw-packet construction.
  The next renderer boundary is Source-owned command emission inside a frame
  opened before material/UI rendering.
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

The v2.27 hardware run preserved the opaque orange triangle at 60 FPS, proving
primitive setup is now the second exclusively shadow-owned state category.
Version 2.28 moves depth/stencil control next. The offscreen pass disables depth
testing and writes through the shadow bridge; the display pass enables both
only when its depth target exists and reapplies the translated control. Both
bootstrap `GnmDepthStencilControl` emissions are removed, and the second native
apply logs an expected depth-only dirty mask. Depth-target binding itself stays
in the diagnostic cache because it is a resource binding, not shadow state.

The v2.28 hardware run logged the expected display depth-only mask `0x10` and
preserved the opaque orange triangle at 60 FPS, validating exclusive shadow
depth ownership. Version 2.29 moves blend control as the fourth owned category.
The offscreen pass disables blending through the shadow bridge; the display
pass selects source-alpha/inverse-source-alpha color blending with separate
one/zero alpha blending. Both bootstrap `GnmBlendControl` emissions are removed.
The second native apply should now report depth plus blend (`0x410`).

The v2.29 hardware run logged the expected display depth-plus-blend mask
`0x410` and preserved the opaque orange triangle at 60 FPS. Render-target mask,
primitive setup, depth, and blend are therefore all exclusively shadow-owned.
Version 2.30 adds a complete PS4-owned `IDebugTextureInfo` interface while
delegating the current empty-backend behavior. This seven-method boundary is
the future home for native texture inventory and PS4 GPU-memory statistics and
avoids attempting the 227-method `IShaderAPI` replacement as one unsafe step.

The v2.30 hardware run validated the native debug-texture interface boundary
with both shadow masks, the opaque orange triangle, and 60 FPS intact. Version
2.31 makes its first method native. `CPs4GnmTexture` now accounts actual backing
bytes across initialization, render-target-view footprint growth, reset, and
destruction. `IDebugTextureInfo::MEMORY_TOTAL_LOADED` reports that bounded total
instead of the empty backend's zero, and startup logs the value after both
diagnostic textures are ready. Other debug-memory categories still delegate.

The v2.31 hardware run reported exactly 1,024 native texture bytes—two 512-byte
diagnostic footprints—before both shadow masks and the opaque orange 60 FPS
draw passed. Version 2.32 hardens this accounting for Source's threaded loading
model. Backing-byte add, remove, and read operations now use relaxed atomic
updates, and `CPs4GnmTexture` is non-copyable so ownership cannot be duplicated
and subtracted twice. The reported value and rendering should remain unchanged.

The v2.32 hardware run retained the exact 1,024-byte native texture total,
both shadow masks, the opaque orange triangle, and 60 FPS. Version 2.33 begins
the Source shader-combo path with a bounded, allocation-free manifest. Entries
are keyed by shader name, vertex/pixel stage, static combo, dynamic combo, and
vertex format and own fixed-size copies of their `.sb` paths. Registration
rejects invalid, oversized, duplicate, or over-capacity entries; lookup is exact
and deterministic. Host tests cover stage/combo/vertex-format separation,
duplicate rejection, missing combinations, and reset. Runtime still loads the
three established diagnostic shaders directly for this no-regression gate.

The v2.33 hardware run retained the 1,024-byte texture total, both native
shadow masks, the opaque orange triangle, and 60 FPS. Version 2.34 exercises
the manifest at runtime. The diagnostic vertex shader and both pixel shaders
are registered with explicit stage/combo/vertex-format identities, then every
packaged `.sb` path is obtained by exact lookup before file loading. Startup
logs `native shader manifest entries=3`; registration or lookup failure keeps
the established visible fallback and reports the failing stage.

The v2.34 hardware run logged `native shader manifest entries=3`, then loaded
all binaries and preserved the opaque orange triangle at 60 FPS. Version 2.35
moves those entries into `/app0/kisak_diagnostic.manifest`. A strict bounded
text parser accepts vertex/pixel stage, combo IDs, vertex format, and `.sb` path;
it rejects malformed stages, duplicates, oversized lines, invalid entries, and
empty files atomically. Package construction now requires and includes the
manifest, and runtime parses it before exact lookup. Host tests cover successful
multi-entry parsing plus duplicate and invalid-stage rejection.

The v2.35 hardware run parsed the packaged manifest, logged three entries,
loaded all shaders, and preserved the opaque orange triangle at 60 FPS. Version
2.36 extends each entry with vertex-input count, constant-layout byte count,
sampler mask, and fragment-output mask. The diagnostic manifest declares these
bindings explicitly, the parser requires all fields, and runtime validates the
vertex shader's declared input count against OpenGNM `.sb` metadata before
copying code. Host tests verify binding metadata survives parsing and lookup.

The v2.36 hardware run validated the extended manifest and preserved the
opaque orange triangle at 60 FPS. Version 2.37 validates every remaining
binding declaration before copying shader code: embedded constant bytes must
match exactly, declared sampler bits must be available through immediate
samplers or a sampler-table pointer, and the fragment-output mask must match
the active nibbles in `SPI_SHADER_COL_FORMAT`. Any mismatch retains the visible
fallback and logs actual versus declared binding values.

The v2.37 hardware run correctly retained the four bars but rejected the
texture-sampling pixel shader, so the triangle was missing. Its exact diagnostic
was `stage=2 constants=0/0 samplers=0x0/0x1 outputs=0x1/0x1`. The shader does
not expose an immediate sampler or separate sampler-table pointer: it uses the
already validated combined texture/sampler descriptor table through an
OpenGNM `PTR_RESOURCETABLE` usage slot. Version 2.38 recognizes either resource-
table or sampler-table pointers as satisfying declared sampler slots while
retaining strict immediate-sampler, constant, and fragment-output validation.

The v2.38 hardware run produced the same stage-2 rejection: this compiled `.sb`
contains no immediate sampler, sampler-table pointer, or resource-table usage
record even though the working draw explicitly binds a combined descriptor
table. Version 2.39 defers only metadata-invisible sampler bits. Metadata-visible
samplers remain strict; unresolved bits are accumulated and must fit the actual
combined table's one-slot mask before the table is accepted. Startup logs
`native shader sampler binding mask=0x1 source=combined_table` when that concrete
binding satisfies the manifest. Constants and fragment outputs remain exact.

The v2.39 hardware run logged the concrete combined-table sampler mask `0x1`,
loaded all three shaders, and restored the opaque orange triangle at 60 FPS.
Version 2.40 enforces the manifest before PKG creation as well as at runtime.
Packaging rejects unsupported stages, non-numeric combo/binding fields,
duplicate full keys, empty manifests, paths outside `/app0`, and any referenced
shader binary missing from the package tree. The manifest path is overridable
for packaging tests while the validated diagnostic manifest remains default.

The v2.40 hardware run passed package/runtime manifest enforcement, concrete
sampler binding, and the opaque orange 60 FPS draw. Version 2.41 adds the
reusable `CPs4GnmShader` resource. It validates binary metadata and stage,
bounds the copied stage header to 1 KiB, requires 256-byte-aligned external GPU
code memory, bounds code size to the supplied allocation, patches VS/PS program
addresses, and exposes typed stage accessors. It owns no GPU allocation yet;
the next gate replaces one diagnostic shader's ad-hoc storage with this object
before moving all three and then native shader handles.

The v2.41 hardware run validated the shader-resource foundation without a
rendering regression. Version 2.42 migrates the texture-sampling pixel shader,
which produces the final display triangle, to `CPs4GnmShader`. Its stage header,
GPU code copy, address patch, and typed PS accessor now come from the reusable
resource; the vertex and solid offscreen pixel shaders retain their established
storage for this one-resource gate. Startup logs the native resource stage and
code byte count before the normal shader-loaded marker.

The v2.42 hardware run logged the native texture-pixel resource and preserved
the opaque orange triangle at 60 FPS. Version 2.43 migrates the solid offscreen
pixel shader as well. Both PS stages now use independent `CPs4GnmShader`
objects; only the vertex shader retains its legacy 1 KiB stage buffer. Runtime
logs `role=solid_pixel` and `role=texture_pixel` with their code sizes before
the established shader-loaded marker.

The v2.43 hardware run logged both pixel resources (`52` and `84` code bytes)
and preserved the opaque orange triangle at 60 FPS. Version 2.44 migrates the
vertex shader, removes the last legacy stage array, and loads all three stages
through `CPs4GnmShader`. GPU code placement now advances using each resource's
validated code size. Runtime logs `role=vertex` in addition to both pixel roles
before fetch-shader construction and the established shader-loaded marker.

The v2.44 hardware run logged all three native shader resources (`40`, `52`,
and `84` code bytes) and preserved the opaque orange triangle at 60 FPS.
Version 2.45 adds a bounded 64-entry native shader-handle table. Handles encode
slot plus generation, resolve only for the requested vertex/pixel stage,
register idempotently, reject capacity exhaustion, and invalidate stale handles
after destruction and slot reuse. Host tests cover stage mismatch, idempotence,
destruction, generation advancement, and stale-handle rejection. Runtime
resource registration follows after this table-only no-regression gate.

The v2.45 hardware run validated the handle-table foundation with all three
shader resources and the opaque orange triangle stable at 60 FPS. Version 2.46
registers those live resources in the table, stores their generation-checked
handles, resolves each resource with the required vertex/pixel stage, and takes
the stage pointer used by fetch construction and draw-state binding only from
that resolved resource. Runtime resource breadcrumbs now include each handle;
registration or typed resolution failure retains the visible fallback.

The detailed version-by-version bring-up record remains below. The active
boundary is no longer boot, module loading, VideoOut, or content mounting. It
is the minimum OpenGNM-backed D3D9 draw path.

## PS4 console UI policy

The console audit found that `IsGameConsole()` is not a safe PS4 selector.
That legacy predicate enables a mixture of Xbox 360 and PS3 filesystem,
byte-order, packed-asset, memory, shader, and renderer assumptions. PS4 must
continue to report `IsPC() == 1` and `IsGameConsole() == 0` so it can consume
Kisak's little-endian PC VPK/VTF/BSP content. PS4-specific code uses `IsPS4()`;
controller-first UI behavior uses the new `IsPlatformConsoleUI()` predicate.

Scaleform 4.2 source is available as the external sibling tree
`../scaleform_sdk`. It contains the AS2/GFx player, render core, D3D9 HAL, and
partial `SF_OS_ORBIS` platform accommodations. The legally supplied content at
`/Volumes/Untitled/CSGO/csgo/resource/flash` contains the authentic menu, HUD,
loading, font, `.gfx`, and `.swf` assets. Neither external tree is committed or
redistributed by Kisak.

Scaleform is therefore the preferred compatibility UI for the PS4 port.
PS4 forces the Sony/PS3 presentation mode only inside `scaleformui` so the
movies receive platform code 2 and Sony controller conventions. It must not
change global `IsGameConsole()` or route engine/content code through PS3.
RocketUI remains the open-source fallback and development diagnostic UI.

Bring up Scaleform in these bounded steps:

0. Complete the reusable `CPs4GnmDevice` D3D9-compatible façade first. Scene
   lifetime, vertex/index buffers, stream/declaration binding, textures,
   render/depth targets, fixed-function state translation, shaders/constants,
   queries, draws, and presentation belong to the material-system backend and
   must be covered by host tests rather than implemented inside Scaleform.
1. Add an optional `KISAK_SCALEFORM_SDK_ROOT` external build input and compile
   the required Kernel, Render, GFx/AS2, image, and font components as static
   libraries for OpenOrbis. Never copy SDK source or binaries into Kisak.
2. Start from Scaleform's D3D9 HAL because Kisak already exposes the D3D9
   object model. Audit every required device call and replace its embedded
   D3D9 shader binaries with PS4 `.sb` shaders generated through the existing
   SPIR-V/`opengnm-psbc` pipeline. If that adapter becomes more complex than a
   native backend, implement `Render::GNM::HAL` against the shared Scaleform
   render core instead.
3. Link `scaleformui` statically, register `SCALEFORMUI_INTERFACE_VERSION`, and
   select `INCLUDE_SCALEFORM` for the PS4 monolithic engine/client. Keep the
   no-op RocketUI bootstrap available only until Scaleform initialization and
   the visible diagnostic movie pass on hardware.
4. Mount the supplied `resource/flash` assets through the normal `GAME` search
   path rather than packaging or committing them. Validate `fontlib.gfx`,
   `gameuirootmovie.gfx`, `mainmenu.gfx`, `pausemenu.gfx`, and one HUD movie in
   that order.
5. Feed `libScePad` events into Source `InputEvent_t` and then Scaleform.

The OpenGNM Scaleform HAL is deferred until `CPs4GnmDevice` passes its minimum
indexed textured draw, dynamic-buffer, blend/depth, render-target,
shader-constant, and EOP recycling tests. The later Scaleform adapter consumes
that façade and must not duplicate PM4, resource lifetime, or D3D9 state-cache
behavior.

### Classic Scaleform boot sequence: Legals -> StartScreen -> MainMenu

The production console boot flow has three separate Scaleform elements. Each
stage is loaded through `_global.RequestElement`; it is not one timeline inside
`MainMenu`. The PS4 manager currently skips the first two stages: after loading
`mainuirootmovie.swf`, `CPs4ScaleformMovieManager` requests `MainMenu`
directly. Restoring the classic flow is a tracked UI milestone after the base
OpenGNM image path is stable.

#### Stage 1: Legals (`legals.swf`)

`CCreateLegalAnimScaleform::CreateIntroMovie()` requests this element from
`cstrike15basepanel.cpp:1441-1458`. The exported movie contains these embedded
JPEG images:

| File | Size | Role |
| --- | --- | --- |
| `images/3.jpg` | 1280x720 | Full-screen Valve-logo splash background |
| `images/20.jpg` | 1280x720 | Second full-screen splash background |
| `images/6.jpg` | 640x360 | Smaller logo/image |
| `images/11.jpg` | 736x313 | Wide Valve/Hidden Path-style logo |
| `images/12.jpg` | 296x101 | Small logo |
| `images/28.jpg` | 198x252 | Ratings-board logo |
| `images/37.jpg` | 268x266 | Ratings-board logo |

It also uses rating-badge PNGs `23.png` (1280x720), `31.png` (251x251),
`34.png` (610x311), and `42.png` (213x261). In
`legals.swf/scripts/frame_1/DoAction.as`, the movie calls
`GetRatingsBoardForLegals()` and selects ESRB, PEGI, USK, CERO, GRB, OFLC, or
BBFC/PEGI artwork. Frame 2 of the panel animation plays
`valve_logo_music.mp3`. After 180 frames (about three seconds at 60 Hz),
`finishAnimation()` removes and unloads the element.

The PS4 stage controller must request `Legals`, wait for both element load
completion and `AnimationCompleted`, then remove the Legals element and advance
to StartScreen. Missing regional metadata must choose a deterministic fallback
rating board and log that choice rather than blocking boot.

#### Stage 2: StartScreen (`startscreen.swf`)

`CCreateStartScreenScaleform::LoadDialog()` requests this element from
`createstartscreen_scaleform.cpp:46-54`. Its exported content includes:

| File/symbol | Size | Role |
| --- | --- | --- |
| `images/1.jpg` | 2134x1200 | Main splash background (about 428 KiB JPEG) |
| `shapes/4.svg` | 307x71 | Vector `COUNTER-STRIKE` logo paths |
| `shapes/8.svg` | 1280x78 | Gradient overlay bar |
| `texts/11.txt` | n/a | `#SFUI_PressStartPrompt@24` prompt |
| `DefineSprite_7` | 753x174, 30 frames | Animated logo reveal/fade |
| `DefineSprite_9` | 1280x78 | Gradient overlay strip |
| `DefineSprite_13` | timeline | `ShowStart` frame 1-to-25 transition |

The ActionScript entry points are intentionally small:
`onLoaded()` calls `gameAPI.OnReady()`, and `ShowStartLogo()` plays the
`ShowStart` label. After platform statistics finish loading,
`cstrike15basepanel.cpp:1263-1270` calls `ShowStartLogo()`. The screen then
waits for a controller Start/confirm action; `CompleteStartScreenSignIn()`
dismisses it and advances to MainMenu. Until full sign-in services exist, PS4
uses an offline local-user completion path while preserving the same callbacks
and transition boundaries.

The key splash source is external content at
`/Volumes/Untitled/Counter Strike Global Offensive/gfx_export/startscreen.swf/images/1.jpg`.
It is authored at 2134x1200 and mapped into 1280x720 by
`patternTransform="matrix(0.6, 0.0, 0.0, 0.6, 0.0, 0.0)"` in
`shapes/2.svg`; `ResizeManager` then applies the 1.5x 1080p scale to fill
1920x1080. The logo remains vector path data in `shapes/4.svg` and must not be
rasterized during packaging.

#### Stage 3: MainMenu (`mainmenu.swf`)

MainMenu is requested only after StartScreen completes. Its existing
`OnLoadFinished`/`OnReady`, `showPanel()`, `RunFrame`, menu, and HUD phase
boundaries remain unchanged; the boot controller selects when the initial
request occurs rather than adding special behavior inside the movie.

#### PS4 implementation and verification plan

1. Replace the unconditional initial `RequestElement("MainMenu")` in
   `scaleform_gfx_manager.cpp` with an explicit boot-stage state machine:
   `LegalsLoading`, `LegalsPlaying`, `StartScreenLoading`,
   `StartScreenWaiting`, and `MainMenuLoading/Ready`.
2. Give every stage a fresh element-specific `gameAPI` object and callback
   table. Do not reuse the root slot's `GameInterface` object.
3. Drive transitions only from the real callbacks: Legals load plus animation
   completion, StartScreen `OnReady` plus controller confirmation, and MainMenu
   `OnReady`. Make removal idempotent so duplicate AS callbacks cannot load two
   stages.
4. Package or mount the legally supplied Legals/StartScreen movies, JPEG/PNG
   dependencies, vector shapes, fonts, localization, and
   `valve_logo_music.mp3`; do not commit or redistribute those assets.
5. Preserve a development option to start directly at MainMenu, but make the
   classic three-stage path the production default once validated.
6. Add bounded markers for stage request, load completion, callback transition,
   removal, timeout/error fallback, and total elapsed frames.

Host tests cover state-machine ordering, duplicate callback suppression,
missing-stage fallback, ratings-board selection, offline Start confirmation,
and direct-to-menu development mode. Hardware acceptance requires: Legals
artwork and audio; automatic transition after roughly 180 authored frames;
the full-resolution StartScreen splash and animated vector logo; DualShock 4
confirmation; one and only one MainMenu request; stable 1080p scaling; and no
memory growth across repeated boot-to-menu cycles.

The first post-v2.53 façade slice now gives `CPs4GnmDevice` deterministic
`BeginScene`/`EndScene` lifetime, eight vertex-stream bindings, 16/32-bit index
binding, primitive-topology state, and overflow-safe indexed-draw range
validation. Host tests cover missing bindings, double scene entry/exit, stream
limits, index range overflow, and vertex range overflow. The OpenOrbis monolith
also links with this state layer; live command emission remains the next slice.

`CPs4GnmBuffer` now supplies the façade's external-memory vertex/index resource
boundary. It validates GPU alignment and index widths, supports bounded static
uploads, D3D-style partial locks, full-buffer dynamic discard locks, balanced
unlocking, and discard generations for later deferred-storage rotation.
`CPs4GnmDevice` accepts these typed resources directly and rejects cross-binding
an index buffer as a vertex stream or a vertex buffer as indices.

The buffer layer now generates native 16-byte `GnmBuffer` vertex descriptors
with validated format, stride, offset, and record count. `CPs4GnmDrawState`
caches the descriptor as a distinct dirty state and emits it with
`sceGnmDrawCmdSetVsharpUserData`, alongside the existing cached index-size and
primitive-topology commands. Descriptor layout and out-of-range record counts
are host-tested against OpenGNM's data-format implementation. Device-to-draw
state assembly and the final indexed draw packet remain the next slice.

`CPs4GnmDevice::BuildIndexedDrawPacket` now freezes validated D3D9 façade state
into an immutable OpenGNM packet: base-vertex-adjusted `GnmBuffer`, first-index
GPU address, index width/count, and translated primitive type. The pure packet
builder is host-tested for triangle list/strip, base vertex, record count, and
index addressing. `Ps4EmitIndexedDraw` consumes that packet, applies only dirty
draw state, and emits `sceGnmDrawCmdDrawIndex`. Connecting this emitter to the
live two-frame submission path is next.

Version 2.54 routes the live indexed diagnostic triangle through
`CPs4GnmBuffer`, `CPs4GnmDevice` scene/binding validation,
`BuildIndexedDrawPacket`, and `Ps4EmitIndexedDraw`. The proven two-entry fetch
descriptor table remains in place because the current shader consumes position
and color through that ABI; only the formerly manual index-width, topology, and
draw calls move behind the façade in this gate. Hardware should retain the
opaque orange triangle and log
`kisak-ps4: D3D9 facade indexed diagnostic draw emitted`.
The v2.54 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`4ef7c7549891012208a5dc808ba18dd1132be79d671e2dbde8a6a57c1bf4137b`,
with marker `kisak-ps4: build marker d3d9_facade_draw_v254`.

The v2.54 hardware run reached
`D3D9 facade indexed diagnostic draw emitted` and then crashed before the
center-pixel/readback breadcrumb. The façade packet itself was valid; the
regression was `BeginCommand()` marking the new vertex-buffer state dirty even
though this shader still binds its two descriptors through an external fetch
table. `Apply()` consequently wrote the zero-initialized standalone descriptor
over VS user data. Version 2.55 tracks whether a standalone vertex buffer has
ever been bound, excludes unbound state from command-start dirtiness, and
guards emission. The v2.54 crash log is
`hardware-captures/logs/2026-07-12/kisak_v254_d3d9_facade_draw_crash.txt`.
The v2.55 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`94a6ce3248e4d134d2f65336ccc6a70048a260732d4cc7b93c7f9f3c701f0e06`,
with marker `kisak-ps4: build marker guarded_facade_vsharp_v255`.

The v2.55 hardware run restored the complete path: façade draw emission,
center pixel `0xff00daff`, EOP completion, repeated flips, and stable 60 FPS
beyond frame 900. Its capture is
`hardware-captures/logs/2026-07-12/kisak_v255_guarded_facade_live.txt`.
Version 2.56 removes the remaining direct `sceGnmCreateVertexBuffer` calls from
the diagnostic setup. Both interleaved position and color descriptors are now
built through `CPs4GnmBuffer::BuildVertexDescriptor`, including offset, stride,
format, and record-count validation, while retaining the proven direct-memory
descriptor table consumed by the fetch shader.
The v2.56 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`77d9d500385f9cca1693ecfe73229415c31e2275a233cfc04e8e3854f5702a38`,
with marker `kisak-ps4: build marker facade_vertex_table_v256`.

The v2.56 hardware run stayed stable but logged
`vertex facade descriptor table failed`, so shader setup deliberately retained
only the four DMA bars. The color attribute begins 16 bytes into a 32-byte
interleaved vertex. The first range check incorrectly required
`vertexCount * stride` bytes after that attribute offset; the correct extent is
`offset + (vertexCount - 1) * stride + elementBytes`. Version 2.57 uses that
overflow-safe formula and adds a host regression for a nonzero interleaved
attribute offset. The capture is
`hardware-captures/logs/2026-07-12/kisak_v256_facade_vertex_table_missing.txt`.
The v2.57 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`84c9e33c05a49e8ff81eac87596cf6166ba3124ba9a42f08a6fe336e3eb81e82`,
with marker `kisak-ps4: build marker interleaved_vertex_range_v257`.

The v2.57 hardware run restored the full interleaved-table path, including
façade draw emission, center-pixel validation, EOP, flips, and stable 60 FPS
beyond frame 780. Its capture is
`hardware-captures/logs/2026-07-12/kisak_v257_interleaved_vertex_live.txt`.
The next host-only façade slice adds `CPs4GnmVertexDeclaration`: up to sixteen
stream/offset/format elements, validated stream indices and formats, and device
assembly of descriptor tables from typed stream resources. Tests cover valid
position declarations, invalid streams, base-vertex offsets, descriptor-table
capacity, and out-of-range vertex extents. The OpenOrbis monolith links this
layer; live setup is unchanged until device initialization moves ahead of
diagnostic resource creation.

Version 2.58 initializes `CPs4GnmDevice` immediately after direct-memory
mapping, before persistent shader resources are created. The live diagnostic
position/color table is now described by a two-element
`CPs4GnmVertexDeclaration` and assembled through
`BuildVertexDescriptorTable`; the resulting two `GnmBuffer` records remain in
the identical direct-memory location consumed by the proven fetch shader.
The v2.58 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`3b2b3fef62e97a288c74dbc3eb829aa349b60d67a2e87df030f1a6fa1651d916`,
with marker `kisak-ps4: build marker live_vertex_declaration_v258`.

The v2.58 hardware run passed the live declaration path, center-pixel check,
EOP, flips, and remained stable through at least frame 1200 at 60 FPS. Its
capture is
`hardware-captures/logs/2026-07-12/kisak_v258_live_vertex_declaration.txt`.
`CPs4GnmConstants` now preserves D3D vertex registers 0-255 and pixel registers
0-223, validates register ranges, and snapshots the used prefix into a
256-byte-aligned per-frame `GnmBuffer`. Host tests cover sparse register
numbering, overflow rejection, alignment, copied values, and reset behavior.

The next hardware diagnostic is a distinct 3D draw gate: perspective-transformed
geometry with varying clip-space Z plus an overlapping farther primitive. It
must visibly demonstrate interpolation and depth rejection, retain EOP/flip
stability, and use the façade declaration, indexed packet, constant snapshot,
and depth-state paths. It will be kept separate from the established 2D bars
and triangle until validated.

Version 2.59 adds the first 3D hardware gate without changing shader binaries.
The three clip-space vertices carry distinct W and Z values, exercising
perspective division and perspective-correct color interpolation. A second copy
uses a farther viewport depth and a +180-pixel X offset: its non-overlapping
edge should remain visible while the near triangle rejects the overlapping
fragments. The stable bars remain behind both draws. Startup logs
`kisak-ps4: perspective depth-overlap diagnostic emitted` after the façade
scene completes.
The v2.59 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`f7ab1a3e5d524129a1f0055d4c38fe4336a97c362f495642c288ccc8743c50c5`,
with marker `kisak-ps4: build marker perspective_depth_draw_v259`.

The v2.59 hardware run passed, but its two overlapping yellow triangles were a
poor visual proof of 3D despite exercising perspective and depth. Version 2.60
replaces that geometry with a six-face cube: 24 per-face-color vertices, 36
16-bit indices, CPU-generated rotated perspective clip coordinates, and the
solid interpolated-color pixel shader. The depth target resolves face ordering;
incorrect winding, projection, indexing, or depth should now be immediately
visible. The v2.59 capture is
`hardware-captures/logs/2026-07-12/kisak_v259_perspective_depth_live.txt`.
The v2.60 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`c4bc798b9784ce50ec3c0ddb18bf0b80d73cf80f7032eb91702143f8075305a8`,
with marker `kisak-ps4: build marker indexed_color_cube_v260`.

The v2.60 hardware run was stable and emitted all cube faces, but the screenshot
showed a screen-filling inside-room presentation. Its center readback was
`0x80e38c9b`; alpha `0x80` proved the display blend state was still active.
Version 2.61 reduces projected X/Y scale from 1.25 to 0.45 and disables both
color and alpha blending for the cube pass. The expected result is a compact,
centered, opaque cube over the unchanged bars. Double-sided rasterization stays
enabled so back-face culling can be validated separately. The capture is
`hardware-captures/logs/2026-07-12/kisak_v260_indexed_color_cube_live.txt`.
The v2.61 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`e6916a9c8fdfee0c15bff5e5edbcdc6564e5728ca65bf6327fa9bfbd681e9c55`,
with marker `kisak-ps4: build marker compact_opaque_cube_v261`.

The v2.61 screenshot showed a compact opaque cube but exposed its rear/interior
faces because raster culling remained deliberately disabled. The six face
index lists are outward-wound, and the PS4 translator declares CCW front faces.
Version 2.62 enables `GNM_CULL_BACK` while leaving projection, indices, depth,
and blending unchanged. A correct result shows only the three exterior faces
visible from the diagnostic camera. The capture is
`hardware-captures/logs/2026-07-12/kisak_v261_compact_opaque_cube_live.txt`.
The v2.62 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`9bd8ea5ccc736dfcae168b43f768805b4d246c878c18583caba674ac74dc74a0`,
with marker `kisak-ps4: build marker cube_backface_cull_v262`.

The culling experiment did not remove the interior faces. Inspection of
`freegnm-examples/cube` identified the actual divergence: the reference uses a
shader-driven tiled-depth clear, while Kisak wrote `1.0f` linearly into a tiled
depth allocation. Version 2.63 packages the example's `clear.frag.sb` and ports
its command sequence: depth-clear enable, `SetDepthClearValue(1.0)`, embedded
fullscreen VS, clear PS/constant descriptor, bound depth target, and a RECTLIST
draw. Cube rasterization returns to `GNM_CULL_NONE`, matching the reference, so
hidden-face removal now tests depth alone.
The v2.63 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`e3e6e5e1282fe2fa36d89a5c854cafacd79ed9121614bc96a2769e12362e8317`,
with marker `kisak-ps4: build marker reference_depth_clear_v263`.

The v2.63 screenshot remained unchanged. A deeper descriptor comparison found
the second reference mismatch: Kisak hardcoded `GNM_TM_DEPTH_1D_THIN`, whereas
`freegnm-examples/cube` derives the tile mode from
`sceGpaFindOptimalSurface(GPA_SURFACE_DEPTH, 32bpp, 1 fragment)`. Version 2.64
uses that GPUAddr-selected tile mode before calculating allocation size and
setting Z read/write addresses, while retaining the reference shader clear.
The v2.64 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`0c29243497ea8935848c0b435c1c2835718e50e62cdf5fcf804336c3b3d3c7c7`,
with marker `kisak-ps4: build marker reference_depth_layout_v264`.

The v2.64 screenshot remained identical, ruling out depth tile selection as the
visible blocker. Version 2.65 bypasses the ShaderShadow translation/cache for
the diagnostic cube and emits the reference example's exact zeroed
`GnmDbRenderControl` plus depth-enabled, Z-write,
`GNM_DEPTH_COMPARE_LESSEQUAL` `GnmDepthStencilControl` immediately before the
indexed draw. Clear, target layout, projection, and geometry are unchanged.
The v2.65 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`19f0b4c89880607233761eaacb5a128f2a588884722d439d14024bc1f117ce4a`,
with marker `kisak-ps4: build marker direct_depth_control_v265`.

The untouched `/data/pkg/IV0000-FGNM00001_00-CUBESAMPLE000000.pkg`
(SHA-256 `b99208b2d8145b78f85603c0ba8be2475296f5f55146f71cb5aef3c9c7c9d576`)
renders a correct spinning 3D cube on the same console. This proves the PS4,
OpenGNM depth implementation, and reference pipeline are sound; Kisak's custom
preprojected cube is the faulty layer. Kisak packaging now includes the
reference `cube.vert.sb`/`cube.frag.sb` as dedicated manifest entries. The next
slice binds their exact position/UV layout, MVP descriptor set, texture/sampler
descriptor set, and 36-vertex draw through the façade.

The native loader now validates and allocates both reference shaders through
`CPs4GnmShader` and registers typed handles for the cube VS/PS alongside the
existing diagnostic shaders. Package construction and all ten host tests pass;
the intermediate package remains unstaged until the reference descriptor sets
and draw are connected.

Version 2.66 connects that complete reference draw contract to Kisak's 1080p
display pass. It supplies the example's 36 position/UV vertices, two vertex
buffer descriptors, generated fetch shader, VS slot-6 MVP constant descriptor,
PS slot-0 texture/sampler descriptor, shader semantic linkage, direct reference
depth controls, and non-indexed `DrawIndexAuto(36)`. The original 4x4 offscreen
diagnostic remains on its own shaders and supplies a Kisak-owned test texture;
the display viewport now also uses the reference 0.5 depth offset. The complete
OpenOrbis target and all ten host tests pass. Marker:
`kisak-ps4: build marker reference_cube_pipeline_v266`. Hardware must now show
the reference geometry with correct occlusion; rotation can be added only after
this fixed-MVP parity gate passes.
The v2.66 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`91668cdc076e1ec5dce72a87e3bcc89e358c9bb4916efda3c8979ae65d1401db`.

The v2.66 hardware run displayed a correctly projected textured cube with
three exterior faces, coherent UV interpolation, and hidden-surface removal at
approximately 60 FPS. This passes the fixed-MVP parity gate and confirms the
Kisak display pass now matches the proven reference shader and depth contract.
Version 2.67 retains that contract and updates only the existing MVP contents:
monotonic elapsed time drives 15-degree/second X rotation and 60-degree/second
Y rotation, composed after the validated base matrix. This makes animation
speed independent of the observed 58-62 FPS presentation variation. Marker:
`kisak-ps4: build marker rotating_reference_cube_v267`.
The v2.67 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`5ade8cf52a7c0ca4852f444b5b3b76b39576128b5470e7e50ef669ba70a07332`.

The v2.67 hardware image remained identical to the fixed v2.66 cube: the MVP
update never reached a changing value. The reference example writes the same
garlic constant allocation without a cache flush, isolating the difference to
Kisak's generic POSIX clock path, which silently retained the base matrix when
`clock_gettime` failed. Version 2.68 uses OpenOrbis
`sceKernelGetProcessTime()` directly, retains a deterministic 60 Hz fallback
if the native counter is unavailable, and logs MVP updates 1 and 120 with
elapsed microseconds and two changing matrix elements. Marker:
`kisak-ps4: build marker native_timer_cube_v268`.
The v2.68 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`f30efe5d7a46bec04a4125bc6ddd2770aaad75badf607aeaf8e8333f8c379292`.

The v2.68 hardware run showed the textured cube rotating continuously, proving
native timer progression, per-frame CPU constant writes, shader constant
visibility, and repeated depth-correct drawing. Version 2.69 begins migrating
that proven draw contract out of the bootstrap and into `CPs4GnmDevice`.
`BuildPrimitiveDrawPacket` now implements D3D-style primitive-count conversion
for triangle lists/strips, line lists, and point lists, rejects zero/overflow
draws outside an open scene, and returns the corresponding OpenGNM topology.
The live cube requests 12 triangle primitives through the façade rather than
hardcoding a 36-vertex OpenGNM draw. Host tests cover every topology and the
failure boundaries. Marker:
`kisak-ps4: build marker facade_draw_primitive_v269`.
The v2.69 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`0c66bc18bcb854a6ba5198eb63504af88bed5adeeeeca5dabf74b35adf5efc21`.

The v2.69 hardware run retained the four bars and continuously spinning cube,
passing the first `CPs4GnmDevice` primitive-draw regression gate. Version 2.70
migrates the cube's remaining raw vertex setup into the façade. Its interleaved
position/UV allocation is now a `CPs4GnmBuffer`; a persistent
`CPs4GnmVertexDeclaration` describes position at byte 0 and UV at byte 24; and
the live draw binds stream 0 and regenerates both OpenGNM descriptors through
`SetStreamSource` plus `BuildVertexDescriptorTable`. The bootstrap no longer
constructs the cube's `GnmBuffer` descriptors directly. Marker:
`kisak-ps4: build marker facade_cube_stream_v270`.
The v2.70 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`69860dc88d871f9fa837c95c6dc3c36f5c556c0d38cf355650873d98a03600a7`.

The v2.70 hardware run retained the spinning textured cube and four bars at
60 FPS, validating façade-owned stream binding and vertex-declaration
translation. Version 2.71 migrates the animated MVP into
`CPs4GnmConstants`. Each frame writes four vertex constant registers, snapshots
them into the active two-frame arena, creates the matching `GnmBuffer`
descriptor in that arena, and binds the frame-local descriptor at VS slot 6.
The bootstrap's persistent hand-built constant allocation and descriptor are
removed. Marker:
`kisak-ps4: build marker facade_cube_constants_v271`.
The v2.71 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`987eaa69211486fe7a01eb14edc4ae86e55355904ec7716aab5b43cc2bb83055`.

Version 2.72 changes only the 4x4 render-to-texture diagnostic clear from
opaque black to opaque CS-style orange (`A8B8G8R8` value `0xff00a5ff`). This
turns the cube's black texture-frame regions orange while preserving the
cyan/magenta regions, animated constants, depth, stream descriptors, and draw
path. Marker: `kisak-ps4: build marker orange_cube_frame_v272`.
The v2.72 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`bed9081b85992529dec95b9b319fa8838d9a64d6ee044bc541b80536dc1f510a`.

Version 2.73 adds an actual contrast outline rather than changing more face
texels. A façade-owned 24-vertex line-list buffer describes the 12 cube edges,
expanded 1.2 percent to avoid coplanar depth flicker. It reuses the validated
cube VS, animated frame-local MVP, and sampled PS, with a dedicated 1x1 dark
navy texture descriptor. The filled cube draws first and the navy line list
draws second under the same depth target. Marker:
`kisak-ps4: build marker navy_cube_edges_v273`.
The v2.73 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`7591803a6d87491984556abe880b14a06e979032a575436dc236e10de6218c39`.

The v2.73 hardware run retained 60 FPS and visibly rendered the dark-navy
contrast lines on the spinning cube, validating the second line-list draw and
edge texture. Version 2.74 moves native texture/sampler table construction into
`CPs4GnmTexture::BuildSamplerTable`. The render-to-texture source, copied face
texture, and navy edge texture now all build their adjacent descriptor tables
through the resource façade. The helper checks validity and storage size;
host coverage verifies exact texture/sampler layout, preserved fields, and
undersized-storage rejection. Marker:
`kisak-ps4: build marker facade_sampler_tables_v274`.
The v2.74 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`238f059bb90ee628b25420c018b905dc670ab77fe2ac4794818eb7b4c0623dd4`.

The v2.74 hardware run retained the spinning cube, navy contrast regions, and
60 FPS, validating resource-owned texture/sampler tables. Version 2.75 moves
the final reference cube DB and depth/stencil controls out of direct bootstrap
emission and into `CPs4GnmDrawState`. A targeted `Invalidate` API forces those
two cached groups dirty after the offscreen/shader-shadow paths modify native
state outside the cache, ensuring the proven zeroed DB control plus enabled
LEQUAL/Z-write depth control is emitted before the cube. Marker:
`kisak-ps4: build marker cached_reference_depth_v275`.
The v2.75 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`5e4dc3aa76be53d3756718ba484a78c099638e7056327552323087fe588e1152`.

The v2.75 hardware run retained the spinning cube, navy contrast regions, and
60 FPS, validating cached restoration of the reference depth controls.
Version 2.76 adds `Ps4EmitPrimitiveDraw` beside the existing indexed façade
helper. It validates the primitive packet, applies its topology through
`CPs4GnmDrawState`, emits only dirty state, and performs the OpenGNM auto-index
draw. Both the 12-triangle filled cube and 12-line edge pass now use this
shared path; the bootstrap no longer calls `sceGnmDrawCmdDrawIndexAuto` for
either live diagnostic draw. Marker:
`kisak-ps4: build marker facade_draw_auto_v276`.
The v2.76 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`827a1abcc3c7ca4c4be4a480daf7bcc43d45870346021f767a0a179e2cecf32d`.

The v2.76 hardware run retained the spinning cube, navy contrast regions, and
60 FPS, validating shared non-indexed façade emission. Version 2.77 moves the
per-frame VideoOut color-target descriptor into
`CPs4GnmDevice::BuildDisplayRenderTarget`. The device validates initialization,
non-null 256-byte-aligned scanout memory, nonzero dimensions, pitch at least
the width, and 8-pixel pitch granularity before creating the fixed PS4 sRGB
linear target. Host tests cover aligned success and the misalignment,
undersized-pitch, and invalid-granularity failures. Marker:
`kisak-ps4: build marker facade_display_target_v277`.
The v2.77 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`02c4af59006b3855e28a8217e56a8d4531c4ae54129defab87b7eee00150e954`.

The v2.77 hardware run retained the spinning cube, navy contrast regions, and
60 FPS, validating device-owned display-target construction. Inspection then
identified the next Source-facing blocker: `CShaderDevicePs4` still exposed
shaderapiempty's zero-capability answers. Version 2.78 reports one PS4 adapter,
a 1920x1080 `IMAGE_FORMAT_BGRA8888` backbuffer/window, and graphics enabled.
It deliberately reports zero stencil bits because the current validated target
is Z32 depth-only; stencil remains a later renderer feature. Material-system
paths guarded by `IsUsingGraphics()` can now execute against the PS4 façade.
Marker: `kisak-ps4: build marker native_device_caps_v278`.
The v2.78 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`d2928e3a8cb73f23fd0c6605552aff965246af7bb5356486a90398c7678aee37`.

The v2.78 hardware run retained the spinning cube, navy contrast regions, and
60 FPS after `IsUsingGraphics()` became native. Version 2.79 replaces the
remaining fixed-console mode data delegated to shaderapiempty. Adapter 0 now
reports one 1920x1080 BGRA8888 mode at 60/1 Hz; current-mode queries return the
same deterministic record; invalid adapters/modes return an initialized empty
record; adapter selection accepts only adapter 0. Adapter identity reports AMD
vendor `0x1002`, `OpenGNM Liverpool`, and Source DX support range 90-95. The
delegate remains only for the existing initialization lifecycle. Marker:
`kisak-ps4: build marker native_display_mode_v279`.
The v2.79 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`de6757f689c52446c16281151a3f476e57d4861336c9b54b039d2be3a32dd529`.

The v2.79 hardware run retained the spinning cube, navy contrast regions, and
60 FPS with native fixed-mode enumeration. Version 2.80 supplies native
recommended configuration KeyValues for adapter 0 and DX levels 90-95:
1920x1080 fullscreen, VSYNC enabled, triple buffering disabled, MSAA disabled,
and high GPU memory level. The default recommendation is DX95. Invalid adapter,
null output, or out-of-range DX requests fail instead of inheriting empty
desktop configuration. Marker:
`kisak-ps4: build marker native_video_config_v280`.
The v2.80 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`57fd5739b394b00dc4a274a78f52bfaba9bdd3fe57cc969a91a0c9e6331b45d2`.

The v2.80 hardware run retained the spinning cube, navy contrast regions, and
60 FPS with native recommended video settings. The next resource blocker was
ownership: real Source `IVertexBuffer`/`IIndexBuffer` objects cannot use normal
heap memory on PS4. Version 2.81 adds `CPs4GnmRuntime`, a shared context that
publishes the initialized device and registers the aligned remainder of the
persistent GPU-visible pool after bootstrap shaders, depth, textures, fetch
shader, and diagnostics. Registration rejects uninitialized devices and
invalid pools; host tests cover registration, identity, capacity, and aligned
allocation. Runtime logs the exact available byte count. Marker:
`kisak-ps4: build marker shared_gpu_runtime_v281`.
The v2.81 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`af9c2acdbf7090d92ea12efb0b3fab3b28970eabaaa2ee0fce2eff5fc4e2a816`.

The v2.81 hardware run retained the spinning cube, navy contrast regions, and
60 FPS. Its new breadcrumb measured only 912,896 bytes in the shared persistent
arena after the 9,437,184-byte depth target and diagnostic resources. That is
insufficient for real Source meshes and UI buffers. Version 2.82 expands the
dedicated direct-memory pool from 16 MiB to 64 MiB and the persistent partition
from 10 MiB to 58 MiB, preserving the validated 6 MiB two-frame command arena.
Expected remaining persistent capacity is approximately 49 MiB; the existing
runtime breadcrumb will provide the exact hardware value. Marker:
`kisak-ps4: build marker expanded_gpu_pool_v282`.
The v2.82 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`08df2748c23c63424ecbbc9c382f944e6d623abb5d1da0916cf3adbda116c2c3`.

The v2.82 hardware run retained the spinning cube, navy contrast regions, and
60 FPS through frame 1200. The measured persistent arena is 51,244,544 bytes
(48.9 MiB), confirming the expanded pool. Version 2.83 adds native
`CPs4SourceVertexBuffer` and `CPs4SourceIndexBuffer` implementations over
GPU-visible `CPs4GnmBuffer` allocations. Vertex locks use Source's canonical
vertex-format layout to populate `VertexDesc_t`; index buffers support 16/32-bit
formats, append locks, and modify ranges. `CShaderDevicePs4` creates/tracks/
destroys these native objects once the runtime is ready, retaining the empty
delegate only before GPU registration or on bounded allocation failure. A
hardware probe creates, locks, writes, and unlocks one static triangle and
reports `native Source buffer probe passed`. The persistent pool is currently
monotonic; deferred reclamation remains required before map-reload soaks.
Marker: `kisak-ps4: build marker native_source_buffers_v283`.
The v2.83 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`a0aee3e36dfa1573b67a27c0e5b5598fde7aa6be7f0811596f4e5a1b06fc2dfa`.

The v2.83 hardware run remained stable at 60 FPS but showed only the four DMA
bars. Its log reached runtime registration and then reported `diagnostic shader
detail not attempted`, isolating the early return to the Source buffer probe.
The probe's first static lock incorrectly requested discard semantics;
`CPs4GnmBuffer` correctly restricts discard to a full dynamic-buffer lock.
Version 2.84 requests discard only for full non-append dynamic replacement,
while static and partial overwrite locks retain their storage. The same fix
applies to vertex and index wrappers. Expected recovery markers are
`native Source buffer probe passed`, `diagnostic shader detail ready`, and the
visible spinning cube. Marker:
`kisak-ps4: build marker static_buffer_lock_v284`.
The v2.84 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`b943a1c720077d0ee0cef49155a660f403aa34c678a0e8298546c870725cd5ab`.

The v2.84 hardware run recovered the spinning cube and navy edges at 60 FPS;
the log contains `native Source buffer probe passed` and shader detail `ready`.
Version 2.85 separates dynamic lifetime from static resources. Dynamic Source
vertex/index objects reserve no persistent memory at creation; a non-append
lock acquires fresh 256-byte-aligned backing from the active frame arena, and
append locks reuse that frame's allocation. Thus reuse is gated by the same EOP
labels as command/constant memory rather than leaking the monotonic persistent
pool. A one-shot probe runs inside the first live submission and logs
`dynamic Source buffer frame probe passed`. Static buffers remain persistent.
Marker: `kisak-ps4: build marker frame_dynamic_buffers_v285`.
The v2.85 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`86c827c23ef7c7787646b77116363546ca60fdd6917b91ba641967ef20a563ab`.

The v2.85 hardware log contains `dynamic Source buffer frame probe passed` and
continued through frame 1200 with the spinning cube at 60 FPS. Version 2.86
wires the normal `CShaderDevicePs4::GetDynamicVertexBuffer` and
`GetDynamicIndexBuffer` entry points to native wrappers. Eight vertex streams
cache one wrapper per current Source format; a format change recreates only the
CPU wrapper because backing is acquired on lock. The reusable index wrapper
provides 131,072 16-bit indices. Vertex capacity is 16,384 per stream. Append
locks validate that the active device frame still matches the allocation frame,
preventing stale cross-frame reuse. The live probe now calls these exact shader
device getters and logs `shader device dynamic buffers passed`. Marker:
`kisak-ps4: build marker shader_device_dynamic_v286`.
The v2.86 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`127b0ee9af8c5cf5375d8c3ae07d5daad12d7e9a40121cc79bec1d848b28d296`.

The v2.86 hardware run retained the spinning cube and 60 FPS. Version 2.87
bridges the two `ShaderApi029` vertex-layout calls that still returned empty
results: PS4 `VertexFormatSize` and `ComputeVertexDescription` now call the
same canonical Source layout implementation used by native buffers. This lets
mesh builders calculate nonzero strides and populate `MeshDesc_t` against
GPU-visible lock memory without duplicating the full `IShaderAPI` interface.
A hardware probe queries the actual ShaderAPI factory, verifies
`VERTEX_POSITION` is 12 bytes, and checks the returned position pointer and
actual stride; success logs `ShaderAPI vertex format bridge passed`. Marker:
`kisak-ps4: build marker shaderapi_vertex_format_v287`.
The v2.87 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`4e6615f98c325c81d221c4170f280b814c4fd3430367443c5d18af66846b857b`.

The v2.87 hardware run retained the navy-framed spinning cube and stable
60 FPS. Version 2.88 bridges dynamic `CEmptyMesh` locks to the native PS4
shader-device vertex and index buffers. Matching unlock calls now release the
active frame allocation instead of remaining empty, while initialization and
other periods without an active GPU frame retain the original dummy-memory
fallback. The bridge rejects nested locks and uses the same Source vertex
description produced by the v2.87 layout work. A live-frame probe validates a
three-position vertex lock and a three-index 16-bit lock, logging
`ShaderAPI dynamic mesh bridge passed`. Marker:
`kisak-ps4: build marker shaderapi_dynamic_mesh_v288`.
The v2.88 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`86b25efc4eeb7242e52568d5f19e89f5dce4ec6e7c46362b85f8f199e3188905`.

The v2.88 hardware run held 60 FPS but displayed black. Its log stopped the
new live probe after `shader device dynamic buffers passed`; every submission
was consequently cancelled and VideoOut used the black CPU fallback. The
probe exposed that `CPs4SourceIndexBuffer` populated `m_nIndexSize` with byte
counts, while Source defines that descriptor field in 16-bit index units
(`IndexSize() >> 1`). Version 2.89 corrects both lock paths to report 1 for
16-bit and 2 for 32-bit indices. Marker:
`kisak-ps4: build marker source_index_units_v289`.
The v2.89 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`61093e8326fd965cee51d7c7828ba9888eaf6cb51493418b92801c5d3d5b2f71`.

The v2.89 hardware run restored the four color bars, navy-framed spinning
cube, and 60 FPS. Its log includes `ShaderAPI dynamic mesh bridge passed` and
the normal indexed diagnostic draw markers. Version 2.90 binds the native
dynamic vertex and index resources into `CPs4GnmDevice` when Source unlocks a
mesh. The live bridge probe now also verifies stream 0 has the expected
12-byte position stride and that the device sees a 16-bit index binding. This
establishes the device-state boundary needed for Source draw packet emission.
Marker: `kisak-ps4: build marker source_mesh_binding_v290`.
The v2.90 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`63b65af349fb04563e8e0f65918a665551b319a2e241bfde446d30bd51c40a99`.

The v2.90 hardware run retained the expected spinning cube and 60 FPS, with
the dynamic mesh bridge and indexed diagnostic draw both passing. Version
2.91 advances the live Source mesh probe through `BeginScene`, triangle-list
topology selection, and `BuildIndexedDrawPacket`. It validates the resulting
three-index, 16-bit OpenGNM draw packet before ending the scene, covering the
last device-side boundary before emitting Source-owned mesh commands. Marker:
`kisak-ps4: build marker source_draw_packet_v291`.
The v2.91 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`995e5dfe1662371250995db3a4f6e9d6b31f28d6a914086659d7138a5dbdad9f`.

The v2.91 hardware run retained the four bars and spinning navy-framed cube at
60 FPS through frame 7200, validating the Source draw-packet gate. Version
2.92 creates a second triangle through the Source dynamic vertex/index lock
bridge using a canonical 32-byte position/normal layout, binds those resources,
builds its vertex descriptors and indexed packet, and emits it through the
shared draw-state cache into the live command buffer. The triangle occupies
the lower-left portion of the screen and uses yellow, cyan, and magenta vertex
colors so it cannot be confused with the cube. Success logs
`Source dynamic mesh command emitted`. Marker:
`kisak-ps4: build marker source_dynamic_draw_v292`.
The v2.92 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`15d406c70972df1c4f93ab39dec034a5f27e4b9a36a612eadbc2ab5d740d9277`.

The v2.92 hardware run displayed the lower-left yellow/cyan/magenta triangle
alongside the bars and spinning cube at 60 FPS, and logged
`Source dynamic mesh command emitted`. Version 2.93 marks the PS4 empty
ShaderAPI mesh as dynamic, preserves the format requested by
`GetDynamicMeshEx`, and populates the visible triangle through the actual
`IMesh::LockMesh`/`UnlockMesh` entry points. The lower-level native binding and
draw packet remain unchanged, isolating this test to the real Source mesh API
boundary. Success logs `ShaderAPI IMesh dynamic command emitted`. Marker:
`kisak-ps4: build marker shaderapi_imesh_draw_v293`.
The v2.93 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`66943859cfa30b9a3fe0c4099babd1e120c2459db3d87c1494e42ee5ab9b3e55`.

The v2.93 hardware run preserved the full diagnostic scene at 60 FPS and
logged `ShaderAPI IMesh dynamic command emitted`. Version 2.94 implements the
PS4 `CEmptyMesh::SetPrimitiveType` and `Draw` boundary. The actual `IMesh`
call now queues validated first-index/count data, maps Source triangle, strip,
line, and point topology to `CPs4GnmDevice`, and the submission path consumes
that queued request when building the packet. Invalid topology or draw ranges
remain unqueued. Success logs `IMesh Draw command emitted`. Marker:
`kisak-ps4: build marker imesh_draw_entry_v294`.
The v2.94 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`5623cb398ea8cc41e7311438c759861ce664adba45cac3d73f15c27d29208d73`.

The v2.94 hardware run retained the complete scene at 60 FPS and logged
`IMesh Draw command emitted`. Version 2.95 moves the RocketUI/Source frame
hooks inside the lifetime opened by `CPs4GnmDevice::BeginSubmission`. This
ensures future UI `GetDynamicMeshEx` locks see an active EOP-gated frame arena
instead of falling back to dummy CPU storage. The callback is cleared before
renderer shutdown and its first successful invocation logs
`Source frame callback ran inside GPU frame`. Actual queued UI draw-list
consumption remains the next boundary. Marker:
`kisak-ps4: build marker source_frame_scope_v295`.
The v2.95 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`481abbf4eee2ae5f9e133e4f84b1ab6984d8000d6871fa730f7cbd6b706eaa99`.

The v2.95 hardware run preserved the scene at 60 FPS and logged
`Source frame callback ran inside GPU frame` before the native buffer probes.
Version 2.96 applies a bounded screen scissor through `CPs4GnmDrawState` only
for the lower-left Source triangle. The right side is intentionally clipped,
providing visible evidence that per-draw scissor state is marked dirty and
emitted with the Source mesh packet. Success logs
`Source dynamic mesh scissor emitted`. Marker:
`kisak-ps4: build marker source_scissor_v296`.
The v2.96 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`2d6fc5110a11ecd99bc6744f100bbfb2b73c115ab515c5792ee5fa448a76398b`.

The v2.96 hardware run visibly clipped the right side of the Source triangle
while preserving 60 FPS and logged `Source dynamic mesh scissor emitted`.
Version 2.97 changes that triangle to alpha 0.55 and applies Source-style
SRC_ALPHA / INV_SRC_ALPHA blending through the cached OpenGNM blend control.
The underlying color bars should visibly mix through the still-clipped
triangle. Success logs `Source dynamic mesh alpha blend emitted`. Marker:
`kisak-ps4: build marker source_alpha_blend_v297`.
The v2.97 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`a917edc0b7dc48e8e7b6bfe6b835301ba9684e7c0bfcc0bf3069a3c14caf7bff`.

The v2.97 screenshot confirmed blend-state emission, but only a narrow apex
overlapped the blue band, making the visual evidence unnecessarily subtle.
Version 2.98 moves the triangle upward by 0.12 clip-space units so a much
larger region spans the blue/white boundary and lowers alpha from 0.55 to 0.45
so the background contribution is clearer. The right-side scissor remains in
place. Marker: `kisak-ps4: build marker source_blend_visible_v298`.
The v2.98 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`2d40215a1dfcd858d7fd8d3ea43deff55ed7947ec37161a24730b3a46d5a7271`.

The v2.98 screenshot made the blue/white alpha mix clear, but moving the
triangle upward also crossed the diagnostic scissor's upper boundary and
flattened its top. That was not part of the intended gate. Version 2.99 expands
the Source scissor vertically to the full 1080-line framebuffer while retaining
the right boundary at x=330. The triangle should regain its pointed top while
remaining clipped only on the right. Marker:
`kisak-ps4: build marker source_scissor_vertical_v299`.
The v2.99 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`7575439de676ddacef4713aa045da95c6524c13ee8b76dd53eb740b1a8c68591`.

The v2.99 screenshot restored the pointed top and isolated the right-side
scissor, but the interpolated yellow/cyan/magenta colors still made the blend
result difficult to distinguish from the triangle's own gradient. Version
3.00 uses uniform red at alpha 0.50 for all three Source vertices. Correct
SRC_ALPHA blending must therefore produce purple over the blue band and pink
over the white band, with no vertex-color gradient to confuse the result. The
right-side scissor remains. Marker:
`kisak-ps4: build marker source_blend_red_v300`.
The v3.00 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`d5063e85aa4c28e1c661a912be3098da4e8ef926a5dbfaa99b31afaac20237a9`.

The v3.00 hardware run rendered uniform solid red, proving the earlier pastel
appearance was not framebuffer blending. The cached blend-enable marker alone
was therefore insufficient. Version 3.01 adds the missing generic OpenGNM
blend-constant command and tests the blend unit independently of shader alpha:
the Source triangle uses CONSTANT_ALPHA / INV_CONSTANT_ALPHA with constant
0.50. Correct output remains purple over blue and pink over white. If it stays
red, the remaining defect is below shader export and blend factors in OpenGNM
PM4/render-target state. Marker:
`kisak-ps4: build marker blend_constant_v301`.
The v3.01 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`b7a68d893b77ee0c79f5f577a783a36d56a7120fc25f8e5b1321b43b07232288`.

The v3.01 constant-alpha hardware test remained solid red. This rules out
shader alpha export and confirms that the color blend result is bypassed later
in the target pipeline. The display target's `BLEND_BYPASS` bit is clear.
Version 3.02 updates OpenGNM's generic `SetBlendControl` PM4 encoding to set
`DISABLE_ROP3` whenever blending is enabled, preventing the configured COPY
ROP from replacing the blended value. Constant 0.50 remains the test factor.
Marker: `kisak-ps4: build marker blend_disable_rop3_v302`.
The v3.02 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`c6cc1ad31957ea20c0ef3be4f2fbbd7925a00eaacba22007b48d6a6612e9758e`.

The v3.02 hardware run remained solid red. Review of the earlier colored
triangle confirms that its apparent mixing was vertex-color interpolation
between its own corners, not framebuffer alpha blending. Shader alpha,
constant-alpha factors, blend enable, ROP3, and `BLEND_BYPASS` have now been
isolated. Version 3.03 changes the VideoOut GPU render-target declaration from
RGBA8 SRGB to RGBA8 UNORM without changing its memory layout or VideoOut
registration. This tests whether Liverpool's SRGB target path is suppressing
the blend unit. Marker: `kisak-ps4: build marker blend_unorm_target_v303`.
The v3.03 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`07aa1464493cd97871753693423405161b191d1bff6630fcb93669228c4a908c`.

The v3.03 UNORM target remained solid red, eliminating SRGB target mode.
Version 3.04 replaces visual inference with a bounded PM4/readback audit. It
logs the dirty mask emitted by the Source draw, the raw render-target
`CB_COLOR_INFO` word, and GPU-completed framebuffer pixels sampled from the
triangle over the blue and white bands. Marker:
`kisak-ps4: build marker blend_pm4_audit_v304`.

### v3.05: Trace final blend PM4 packets

The v3.04 hardware readback returned opaque red (`0xffff0000`) over both the
blue and white bars even though the cached draw state included the blend dirty
bit and the render target had blending enabled rather than bypassed. v3.05 adds
a bounded, one-shot scan of the submitted command stream for the final
`CB_BLEND0_CONTROL`, `CB_COLOR_CONTROL`, and `CB_BLEND_ALPHA` writes, including
write counts. This separates a missing/overwritten PM4 packet from a deeper
Liverpool blend-target requirement before changing more OpenGNM state. The
build marker is `kisak-ps4: build marker blend_packet_trace_v305`.

### v3.06: Preserve ROP3 during ordinary blending

The v3.05 PM4 audit proved the requested blend equation, normal color control,
and constant alpha all reached the final command stream without later
overwrites. It also exposed `DISABLE_ROP3` in the blend-control word. Comparison
with Mesa's GFX7/GFX8 render-state path showed that bit is not part of ordinary
UNORM blending; Mesa uses it only when an enabled logic operation cannot apply
to float or sRGB targets. OpenGNM incorrectly tied it directly to blend enable.
v3.06 removes that coupling, adds a PM4 regression test expecting blend control
`0x40011413`, and retains pixel readback as the hardware acceptance check. The
build marker is `kisak-ps4: build marker blend_rop3_fix_v306`.

### v3.07: Isolate the Source-alpha blend path

The v3.06 hardware trace showed the corrected `0x40011413` blend-control word,
but both readback samples remained opaque red. v3.07 removes the blend-constant
dependency and uses the canonical Source UI equation `SRC_ALPHA /
ONE_MINUS_SRC_ALPHA`. The dynamic vertices already carry uniform alpha `0.5`,
so successful destination reads must produce a red/blue mix over the blue bar
and pink over the white bar. The expected PM4 blend word is `0x40010504`; pixel
readback remains the acceptance test. The build marker is
`kisak-ps4: build marker source_alpha_blend_v307`.

### v3.08: Prove or reject blend-unit execution

v3.07 emitted the expected Source-alpha PM4 word but still produced opaque red
with alpha `0xff`, which is inconsistent with the programmed equation. v3.08
uses a factor-independent `ZERO / ONE` color equation. If blending executes,
the source contributes nothing and the triangle is invisible while both pixel
samples retain their bar colors. If it remains red, Liverpool is bypassing the
blend control and investigation moves to render-target/register enable state.
The expected PM4 word is `0x40010100`; the build marker is
`kisak-ps4: build marker blend_bypass_probe_v308`.

### v3.09: Disable render-target blend optimizations

v3.08 remained red with `ZERO / ONE`, proving Liverpool bypassed the programmed
blend equation. The display-linear target left both `CB_COLOR0_INFO` blend
optimization controls at automatic. v3.09 forces `BLEND_OPT_DONT_RD_DST` and
`BLEND_OPT_DISCARD_PIXEL` to `FORCE_DISABLE` while retaining the factor-
independent probe. This changes target info from `0x00008028` to `0x00908028`.
An invisible triangle proves the automatic destination-read optimization was
the bypass; continued red moves the audit to another target enable. The build
marker is `kisak-ps4: build marker blend_opt_disable_v309`.

### v3.10: Enable SIMPLE_FLOAT on OpenGNM color targets

v3.09 remained red after both blend optimizations were forced off. Comparison
of OpenGNM's color-target descriptor with Mesa's GFX6-GFX8 construction found
that OpenGNM left `CB_COLOR_INFO.SIMPLE_FLOAT` (bit 17) unset and labeled the
field unused, while Mesa enables it for every color target regardless of number
type. v3.10 names and initializes that field in OpenGNM and adds a descriptor
regression test. With the temporary optimization override retained, target info
becomes `0x00928028`; the `ZERO / ONE` probe should finally disappear if this
was the missing color-export/blend conversion enable. The build marker is
`kisak-ps4: build marker simple_float_blend_v310`.

### v3.11: Flush reused CB storage before DMA and use ZERO/ZERO

The v3.10 target contained `SIMPLE_FLOAT` but `ZERO / ONE` remained red. A
packet-order audit found that this did not prove blend bypass: `ONE` preserves
the destination, and reused VideoOut buffers already contain the prior red
triangle. More importantly, the frame wrote fresh bars with `DMA_DATA` before
issuing the color-buffer flush/invalidate, allowing dirty CB lines from the
previous use to be written back over the new bars. v3.11 flushes CB0 before the
DMA fills and retains the post-DMA acquire. It also changes the diagnostic to
`ZERO / ZERO` (`0x40010000`), which must produce zero/black independent of
source alpha, destination contents, or destination-cache freshness. The build
marker is `kisak-ps4: build marker cb_preflush_zero_probe_v311`.

### v3.12: Disable the source draw's color export mask

v3.11 emitted `ZERO / ZERO` (`0x40010000`) after pre-flushing the reused color
buffer, yet the sampled footprint remained opaque red. The packaged shader was
independently disassembled and confirmed to export FP16 RGBA with alpha `0.5`.
v3.12 sets `CB_SHADER_MASK=0` only for the final Source triangle and logs the
final shader mask, SPI color format, and `sceGnmGpuMode()`. If the triangle
disappears, the draw consumes the traced pixel state and the remaining fault is
strictly in CB blend state (including Neo/RB+ state if applicable). If red
remains, it is stale destination data or another write rather than that pixel
shader export, and the next test will split background and overlay into separate
EOP-completed submissions. The build marker is
`kisak-ps4: build marker source_mask_zero_v312`.

### v3.13: Produce blend destinations through CB

v3.12 removed the triangle when its `CB_SHADER_MASK` was zero. The trace also
reported base GPU mode, excluding Neo/RB+ `SX_MRT_BLEND_OPT` state. This proves
the final Source draw and FP16 pixel export are consumed correctly and leaves
the destination path as the major difference from working freegnm UI passes:
Kisak's bars were produced by `DMA_DATA`, whereas blending reads through CB.
v3.13 redraws the same four bars through CB using the existing fullscreen clear
shader, flushes that CB work, removes the temporary target-optimization override,
restores shader mask `0xf`, and restores `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`.
Successful blending produces purple over blue and pink over white. The build
marker is `kisak-ps4: build marker cb_native_bars_blend_v313`.

### v3.14: Make blend control the final pre-draw packet

v3.13 retained four CB-rendered bars but the Source triangle was still opaque
red, ruling out DMA/CB destination coherence. The cached state application
emitted blend control before index size, depth-target, pointer-user-data,
pixel-input, vertex-buffer, and primitive packets. v3.14 applies all cached
Source state first, then re-emits `CB_BLEND0_CONTROL=0x40010504` immediately
before `DRAW_INDEX_2`. If blending begins working, one of the intervening state
packets or its context roll was restoring the disabled blend state. Continued
red proves the final blend register write itself is ineffective and the next
probe can use a firmware/reference command or alternate target format. The
build marker is `kisak-ps4: build marker final_blend_write_v314`.

### v3.15: Make blend-last ordering part of DrawState

v3.14 visually validated alpha blending when `CB_BLEND0_CONTROL` was re-emitted
immediately before the indexed draw. This isolates the failure to a later
Source state packet/context roll restoring disabled color-buffer state. v3.15
moves blend emission to the end of `CPs4GnmDrawState::Apply`, after depth-target,
shader-I/O, descriptor, primitive, and vertex-buffer packets, then restores the
normal `Ps4EmitIndexedDraw` path. This converts the successful diagnostic
workaround into the renderer-wide ordering rule used by every Source draw. The
CPU-side framebuffer samples can remain stale without a post-EOP CPU cache
invalidation, so visual hardware output remains the acceptance signal for this
gate. The build marker is
`kisak-ps4: build marker drawstate_blend_last_v315`.

### v3.16: Establish and reassert blend state around context rolls

v3.15 regressed to opaque red. Its PM4 trace contained three blend writes per
frame, while the visually successful v3.14 contained four: the Source draw had
both its normal cached blend write and a final identical reassertion. Moving the
single write to the end was therefore not equivalent. v3.16 makes the proven
two-phase sequence renderer-wide: `CPs4GnmDrawState::Apply` establishes blend
control after pixel-shader state, emits the remaining depth/I/O/descriptor/
primitive state, then reasserts blend control at the end. The normal indexed
draw path remains in use. The expected trace returns to four writes and the
build marker is `kisak-ps4: build marker drawstate_blend_reassert_v316`.

### v3.17: Reassert blend state at the draw-packet boundary

v3.16 remained opaque red and logged five blend writes, showing that duplicating
the write inside `CPs4GnmDrawState::Apply` did not reproduce v3.14's successful
sequence. The meaningful boundary in v3.14 was after `Apply()` returned and
immediately before the hardware draw helper. v3.17 keeps the normal cached blend
write in `Apply`, exposes a narrow `ReassertBlendControl`, and calls it from both
`Ps4EmitIndexedDraw` and `Ps4EmitPrimitiveDraw` after state application and just
before `DRAW_INDEX_2`/`DRAW_INDEX_AUTO`. This makes the exact proven sequencing
generic without diagnostic-specific draw code. The build marker is
`kisak-ps4: build marker drawpacket_blend_reassert_v317`.
The v3.04 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`e3b03bef8e2a2263140a96915d14d417fd7426680e1f9777af044272436a8066`.

   Validate D-pad/left-stick focus, Cross confirm, Circle back, Options pause,
   disconnect/reconnect, and Sony button glyphs.

The console UI exit gate is the authentic Scaleform main menu rendered through
OpenGNM and navigable using only a DualShock 4. It does not require changing
the global engine meaning of `IsGameConsole()`.

Scaleform integration status (2026-07-12): the OpenOrbis build now accepts an
external `KISAK_SCALEFORM_SDK_ROOT` (default `../scaleform_sdk`), validates the
required GFx 4.2 and D3D9 HAL layout, and compiles `GFx.h` for
`x86_64-ps4-elf`. The probe verifies `SF_OS_ORBIS`, Scaleform 4.2, and the
64-bit pointer ABI, then exposes the detected `4.2.22` version in startup
diagnostics. Version 2.47 builds the generic and HeapMH Kernel inventory as an
OpenOrbis static library, links it into the monolithic executable, and runs a
bounded allocator/global-heap initialization and shutdown self-test at startup.
Thread support and AMP are disabled because this SDK snapshot has no
OpenOrbis pthread implementation and AMP prematurely pulls in the Render graph.
The staged v2.47 package is
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`accba4fd4423cee655ac272e692e10adc819cc9e4bd1bd36e8026da3b1f160c2`, with
marker `kisak-ps4: build marker scaleform_kernel_v247`. Hardware must confirm
`kisak-ps4: scaleform kernel self-test passed` before Render sources are added.
The next Scaleform slice is the Render core static library, followed by GFx/AS2
source groups and the renderer adapter.

Version 2.48 links separate Scaleform Render, GFx player, and AS2 static
libraries using the SDK inventories. Platform GL/KTX/PVR/font-provider sources,
AS3, networked AMP, and external JPEG/PNG/zlib codecs remain excluded. The
startup self-test now constructs a GFx loader and installs `AS2Support`, proving
the player/VM graph links and initializes. The remaining UI boundary is the
OpenGNM/D3D9 render HAL plus Source filesystem-backed movie loading.
Version 2.49 enables Scaleform's bundled zlib decompressor without the unused
POSIX gzip-file API and adds the first real GFx asset gate. Startup now asks the
AS2 loader to parse
`/data/kisak-strike/csgo/resource/flash/fontlib.gfx` with `LoadAll` and logs
whether movie creation passed. This is intentionally an absolute-path probe;
the next slice is a Source-filesystem `FileOpener`, followed by the OpenGNM
render HAL. Hardware validation of the movie probe is pending.

The staged package is
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`c40a44bbbc2708ede089be07c161d527454f8870b6f906081aea80c511392cc3`.
The marker is
`kisak-ps4: build marker scaleform_fontlib_probe_v249`.

The v2.49 hardware run passed the real `fontlib.gfx` parse, the two-frame EOP
test, and continued through at least frame 1200 at 60 FPS without a crash. Its
log is `hardware-captures/logs/2026-07-12/kisak_v249_scaleform_fontlib_live.txt`.
Version 2.50 replaces the probe's absolute host path with a Scaleform
`FileOpener` that resolves `resource/flash/fontlib.gfx` through Source's mounted
`GAME` search path before opening the resolved file. This is the first bridge
from GFx URL loading to Kisak's content layout and will also cover VPK-backed
content once the opener becomes a native `Scaleform::File` adapter instead of
the current resolved-path `SysFile` bridge. Hardware validation is pending.
The v2.50 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`489d63a433cc266c0f64779743d59887b1373e30e3f2acd27307aa2eba675145`,
with marker `kisak-ps4: build marker scaleform_source_fileopener_v250`.

The v2.50 hardware run passed the Source-search-path `fontlib.gfx` probe and
continued beyond frame 480 at 60 FPS. Its appended runtime capture is
`hardware-captures/logs/2026-07-12/kisak_v250_scaleform_source_fileopener_live.txt`.
Version 2.51 upgrades the bridge to read the complete GFx resource through
`IFileSystem::ReadFile("GAME")` and returns an owning Scaleform memory file.
This avoids reopening the resolved loose path and therefore supports GFx files
served by mounted VPK archives. Hardware validation is pending. The v2.51
package is staged at `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`,
SHA-256 `1361de321b3c44918a7f5ccc1f03029ee563f3f54dba6545b0534bbe7d523acc`,
with marker `kisak-ps4: build marker scaleform_vpk_fileopener_v251`.

The v2.51 hardware run passed VPK-capable `IFileSystem` movie loading and
continued beyond frame 600 at 60 FPS. Its appended capture is
`hardware-captures/logs/2026-07-12/kisak_v251_scaleform_vpk_fileopener_live.txt`.
Version 2.52 adds the next player-lifecycle gate: recreate the VPK-backed
`fontlib.gfx`, construct a live GFx movie with its first frame initialized, and
advance the AS2 timeline once. Rendering remains deliberately disconnected
until this VM/display-list boundary is hardware-stable.
The v2.52 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`805a78c55a1364846e2868ec7ad5e06ee45366e96bb06642d9956a00fa4af630`,
with marker `kisak-ps4: build marker scaleform_as2_instance_v252`.

The v2.52 hardware run passed movie creation, first-frame initialization, AS2
advance, and remained stable through at least frame 1200 at 60 FPS. Its capture
is `hardware-captures/logs/2026-07-12/kisak_v252_scaleform_as2_instance_live.txt`.
Version 2.53 forces a GFx render-tree capture and verifies that the movie's
`MovieDisplayHandle` owns a live render context. It does not call
`Renderer2D::Display` yet; this isolates the cross-thread render-tree handoff
from the forthcoming OpenGNM HAL and draw submission.
The v2.53 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg`, SHA-256
`304394411ed27d3a9a4ab32c3b08b140890de555130cec273da13c04e94838e3`,
with marker `kisak-ps4: build marker scaleform_render_tree_v253`.

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

The extracted retail PS3 game at
`/Users/bizkut/Downloads/Counter Strike Global Offensive/extracted_main`
provides a useful behavioral reference for that backend. Its loose content
includes `controller.ps3.cfg`,
`controller_bindings.ps3.cfg`, and `controller_move_bindings.ps3.cfg`, covering
axis assignments, dead zones, response curves, sensitivity, rumble scale, and
the complete gameplay binding set. Preserve Source's existing Xbox-style input
abstraction on PS4 and map DualShock 4 controls as follows: Cross/Circle/
Square/Triangle to `A_BUTTON`/`B_BUTTON`/`X_BUTTON`/`Y_BUTTON`; L1/R1 to the
shoulders; L2/R2 to the triggers; L3/R3 to `STICK1`/`STICK2`; Share to `BACK`;
Options to `START`; D-pad directions directly; and the two analog sticks to
`xmove` and `xlook`. The PS3 defaults are the initial tuning reference, not an
immutable PS4 profile; hardware tests must calibrate DS4 dead zones and trigger
ranges.

The same content contains controller help/localization plus Scaleform assets
such as `cs15_controller_flyouts_ps3_03.png.dds`,
`cs15_controller_flyouts_gamepad.png.dds`, individual stick/trigger/D-pad
textures, and the original `.gfx` HUD/menu files. RocketUI cannot execute those
Scaleform files. Recreate their navigation and flyout behavior in RML/CSS and
convert legally supplied controller artwork to package-friendly textures. Do
not commit or redistribute proprietary retail assets. `inputsystem_ps3.sprx`
is compiled proprietary PS3/PowerPC code and is reference-only: no Cell Pad,
PS3 PRX, or Move implementation may enter the PS4 build.

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
3. **Content filesystem and persistent engine loop — complete for bootstrap.**
   Layer packaged `/app0` bootstrap content with writable/external
   `/data/kisak-strike` content, normalize Source paths, mount VPKs, and replace
   the finite diagnostic loop with a quit-aware engine loop. Exit gate: load
   `gameinfo.txt`, the sound manifest, one VMT/VTF pair, and one BSP through
   normal `GAME` search paths, then shut down cleanly. All listed reads and the
   persistent quit-aware loop are hardware validated; real engine-driven map
   loading remains under milestones 4 and 8.
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
   and resolve. Pools, EOP recycling, native buffers, declarations, dynamic
   Source mesh locks/bindings, shader constants, indexed texture/depth draws,
   and immutable indexed draw-packet construction are validated. Next open the
   GPU submission frame before Source material/UI rendering and emit a
   Source-owned dynamic mesh through that command buffer.
   Exit gate: hardware clear, triangle, indexed texture, depth,
   blend, and render-to-texture tests pass without timeout or memory growth.
6. **Offline shader conversion and manifest — pending.**
   Generate the minimum UI/world combinations from Source shader metadata,
   compile HLSL to SPIR-V and then PS4 `.sb`, preserve Source register numbers,
   and emit strict binding metadata. Missing referenced combinations fail the
   package; diagnostic builds display an error shader and log the combo key.
7. **Scaleform console UI plus DualShock 4 input — pending.**
   Replace the no-device PS4 input backend with `libScePad` sampling, button and
   analog mapping, dead zones, trigger normalization, controller slots,
   disconnect/reconnect handling, and rumble. Feed Source `KEY_XBUTTON_*` and
   analog events so existing gameplay code and the PS3-derived configuration
   remain usable. Route the supplied Scaleform console movies through the
   OpenGNM-backed ShaderAPI and preserve RocketUI only as a diagnostic fallback.
   Support D-pad/left-stick focus, Cross/confirm, Circle/back, Options/pause,
   last-used-device prompt switching, and PlayStation button glyphs from
   legally supplied content. Exit gates: navigate every menu using only a DualShock 4,
   verify gameplay movement/look/actions and rumble, pass disconnect/reconnect,
   and complete a 30-minute controller-only soak.
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

## Milestone verification matrix

Every renderer milestone must pass its host checks first, then produce a fresh
hardware log with the new build marker and success breadcrumb. A stable FPS
overlay alone is not evidence that the intended GPU path executed.

1. **Source dynamic-mesh command emission — next.**
   Begin the two-frame submission before `RunFrame`/UI rendering, expose the
   active OpenGNM command buffer through the PS4 ShaderAPI, and make
   `CEmptyMesh::Draw` emit the packet built from its native dynamic buffers.
   Host tests must cover missing frame/scene, zero counts, first-index bounds,
   base vertex, 16/32-bit indices, topology mapping, nested locks, and command
   overflow. Hardware gate: a Source-created triangle is visually distinct
   from the diagnostic cube; the log records lock, bind, packet, draw, EOP,
   and flip once; the existing bars/cube remain correct for 1,200 frames.
2. **ShaderAPI state and constants.**
   Route viewport/scissor, blend, depth/stencil, cull, color mask, sRGB,
   vertex declaration, shader handles, and vertex/pixel constants through the
   dirty-state cache. Golden host tests compare D3D9-to-GNM mappings and
   constant register offsets. Hardware gate: separate opaque, blended,
   depth-occluded, scissored, and constant-animated Source meshes, with an
   explicit error shader for an invalid handle.
3. **Textures and render targets.**
   Validate 2D/cube/volume layouts, mip offsets, uploads, sampler state,
   compressed formats, render-target/depth binding, clear, copy, resolve, and
   render-to-texture. Host tests cover alignment, format rejection, swizzles,
   lifetime deferral, and pool exhaustion. Hardware gate: indexed textured
   quad, BC texture, mip selection, render-to-texture, and resolve/copy pass
   for 1,200 frames without EOP timeout or increasing memory usage.
4. **Minimum UI shader package and Scaleform HAL.**
   Compile every referenced UI combo through HLSL -> SPIR-V -> `.sb`; package
   validation must reject missing combos or invalid binding metadata. Then
   connect Scaleform Render/GFx/AS2 to the shared PS4 device façade rather than
   duplicating PM4/resource logic. Hardware gate: load `fontlib.gfx`, then
   `gameuirootmovie.gfx` and `mainmenu.gfx`, render readable text and images,
   and navigate the menu using only DualShock 4. Run a 30-minute menu soak.
5. **World-rendering progression.**
   Add shader manifests and visible gates in this order: BSP/world surfaces,
   static props, models/skinning, decals, particles, shadows, post-processing.
   For each stage, package-time combo coverage must be complete and unsupported
   formats/states must show the diagnostic shader instead of black output.
   Record per-pool high-water marks, command usage, EOP latency, frame time,
   and a 30-minute map soak before advancing.
6. **Audio, gameplay, and acceptance.**
   Test AudioOut ring wrap, underrun recovery, shutdown races, pad reconnect,
   rumble, saves, UDP loopback, and listen-server lifecycle independently.
   Acceptance requires a complete offline bot round and clean shutdown, then a
   30-minute combined soak with no memory growth, command overflow, EOP
   timeout, GPU hang, or audio starvation. Base PS4 correctness gate is stable
   30 FPS; 60 FPS becomes the optimization gate after feature completeness.

For every staged package record version, commit, SHA-256, marker, exact visual
result, last relevant log lines, FPS range, and run duration. Preserve failed
logs when they identify a new boundary; truncate or rotate `startup.log` for
each launch so stale markers cannot be mistaken for current evidence.

## Immediate implementation slice

1. **Complete:** add host-tested PS4 path normalization and root selection for
   `/app0` and `/data/kisak-strike`.
2. **Complete:** hardware validated external loose/VPK roots and replaced the
   sound-manifest fallback with a successful normal asset load.
3. **Complete:** validated representative VMT, VTF, and BSP reads through the
   normal `GAME` search path.
4. **In progress:** the PS4 ShaderAPI target, static module selection, device,
   native resource interfaces, dynamic mesh locks/bindings, and indexed packet
   construction are hardware validated at 60 FPS. Next move frame ownership
   ahead of Source rendering and emit a Source-owned dynamic mesh command.
5. **Complete:** two command/constant frame arenas submit real OpenGNM command
   buffers and gate reuse on GPU-written EOP labels; the three-submit hardware
   test passed at 60 FPS and the arenas are retained by the shared runtime.
6. **Next:** add host coverage for Source draw-command emission and a bounded
   hardware draw that cannot be confused with the existing diagnostic cube.
7. **Then:** finish blend/depth/scissor/constant state gates, followed by
   texture/render-target gates, before starting the OpenGNM Scaleform HAL.

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

### v3.18: Isolate blending on an ordinary offscreen target

v3.17 reproduced the final pre-draw blend reassertion through the reusable draw
packet helpers, but hardware still returned opaque red over both display bands.
The earlier multicolor triangle was vertex interpolation, not framebuffer
blending, so no historical run has yet proven the blend unit on this path.
v3.18 preserves the cube texture, clears its ordinary 4x4 RGBA8 render target to
zero, redraws the alpha-0.5 diagnostic through the same D3D facade, and logs four
GPU-completed pixels. Half-intensity pixels isolate the remaining problem to the
VideoOut target; full-intensity pixels prove the failure is general blend state
or OpenGNM PM4 behavior. The build marker is
`kisak-ps4: build marker offscreen_blend_isolation_v318`.

### v3.19: Present the tiled offscreen blend through the cube sampler

The v3.18 hardware run remained stable, but its four CPU-linear offscreen
samples were zero and therefore inconclusive for a tiled render target. v3.19
copies the post-blend target in its native tiled layout and presents it through
the already validated cube texture sampler. This removes CPU layout assumptions:
the cube windows now directly visualize the offscreen blend result. The build
marker is `kisak-ps4: build marker sampled_offscreen_blend_v319`.

### v3.20: Guarantee offscreen blend coverage with a fullscreen primitive

The v3.19 cube became black. This proved that it sampled the post-probe target,
but the reused indexed geometry did not guarantee coverage of the 4x4 viewport.
v3.20 uses OpenGNM's embedded fullscreen vertex shader, constant-color pixel
shader, RECTLIST primitive, and a full 4x4 scissor. It clears the ordinary target
to black, draws red at alpha 0.5 with `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`, copies
the native tiled result, and displays it on the cube. Dark red cube windows prove
offscreen blending; bright red proves blend bypass. The build marker is
`kisak-ps4: build marker fullscreen_offscreen_blend_v320`.

### v3.21: Restore complete hardware state after the fullscreen probe

The v3.20 log proved that cube and Source draw packets were emitted, but neither
appeared. The isolated embedded fullscreen shader and RECTLIST path changes
implicit hardware state beyond the fields represented by the Source cache.
v3.21 initializes default GNM hardware state after copying the probe result and
begins a fresh cached command, forcing the display pass to re-emit its complete
state. The cube should return with probe-colored windows and the direct Source
triangle should return red. The build marker is
`kisak-ps4: build marker restore_after_blend_probe_v321`.

### v3.22: Restore the visible renderer baseline

v3.21 still suppressed the cube and Source triangle despite re-emitting the
expected packets. The fullscreen blend probe therefore cannot safely share the
display command stream. v3.22 removes that inline probe and restores the last
hardware-validated sequence: textured spinning cube plus clipped red Source
triangle over four bars. Further offscreen experiments must use a separate,
EOP-completed submission. The build marker is
`kisak-ps4: build marker restore_visible_baseline_v322`.

The v3.22 hardware run restored the expected orange spinning cube and clipped
red triangle. The clean log reached frame 10,800 with continuing successful
VideoOut flips, confirming both visual recovery and long-run stability. This is
the retained baseline while the blend probe is moved to an independent
EOP-completed submission.

### v3.23: Run the blend probe in an isolated submission

v3.23 runs the fullscreen half-alpha red probe once before the normal display
frame. It uses its own command buffer, submission label, and EOP wait, then
copies the tiled result for the cube sampler. The normal frame starts afterward
from its original command stream and does not overwrite the probe texture.
The cube and clipped triangle must remain visible; dark red cube windows mean
the tiled target blended, while bright red means it bypassed blending. The build
marker is `kisak-ps4: build marker isolated_blend_submission_v323`.

The v3.23 hardware run passed the isolated EOP submission and displayed a solid
dark-red spinning cube alongside the still-opaque clipped red VideoOut triangle.
GPU readback reported `0xff000080` for the sampled tiled result, exactly the
half-red output expected from alpha 0.5 over black, while both direct scanout
samples remained `0xffff0000`. This proves OpenGNM blend state, shader alpha,
and tiled RGBA8 color-target blending are correct. The remaining limitation is
the display-linear VideoOut render target. The renderer must draw blended scene
content into a tiled intermediate and copy/resolve it into scanout memory.

### v3.24: Add the scene presentation shader

The presentation pass now has a dedicated pixel shader that samples varying UVs
from a scene texture and exports opaque RGB to the linear VideoOut target.
It is compiled offline for base PS4, packaged as `kisak_present.frag.sb`, tracked
in the strict shader manifest, and loaded through the native shader handle table.
This prepares the opaque fullscreen resolve without altering Source material
shader conventions or the validated v3.23 image.

### v3.25: Allocate the full-resolution scene color target

The proven 4x4 blend target and the VideoOut target both use
`GNM_TM_DISPLAY_LINEAR_ALIGNED`, so the remaining difference is not simply tiled
versus linear layout. v3.25 allocates a dedicated 16 MiB direct-memory pool and
constructs a 1920x1080 RGBA8 aligned scene color target plus sampler table. It
does not switch rendering yet; hardware must first validate its calculated byte
size, color-info word, allocation, and cleanup while preserving the v3.23 image.

### v3.26: Render scene into the target and resolve to VideoOut

v3.26 routes the diagnostic bars, cube, and Source triangle into the validated
1920x1080 scene target, then emits an opaque fullscreen quad using
`present_pixel` into the linear VideoOut render target. The resolve disables
blending, so the scanout surface is only written with final opaque RGB. The
previous direct path remains available when scene-target allocation fails. The
build marker is `kisak-ps4: build marker tiled_scene_resolve_v326`.

The v3.26 hardware run completed the first end-to-end resolve: the scene target
reported `bytes=8294400 info=0x00028028`, the opaque resolve breadcrumb appeared,
and the screen showed the darker-red spinning cube with the clipped transparent
triangle. VideoOut flips continued through frame 1200. This confirms the PS4
renderer can retain Source alpha blending in the scene target while presenting
opaque scanout pixels.

### v3.27: Double-buffer the scene color target

v3.27 reserves two 16 MiB scene-color slices and selects one from the OpenGNM
frame index, including a matching sampler table. This gives the renderer two
independent 1920x1080 targets for frame recycling while retaining EOP-gated
submission ownership. The visible resolve path is unchanged; the build marker
is `kisak-ps4: build marker scene_double_buffer_v327`.

The v3.27 hardware run preserved the darker-red spinning cube and transparent
clipped triangle, reached frame 1020 in the captured window, and continued the
opaque resolve path with EOP synchronization. The two-slice allocation and
frame-index selection are therefore compatible with the validated presentation.

### v3.28: Run console RocketUI menu and HUD phases in the frame loop

v3.28 invokes both `RenderMenuFrame()` and `RenderHUDFrame()` after each
RocketUI `RunFrame()` update. The PS4 bootstrap records bounded first-use
markers for the RunFrame, menu, and HUD phases, keeping console UI scheduling
inside the same source-frame callback that feeds the tiled scene resolve. The
current bootstrap UI implementation remains a no-op renderer; Scaleform/Rocket
GPU draw emission remains the next UI implementation step. The build marker is
`kisak-ps4: build marker rocketui_frame_phases_v328`.

### v3.29: Make Scaleform/GFx the production console UI

The PS4 client now treats the supplied Scaleform/GFx movies as the production
UI path. RocketUI/Rml remains only as a source-compatibility boundary while the
old module name is removed from the console launch graph. The existing
`RunFrame()`, menu, and HUD phase boundaries stay unchanged so engine timing,
input ownership, and presentation ordering do not move while the renderer is
being replaced.

The implementation is split into four contracts:

1. `CPs4ScaleformMovieManager` owns one `Scaleform::System`, a GFx `Loader`
   with the Source filesystem `FileOpener`, the font library, and menu/HUD
   `MovieDef`/`Movie` instances. It advances and captures movies on the source
   thread, sets a 1920x1080 viewport, and exposes only bounded lifecycle and
   load-state diagnostics to the bootstrap.
2. `CPs4ScaleformInputBridge` maps `InputEvent_t` button/analog/controller
   events to GFx `KeyEvent`, `CharEvent`, and `GamePadAnalogEvent` values. The
   bridge preserves Source's consume/deny-input decision and supports
   DualShock 4 disconnect/reconnect without touching ActionScript directly.
3. `CPs4ScaleformHal` is the OpenGNM backend boundary. It receives captured
   GFx display trees and translates their batches into OpenGNM vertex/index
   buffers, texture uploads, blend modes, scissor rectangles, and EOP-fenced
   submissions. No GL, ToGL, D3D9 runtime, PS3 GCM, or RSX binary is allowed in
   this path. The first implementation may reject unsupported filters with a
   visible diagnostic quad, but it must never silently drop a movie draw.
4. `CPs4ScaleformAssetManifest` packages the supplied `resource/flash/*.gfx`
   movies, font libraries, and external image/font dependencies under `/app0`;
   loose `/data/kisak-strike` content remains an opt-in override. The package
   script's `closure` mode includes every `.gfx`/`.swf` dependency; `all` also
   includes external image files, while `roots` is reserved for bring-up.
   Package validation rejects a missing movie, font, or manifest dependency
   before an eboot is staged.

The first GFx migration slice loads `mainmenu.gfx` and `fontlib.gfx`, creates a
live movie instance, advances/captures it in both existing frame phases, and
routes controller events into ActionScript. It deliberately keeps the
diagnostic OpenGNM scene visible until the HAL emits its first real GFx batch.
That gate is considered passed only when a captured GFx batch produces a
non-empty OpenGNM submission and an EOP completion marker. Subsequent slices
add texture swizzles/compression, masks and scissor, blend variants, text/font
atlases, ActionScript callbacks, and HUD/menu slot composition.

Required host gates are a movie/file-opener test, input mapping test, asset
manifest closure test, HAL blend/scissor translation test, and deterministic
captured-tree-to-command-buffer test. The PS4 gate is: load `fontlib.gfx` and
`mainmenu.gfx`, navigate one menu screen with DualShock 4, render one HUD slot,
and hold 60 FPS for 30 seconds with no EOP timeout, missing-asset marker, or
Scaleform allocation growth.

The v3.29 build marker is
`kisak-ps4: build marker scaleform_gfx_primary_v329`.

The first v3.29 implementation slice is now present. The PS4 monolithic target
uses `CPs4ScaleformMovieManager` instead of the RocketUI bootstrap, loads the
two supplied root movies through `IFileSystem::ReadFile("GAME")`, maps keyboard
and controller button events to GFx key/pad events, and captures both display
trees at the existing menu/HUD phase boundaries. The legacy full
`INCLUDE_SCALEFORM` app-system remains disabled until its large public
`IScaleformUI` surface is backed by the OpenGNM HAL; this avoids presenting a
GL/D3D9 implementation as a PS4 renderer. The current capture bridge is
intentionally diagnostic: it proves that GFx produces a live render tree, but
does not yet submit that tree as OpenGNM geometry. `CPs4ScaleformHal` now owns
the first OpenGNM blend/scissor translation and capture queue boundary; the
next code slice connects captured GFx batches to vertex/index/texture emission
and an EOP-fenced quad submission.

### v3.30: Inspect captured GFx trees without perturbing the validated scene

The next HAL slice now walks each captured `Render::TreeRoot` on the PS4 path
and records bounded node statistics (total/visible/container/shape/mesh/text
counts plus root viewport state) before the batch reaches the OpenGNM queue.
The walk is capped at 4,096 nodes and marks truncation so unusually complex
movies cannot turn the diagnostic into an unbounded per-frame traversal.
This keeps the proven dark-red spinning cube and clipped transparent triangle
diagnostic scene unchanged while proving that the live GFx tree is structurally
non-empty. Host coverage rejects null roots, resets per-frame statistics, and
retains the existing blend/scissor checks. The tree walker is excluded from
the standalone host HAL binary, so the 11-test host suite stays independent of
Scaleform runtime linkage. The first captured menu and HUD trees each emit a
bounded `scaleform tree stats phase=...` breadcrumb so hardware logs distinguish
a real GFx tree from a handle-only capture and show which slot contains drawable
nodes. The build marker is
`kisak-ps4: build marker scaleform_tree_capture_v330`.

The remaining UI gate is still real batch emission: convert the captured tree's
primitive fills into OpenGNM vertex/index/texture packets, submit them behind
the existing scene resolve, and wait for an EOP label before recycling their
frame storage. Until that gate passes, the diagnostic scene remains the
expected hardware image rather than a claim that the `.gfx` movies are visible.

### v3.31: Initialize the supplied movies like the Source console UI

The GFx manager now installs a shared `FontLib`, loads `fontlib.gfx` before the
root movies, sets the PS4 console `PlatformCode`/controller globals and slot
number, creates the `GameInterface` object, sends focus to the full-screen slot,
and invokes the legacy `InitSlot`/`ForceResize` ActionScript hooks after the
initial zero-time advance. These hooks are the missing Source-side setup that
turns an otherwise valid four-container handle into drawable movie content.
The manager logs the result for each menu/HUD slot while retaining the existing
tree-count and 4,096-node cap. The build marker is
`kisak-ps4: build marker scaleform_movie_init_v331`.

The hardware gate remains unchanged: the next run must show non-zero shape,
mesh, or text nodes in at least one slot before the OpenGNM batch emitter is
enabled.

### v3.32: Match Source root-slot and element loading

The GFx bootstrap now follows Kisak's `BaseSlot::Init` topology. The menu slot
loads `resource/flash/mainuirootmovie.gfx` and requests the `MainMenu` element;
the HUD slot loads `resource/flash/gameuirootmovie.gfx` as its independent root.
`InitSlot`, `ForceResize`, and `RequestElement` are queried and invoked through
the movie's `_global` object, matching the Source implementation instead of
calling `Movie::Invoke` on the wrapper. This keeps the existing diagnostic scene
visible while making the captured tree reflect the actual console UI roots.
The build marker is `kisak-ps4: build marker scaleform_root_slots_v332`.

The next hardware gate is a non-zero shape, mesh, or text count after the
`MainMenu` request. If the roots remain container-only, add the minimal Source
`GameInterface` callback table and request the first HUD element before enabling
OpenGNM primitive emission.

The v3.32 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`231ea437fdc51ae51017087acea5d6fedd3c56f865adbf87ab28d69ef26de1a6`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.53: Roll back unsafe nested-end continuation

The v3.52 hardware log stopped after the Scaleform kernel self-test, before the
fontlib probe, VideoOut initialization, or the first engine frame. Continuing
past zero tags at the top-level loader therefore hangs the earliest GFX parse;
those tags cannot safely be reinterpreted without repairing the corresponding
sprite-loader stream position. The SDK parser is restored to its original
bounded safety break. The console GFX root selection and all non-parser
diagnostics remain enabled, restoring the v3.51 stable fallback baseline.

Marker: `kisak-ps4: build marker scaleform_nested_end_rollback_v353`.

The v3.53 recovery package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`db76e7e60168d8359ded4a2419a29e8d0be668541a8d0d89503ef8fbcf7d9e2b`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware should
return to the v3.51 60 FPS diagnostic scene.

### v3.54: Trace the AS2 tag-to-interpreter pipeline without parser changes

The v3.53 hardware run restored the stable 60 FPS fallback through at least
frame 10,800: four color bars, the dark-red spinning cube, and the clipped
transparent triangle all remain correct. Both optimized root movies still load
one frame and capture a five-container display tree, but none of the root AS2
globals (`gfxExtensions`, `ElementLoaders`, `InitSlot`, or `RequestElement`)
exist after the bootstrap advances.

A byte-accurate host walk corrected the earlier v3.51/v3.52 diagnosis. Both
GFX roots contain 27 well-formed top-level tags; every empty `DefineSprite`
has a six-byte body containing its own nested `End`, and the root `ShowFrame`
and final `End` land exactly at the declared file length. AS2 tag registration
is also static and complete: tag 12 dispatches through `GFx_DoActionLoader` and
tag 59 through `GFx_DoInitActionLoader` whenever the loader snapshots a valid
`AS2Support` state. Continuing past a non-terminal root `End` was therefore
unsafe and is not part of this revision.

The PS4 Scaleform build now emits a bounded trace at four non-mutating
boundaries: `DoAction` dispatch, insertion into the frame playlist, timeline
execution/`ActionBuffer` queueing, and interpreter entry. The existing
stream-end safety warning also reports the actual stream position, declared
file end, tag offset, frame, and tag count. The next hardware log will identify
the first missing boundary without enabling full ActionScript disassembly or
changing parser control flow. Loader startup now confirms that AS2 support was
actually installed instead of logging the diagnostics marker before that
state assignment.

Marker: `kisak-ps4: build marker scaleform_as2_pipeline_trace_v354`.

The v3.54 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`fb9ab566404bf15ef0e48c27d79ad33ddc2db1c232d96c6d0884327488a37c41`.
The PS4 link/package build completes and all 11 host tests pass. Hardware
validation should retain the v3.53 fallback while adding `PS4 AS2 trace:`
lines before the root-global probes.

### v3.55: Recover packaged-file sizes from the opened PS4 stream

The v3.54 hardware trace crossed AS2 support capture, `DoAction` dispatch,
playlist insertion, and interpreter entry. It also exposed the real parser
boundary: the menu and HUD streams reported EOF at byte 6,389 and 6,554,
exactly where their first `DoAction` blocks end, even though their GFX headers
declare 36,676 and 36,841 bytes. Fontlib was similarly truncated to 60,812
bytes while declaring 7,496,222 bytes. GFx therefore received zero-filled tag
headers at the first `DefineSprite`; it never loaded the exported classes,
`DoInitAction` blocks, root `ShowFrame`, or final `End`.

The truncation originated in Kisak's stdio filesystem. `CStdioFile::FS_fopen`
opened `/app0` successfully, but when path-based `_stat()` failed it left the
output length uninitialized. `CFileHandle::Size()` then propagated that bogus
value into `ReadFile`, and the Scaleform memory file used it as a hard EOF.
The PS4 path now initializes the length deterministically and, when `_stat()`
fails, obtains the length by seeking the already-open stream to its end and
restoring the original position. This keeps non-PS4 behavior unchanged and
works for packaged paths that are readable through stdio but not stat-able by
name. Bounded file-opener diagnostics record actual versus header-declared
movie sizes for the next run.

The next gate is equal `scaleform file bytes=` and `declared=` values, no
premature stream-end warning, then non-zero root globals and the first
`MainMenu` element request. The stable diagnostic scene remains the fallback.

Marker: `kisak-ps4: build marker scaleform_app0_size_fallback_v355`.

The v3.55 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`71d9967f057247466d150e1aaf9abe9df1103be85424060cfc6e1b887588ad36`.
The PS4 link/package build completes and all 11 host tests pass. Hardware
validation should first confirm full movie sizes before evaluating the root
globals or Scaleform render tree.

### v3.56: Measure packaged files through their open descriptors

The v3.55 run remained stable but proved that OpenOrbis stdio does not provide
a reliable `SEEK_END` size for packaged `/app0` streams. The fallback reported
only 128 bytes for each root movie and 14,720 bytes for fontlib, while their
headers still declared 36-47 KiB and 7.49 MiB respectively. GFx could continue
past the memory-file boundary only through its zero-fill error behavior, so it
again stopped at the first class tag.

The PS4 fallback now calls `fileno()` on the successfully opened stream and
uses descriptor-based `fstat()` first. This avoids both the failing path-based
`_stat()` call and the packaged-stream `SEEK_END` behavior. The seek probe is
retained only as a final fallback if the descriptor cannot be queried. The
existing actual-versus-declared breadcrumbs will validate the result directly.

The next gate remains full file sizes, followed by loading every root class and
`DoInitAction` block, the root `ShowFrame`, non-zero AS2 globals, and the first
`MainMenu` element request.

Marker: `kisak-ps4: build marker scaleform_app0_fstat_v356`.

The v3.56 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`80634bbdf6a9361bc79f17d42e79cbf1698ba16ffe3b337ad04346038f1019e8`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.57: Query packaged-file length through the kernel descriptor offset

The v3.56 run proved descriptor `fstat()` shares the same incompatible size
result as the path-based stat ABI: roots still reported 128 bytes and fontlib
14,720 bytes. Host extraction of the exact uploaded package confirms the GP4
is correct: fontlib is 7,496,222 bytes, the menu root is 36,676 bytes, and the
HUD root is 36,841 bytes, with SHA-256 hashes identical to their staging files.
The truncation is therefore entirely in the PS4 runtime size query.

The PS4 stdio fallback now obtains the current and end offsets with `lseek()`
on the opened descriptor, restores the original descriptor position, and
clears the `FILE` error state before any buffered read occurs. This bypasses
both the incompatible stat structure and the partial `fseek` end position.
The old stdio seek path remains only if descriptor seeking itself fails.

The next gate is unchanged: actual and declared file sizes must match before
GFx parses the complete roots and exposes `InitSlot`/`RequestElement`.

Marker: `kisak-ps4: build marker scaleform_app0_lseek_v357`.

The v3.57 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`9bfe06dd4f1f0a017bba295f64c339280b2362c1af8ed5c4fdf3a4fc8e2bd0b0`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.58: Read packaged Scaleform movies without runtime size queries

The v3.57 hardware run stayed stable at 60 FPS with the dark-red spinning cube
and clipped transparent triangle, but descriptor `lseek(SEEK_END)` returned the
same false 128-byte root sizes and 14,720-byte fontlib size. This exhausts the
path stat, descriptor stat, stdio seek, and descriptor seek length mechanisms;
the extracted uploaded package remains byte-for-byte complete.

The Scaleform file opener now handles uncompressed `/app0/resource/flash`
SWF/GFX movies directly. It reads the eight-byte movie header, validates its
declared length against a 64 MiB bound, allocates that exact buffer, and fills
it with sequential `fread()` calls. No PS4 file-size or end-position API is
used. A bounded breadcrumb records actual sequential bytes versus the declared
length. The existing filesystem and compressed-movie paths remain as fallback.

The next hardware gate is exact byte equality for fontlib and both root movies,
followed by parsing beyond the former `DefineSprite` boundaries and exposure of
the root `InitSlot`/`RequestElement` hooks.

Marker: `kisak-ps4: build marker scaleform_app0_direct_read_v358`.

The v3.58 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`9aaa72fbbb6ab7f9ce24251bbc6c098ab0e7658ded2074eeaeffcda6e44b4680`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.59: Inventory GFx shape layers and fill resources

The v3.58 hardware run validated the direct packaged-movie reader completely.
Fontlib, both roots, colorlib, and sharedlib all matched their declared lengths;
the former parser truncation disappeared. Both root hook sets executed,
`MainMenu` completed its load callback, and its captured render tree contained
456 nodes: 138 shapes and 48 text nodes. The stable diagnostic fallback remains
visible because the current OpenGNM adapter only captures the tree and produces
no Scaleform meshes.

The capture traversal now inventories every shape provider by draw layer and
fill type, reporting solid, image, and gradient fill counts. This establishes
the exact primitive and resource mix the first OpenGNM mesh path must support,
without guessing from node counts or changing the proven display fallback.

The next hardware gate is a complete, stable shape inventory. The following
implementation slice will tessellate the dominant fill path into transient
OpenGNM vertex/index batches, then add texture and text-atlas resources.

Marker: `kisak-ps4: build marker scaleform_shape_inventory_v359`.

The v3.59 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`f036b230c8b61da005d4790dd275d22a32a1a287e4a08e16dae7c6bcde371592`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.60: Tessellate captured GFx shape layers on PS4

The v3.59 hardware inventory remained stable at 60 FPS and identified 150 menu
shape layers: 68 solid fills, 82 gradients, and no image fills. This makes a
shared geometry path followed by solid and gradient binding the shortest route
to the first real Scaleform pixels.

The OpenGNM adapter now walks each captured `ShapeDataInterface` path from its
layer start, preserves left/right fill styles, flattens quadratic and cubic
curves with Scaleform's tolerance rules, and submits the result to Scaleform's
native even-odd tessellator. The first menu and HUD captures report successful
layers plus generated vertex and triangle totals. Geometry collection is gated
to the first drawable capture per phase, avoiding recurring CPU tessellation in
the 60 FPS presentation loop.

The next gate is successful tessellation of all 150 menu layers without a crash
or oversized mesh. Those vertices and indices will then be retained in the
per-frame OpenGNM upload arena and emitted first through the solid-color shader;
gradient layers will reuse the same geometry with an atlas/UV binding.

Marker: `kisak-ps4: build marker scaleform_cpu_tessellation_v360`.

The v3.60 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`e87fe55fe6462c30bee10927393da1c22828f470c3a93aa50c5078f8149c3e43`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.61: Retain tessellated GFx geometry as bounded draw batches

The v3.60 hardware run tessellated all 150 menu layers successfully into 2,256
vertices and 1,954 triangles while preserving 60 FPS and the stable diagnostic
fallback. The geometry comfortably fits a small transient UI arena.

The OpenGNM adapter now copies each Scaleform tessellator mesh into persistent
CPU-side vertex and 16-bit index arrays and records its index range, vertex
range, fill color, and whether the fill is complex. Capture is bounded to
65,535 vertices, 196,605 indices, and 4,096 batches; over-limit meshes are
skipped instead of corrupting an upload. Storage is rebuilt only for the first
menu capture and retained for the upcoming GPU emission step.

The next gate is agreement between tessellated totals and retained geometry,
with valid nonzero batch counts. The following slice will apply the accumulated
GFx transforms, copy solid batches into the frame upload arena, and issue them
through the existing solid OpenGNM shader before the diagnostic overlay.

Marker: `kisak-ps4: build marker scaleform_geometry_retention_v361`.

The v3.61 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`ce4c716a57f1c430e6103fc6540a45756f17b28d22a36470cc3b956e29bd50cd`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.62: Apply accumulated GFx view transforms to retained geometry

The v3.61 hardware run retained all 1,954 triangles as 5,862 indices across
exactly 150 batches. Its 1,504 retained vertices are Scaleform's deduplicated
per-mesh representation and are consistent with the full tessellator totals.

Each shape capture now obtains Scaleform's accumulated `CalcViewMatrix()` and
applies it to every retained tessellator vertex. This includes the nested root,
element, and movie-clip transforms that local shape coordinates lacked. A
bounded marker reports the resulting minimum and maximum coordinates so the
next run can validate the GFx coordinate domain before viewport-to-clip-space
conversion and GPU submission.

The next gate is finite transformed bounds consistent with the 1280x720 movie
viewport. Once validated, the adapter will scale those coordinates to the PS4
viewport, upload solid batches, and issue the first menu geometry through the
existing OpenGNM solid shader.

Marker: `kisak-ps4: build marker scaleform_view_transform_v362`.

The v3.62 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`598f8dcc088f5c350de20e1394e890cb01eb31dda1cf605b15a64f267da2dcc0`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.63: Diagnose the collapsed GFx X transform before GPU emission

The v3.62 hardware run remained stable but its transformed bounds were
`x=960..960`, `y=-0.04..1082.55`. Issuing these vertices would collapse every
triangle into a vertical line, so GPU submission remains intentionally gated.

The first retained tessellator mesh now logs its raw bounds and complete affine
matrix. Capture also counts shape matrices whose 2x2 determinant is effectively
zero. This distinguishes bad raw vertex extraction, one malformed node, and a
systemic root/viewport matrix issue without changing the proven presentation
path.

The next gate is the matrix sample and degenerate-transform count. The adapter
will then correct the exact transform stage, revalidate finite 2D bounds, and
only afterward upload solid batches to OpenGNM.

Marker: `kisak-ps4: build marker scaleform_transform_diagnostics_v363`.

The v3.63 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`bd800aa2e4df65f6e98e9ce3d97e55a9954f4f1879b66046484d013247aa77df`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.64: Initialize the complete GFx viewport state explicitly

The v3.63 hardware trace isolated the collapse to the root viewport matrix.
Raw geometry was correct at 25,600 by 14,400 twips, but the affine sample was
`[0 0 960; 0 0.075 0]` and all 138 shape transforms were degenerate. The zero
X scale and center translation are the exact result of a zero aspect ratio;
the Y coefficient also shows a non-default viewport scale.

The movie manager now constructs `GFx::Viewport` explicitly instead of relying
on the six-argument inline helper, sets both GFx-only fields (`Scale` and
`AspectRatio`) to 1.0, and passes that complete object to `SetViewport`. It
reads the applied viewport back and logs width, height, scale, and ratio. This
fixes the root render transform at its source so Stage sizing, ActionScript
layout, rendering, and controller hit testing remain consistent.

The next gate is an applied 1920x1080 viewport with scale/ratio 1.0, zero
degenerate shape transforms, and finite two-dimensional bounds. Solid OpenGNM
submission remains gated until those conditions hold.

Marker: `kisak-ps4: build marker scaleform_viewport_fields_v364`.

The v3.64 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`7416a09cb8fb1776486948412a879db80add06810d007a7f311c00d03d7856bc`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.65: Trace the visible-frame inputs to the GFx root matrix

The v3.64 run proved the explicit viewport is applied as 1920x1080 with scale
1.0 and aspect ratio 1.0, yet the captured root remains
`[0 0 960; 0 0.075 0]`. The constructor fields are therefore correct and the
collapse occurs later in GFx viewport-matrix calculation.

The applied-viewport marker now includes the active scale mode and the movie's
pixel-space `GetVisibleFrameRect()`. Those values directly determine the X/Y
divisors in `MovieImpl::ResetViewportMatrix()`. This splits a corrupt/empty
visible width from a matrix append/ABI failure without modifying geometry or
input state.

The next gate is a finite nonzero visible width and height under `SM_NoScale`.
If the visible width is wrong, viewport update is corrected; if it is valid,
the matrix scaling implementation/configuration becomes the isolated fault.

Marker: `kisak-ps4: build marker scaleform_visible_frame_v365`.

The v3.65 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`5717ce5c3ff8af424d99045c846373e10148dace56f6eb795225cf45c1e0fe5f`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.66: Locate the degenerate ActionScript container transform

The v3.65 run confirmed every viewport-matrix input is valid: 1920x1080,
scale/aspect 1.0, `SM_NoScale`, and a visible frame of 0,0..1920,1080. The
zero X axis is therefore introduced below the root by a shared display-tree
container, most likely a ResizeManager matrix assignment.

The first captured shape now emits a bounded chain of local `M2D()` matrices
from the shape through at most twelve parents, including node types. Comparing
these local matrices with the already logged combined transform identifies the
first degenerate container without changing ActionScript execution or the
render path.

The next gate is the exact ancestor depth with zero X determinant. The fix will
be applied at that node's AS2 matrix construction/property boundary rather than
masking the invalid transform in OpenGNM geometry.

Marker: `kisak-ps4: build marker scaleform_ancestor_chain_v366`.

The v3.66 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`05c63830696612f71cbfbd9ef76a9a44ae62ddbf348bdd3b916e995b6254c8bb`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.67: Repair the malformed 1080p ResizeManager matrix at AS2 assignment

The v3.66 ancestor trace found exactly one bad local transform. Shape and
intermediate containers were identity, the root was a valid uniform 0.05 twip
conversion, but depth 3 was `[0 0 19200; 0 1.5 0]`. This is the menu's intended
uniform 1.5 ResizeManager scale with X collapsed and a compensating 960-pixel
center offset.

The Orbis AS2 transform setter now recognizes only the exact malformed
axis-aligned bootstrap matrix `[0,0,0,1.5,960,0]`, restores X scale from Y, and
removes the false center offset before converting translation to twips. Other
zero-scale matrices, including animation states, are unchanged. The Scaleform
runtime emits a diagnostic when the repair is applied.

The next gate is that repair marker, a depth-3 uniform 1.5 matrix, zero
degenerate transforms, and bounds spanning both screen axes. Solid OpenGNM
submission follows once those conditions are proven.

Marker: `kisak-ps4: build marker scaleform_resize_matrix_v367`.

The v3.67 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`ab1cc280175238a5e3c092653301ba9824a8df6fae6d2a59c71e59f053f70238`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.68: Submit the first retained Scaleform solid batches to OpenGNM

The v3.67 hardware run validated the AS2 repair. The depth-3 container is now a
uniform ~1.5 transform, the combined transform is ~0.075 on both axes, all 138
shape matrices are nondegenerate, and retained geometry spans both screen axes.
Off-screen X extents belong to menu layout/animation containers and are clipped
by the viewport.

The presentation path now consumes the retained Scaleform geometry. It filters
out complex gradient fills, converts the remaining screen-space vertices to
PS4 clip coordinates, uploads vertices and 16-bit indices through the current
frame arena, assigns each mesh's solid RGBA color, and emits one indexed draw
with normal alpha blending and a 1920x1080 scissor. The draw targets the tiled
scene color surface before the existing opaque presentation resolve, so it is
visible in the final VideoOut buffer. Gradients and text remain deliberately
disabled for this first hardware submission.

The next gate is the `scaleform solid draw` breadcrumb, continued EOP/flip
stability, and visible menu-colored geometry over the diagnostic scene. The
following slice will correct any channel/order issues, then preserve draw order
per batch and add gradient resources.

Marker: `kisak-ps4: build marker scaleform_solid_draw_v368`.

The v3.68 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`805cae0ee977298d7de8b49b2556131c97323376b9104bfb182b22ca11c09714`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.69: Preserve Scaleform's per-vertex tessellated colors

The v3.68 hardware run submitted 68 solid batches and 2,070 indices at a stable
60 FPS, proving the upload, indexed draw, blending, resolve, EOP, and flip path.
The screen became fully white because batch color lookup incorrectly treated
the tessellator's internal mesh style IDs as shape fill-style IDs; invalid or
zero IDs fell back to opaque white.

Retained vertices now carry color selected with Scaleform's own
`TessVertex::Styles` and flag rules. Simple vertices use the active style,
mixed-color vertices average both styles exactly as `acquireTessMeshes()` does,
and complex vertices remain excluded from the solid pass. Batch complexity is
also taken from tessellator flags instead of the invalid direct style lookup.
The GPU upload consumes these retained per-vertex RGBA values without replacing
them at batch level.

The next gate is restoration of the underlying diagnostic scene with visible
menu-colored solid geometry instead of an opaque white frame. Draw ordering and
gradient/text support follow after color correctness is proven.

Marker: `kisak-ps4: build marker scaleform_vertex_colors_v369`.

The v3.69 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`c751458d27ad6a3898cdfdc7f651daa6915e6e57d16b703b23839604015556e9`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.70: Add a vertex-gradient bridge for complex GFx fills

The v3.69 screenshot and log prove per-vertex solid color transport is correct:
the scene target reads `0xff38dca6`, matching the visible lime layer. That layer
is intended to combine with 82 gradient fills which were still omitted, so the
uniform palette is not another channel-order bug.

The retained tessellation path now detects gradient-backed complex fills and
samples their complete Scaleform stop records into per-vertex colors. Linear
fills interpolate across mesh X bounds; radial/focal fills use normalized
elliptical distance. Multi-stop colors use Scaleform's own `Color::Blend`.
These gradient meshes join the existing blended draw while image-backed complex
fills remain excluded. This is a CPU vertex-gradient bridge; the later full HAL
will replace it with gradient textures and exact fill matrices.

The next gate is 150 submitted color batches / 5,862 indices, stable flips, and
visible multi-color menu geometry instead of the uniform lime frame. Exact
fill-matrix mapping and text atlases follow after this geometry/color gate.

Marker: `kisak-ps4: build marker scaleform_gradient_approx_v370`.

The v3.70 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`33057e7c27de31f5f87d926abe7bbab5362dcfaeb1bd77eefd5a4ec5e71b0b9b`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.71: Sample gradients in authored GFx fill space

The v3.70 hardware run submitted all 150 retained batches and 5,862 indices at
about 59 FPS. Its additional bars and panel geometry prove gradient-backed
meshes now reach OpenGNM, but the green-heavy palette exposed the temporary
mesh-bounds sampler.

Gradient vertices now pass through each fill's `ComplexFill::ImageMatrix`, the
inverse SWF matrix that GFx already normalizes into texture space. Linear fills
sample the transformed U coordinate; radial and focal fills sample normalized
distance from the transformed texture center. This preserves authored gradient
placement across adjacent meshes instead of restarting every ramp at each mesh
edge. Exact focal-point evaluation and GPU gradient textures remain follow-ups.

The hardware gate is stable 150-batch presentation with visibly corrected
gradient placement and reduced green cast. Image fills and text atlases remain
excluded from this checkpoint.

Marker: `kisak-ps4: build marker scaleform_gradient_matrix_v371`.

The v3.71 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`27ad55a9a86c5ae2cb61d2b6ce1c0eb285e1f171757a8ce328bcfd4e87e2820c`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.72: Add interior samples to retained gradient triangles

The v3.71 hardware marker and all 150 submitted batches were present, but the
screen remained visually unchanged. Exact fill matrices therefore work at the
capture boundary, while the CPU bridge remains too sparse: radial gradients in
particular can assign the same outer stop to every corner of a large triangle,
leaving hardware interpolation with no interior color information.

Each gradient triangle now gains a centroid sampled through its authored GFx
fill matrix and is split into three equivalent triangles. Solid meshes retain
their original vertices and indices. The bounded capture remains below 16-bit
vertex and arena limits; the expected menu capture is approximately 2,768
vertices and 13,446 indices across the same 150 batches.

This is a diagnostic correctness bridge, not the final Scaleform implementation.
If interior colors become visible, the next step is OpenGNM gradient textures;
if the screen is still unchanged, image-backed fills and text atlases are the
dominant missing layers and take priority.

Marker: `kisak-ps4: build marker scaleform_gradient_interior_v372`.

The v3.72 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`9dcff2c4b269a4cbf521429b1114556980cfe45cf0743065eafe8e0f7bd286f0`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.73: Upload and sample a real OpenGNM gradient atlas

The v3.72 screenshot shows strong interior variation and visible centroid fans,
proving that gradient records and authored fill matrices are correct while the
per-vertex approximation is the remaining source of streaking.

The HAL now captures a 256-sample RGBA row for every retained gradient mesh,
packs channels explicitly for `GNM_FMT_R8G8B8A8_UNORM`, and records atlas UVs
alongside each vertex. The submission path uploads those rows into a 256x128
linear OpenGNM texture, builds a bilinear clamp sampler table, and renders the
82 gradient batches through the proven reference-cube position/UV shader path.
Solid batches remain on the color shader. Centroid refinement stays enabled for
radial coordinate approximation during this first texture-backed checkpoint.

This probe deliberately renders gradient batches after solids and the reused
pixel shader forces sampled alpha to one. The dedicated Scaleform shader pair
will restore original interleaved ordering, gradient alpha, and per-pixel radial
evaluation after the native sampled-image compiler path is ready.

The hardware gate is a stable `scaleform gradient atlas draw` marker, 82 atlas
rows/batches, smooth multi-stop ramps, and no GPU hang or frame-rate regression.

Marker: `kisak-ps4: build marker scaleform_gradient_atlas_v373`.

The v3.73 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`41e44942cb5795149fa200b05d7b51cfcc7c86f1f4d85ba54279b9cf9c3e39a6`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.74: Preserve sampled gradient alpha

The v3.73 hardware run rendered all 82 atlas rows/batches and 11,376 gradient
indices at about 62 FPS. Smooth sampled ramps and the mostly black frame prove
the atlas descriptor/UV path works: the borrowed cube pixel shader deliberately
returned sampled RGB with alpha forced to one, so full-screen transparent mask
gradients became opaque black.

Packaging now pairs the proven cube UV vertex shader with the existing
`eden-composite-blit` pixel binary, whose compatible interface returns the full
sampled RGBA value. This retains the validated texture and descriptor ABI while
restoring the alpha channel required by GFx blend state. The reference cube's
opaque texture remains visually unchanged.

The next hardware gate is the same 68 solid and 82 gradient batch split with
the underlying solid layers visible through transparent gradient regions. Draw
ordering and exact focal/radial evaluation remain later dedicated-HAL work.

Marker: `kisak-ps4: build marker scaleform_gradient_rgba_v374`.

The v3.74 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`b5c7f9e8bd5b8af82d8e682ca6db12ebe3c7bdea99cc4f5665781f1da536b2ed`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.75: Move radial gradients into 2D atlas tiles

The v3.74 screenshot proves sampled alpha is restored: the lime solid layers
remain visible beneath all 82 RGBA atlas batches. Remaining fan-shaped streaks
come from interpolating a precomputed radial distance across triangle vertices,
not from the atlas or blend state.

The HAL now assigns each gradient a 64x64 tile in a 1024x512 RGBA atlas. Linear
tiles contain horizontal ramps; radial and focal tiles contain a sampled 2D
distance field. Authored GFx fill matrices map original gradient UV directly
into each tile, with half-texel inset coordinates preventing bilinear bleed
between adjacent tiles. The centroid subdivision bridge is removed, returning
the retained menu to approximately 1,504 vertices and 5,862 indices.

Focal gradients temporarily use the radial distance field until the native GFx
focal equation is ported. The hardware gate is 82 tiles/batches, smooth radial
interiors without triangular fans, stable flips, and no frame-arena exhaustion.

Marker: `kisak-ps4: build marker scaleform_gradient_tiles_v375`.

The v3.75 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`26969b8a9c4cef92db5aa5f2b7c5bfd2cc7a9adf2e40675de7950058387f8649`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.76: Tessellate visible vector text from GFx layouts

The v3.75 run removed radial fan artifacts and rendered all 150 shape batches,
but the visible UI remained the same because 48 `TreeText` nodes were only
counted. The current capture path has no live Scaleform `Renderer2D`, text mesh
provider, or dynamic glyph cache, so text must initially be recovered directly
from each retained `TextLayout`.

The HAL now parses font, size, color, line-position, and character records;
obtains permanent glyph outlines with a temporary-shape fallback; scales them
by font size over nominal font height; and tessellates them with Scaleform's
non-zero glyph fill rule after applying the text node view matrix. Glyph batches
carry their layout color and render in a dedicated final solid pass after the
gradient atlas, preventing the current split renderer from covering text.

Bounded diagnostics report text records, usable glyph shapes, vertices,
triangles, and submitted text batches. Packed/raster glyph atlases, clipping,
color transforms, hinting, shadows, and filters remain follow-up work.

Marker: `kisak-ps4: build marker scaleform_vector_text_v376`.

The v3.76 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`3272f36c3b0a616ee2d0bc728254b9b64b15c36f11a5adce0adbfca7ef2387ec`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.77: Render packed GFx font glyphs through an atlas

The v3.76 log reports 848 text character records but zero vector glyph shapes,
proving the active CSGO fonts use Scaleform's packed texture-glyph path. The
vector fallback therefore could not alter the visible frame.

The HAL now reads each `TextureGlyph` from its embedded raw image, deduplicates
glyph/color pairs into a bounded 64x64-tile RGBA font atlas, and emits the exact
Scaleform packed-glyph quad coordinates derived from `UvBounds`, `UvOrigin`,
font size, texture glyph height, and line pen position. A8 coverage is combined
with the layout color and alpha while copying atlas pixels. Packed glyph quads
render in a final texture pass after gradients and vector text.

The gradient tiles are reduced to 32x32 in a 512x256 atlas, leaving the second
1024x512 font atlas within the three-megabyte per-frame direct-memory arena.
Diagnostics now distinguish packed records, unique atlas glyphs, and submitted
font-atlas batches. Glyphs larger than 64 pixels, non-raw images, distance-field
fonts, clipping, and multiple atlas pages remain follow-ups.

Marker: `kisak-ps4: build marker scaleform_packed_text_v377`.

The v3.77 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`99b3f4a57bd63d26224bfd614f913c7e65c14ffc054aa1dea812eaf7a5eff1cc`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.78: Resolve delegated packed-font images before CPU atlas capture

The v3.77 hardware run remained visually unchanged. Its marker-scoped log
isolates the failure before OpenGNM submission: all 848 character records were
read, but `packed=0`, `atlas_glyphs=0`, and no font-atlas draw was emitted.

Scaleform file images are commonly wrapped in `ImageDelegate`. The wrapper's
`GetImageType()` intentionally reports its enclosed image type, but the first
font-atlas bridge checked that forwarded value and then cast the wrapper itself
to `RawImage`. The non-virtual `GetImageData()` call therefore read the wrong
object and rejected every glyph. The bridge now follows `GetAsImage()` through
bounded delegate layers before the type check and cast.

Packed fonts use one-byte `Image_A8` coverage. Scaleform's legacy generic
`ImageData::GetPixel()` indexes that format as if it were four bytes per pixel,
so the bridge now reads A8 scanlines directly and keeps explicit RGBA-alpha
handling for the supported four-channel formats. Bounded diagnostics report
resolved or rejected concrete image type, format, dimensions, copied glyph
rectangle, non-zero coverage count, and maximum coverage. This makes a
`TextureImage`/`SubImage`, empty UV rectangle, or zero-alpha atlas distinguishable
on the next hardware run without perturbing the validated shape passes.

The next gate is `packed>0`, `atlas_glyphs>0`, a
`scaleform font atlas draw` marker, and visible menu text. If the image is
rejected as `Type_TextureImage`, font image creation must retain a CPU-visible
A8 backing store instead of attempting an unsafe GPU readback.

Marker: `kisak-ps4: build marker scaleform_font_delegate_v378`.

The v3.78 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`c79dab5124680b9ef0cdac3516d9e1fbea0591da3544b23c243710ffd487ae30`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.79: Resolve CS:GO font aliases before text capture

The v3.78 hardware run remained visually unchanged and reported 848 text
records but zero packed glyphs, vector shapes, or atlas glyphs. It emitted
neither a resolved-image nor rejected-image diagnostic, proving execution never
reached image extraction. An asset audit corrects the earlier packed-font
inference: `fontlib.gfx` contains nine `DefineCompactedFont` records and no
`FontTextureInfo` or font-image tags. Four records contain real compacted vector
outlines, including Stratum2 Bold and Regular; a null packed texture glyph is
therefore expected.

The root cause is missing parity with `ScaleformUIImpl::InitFonts`.
`mainmenu.gfx` imports aliases such as `$TextFontBold` from
`gfxfontlib.swf`, while the PS4 manager previously installed only `FontLib`.
Without the `FontMap` entry `$TextFontBold` to `Stratum2 Bold`, GFx searches
the library for the literal alias, fails, and substitutes a nameless empty font
with no outlines. That exactly accounts for the 848 character records and zero
renderable glyphs.

The PS4 manager now reads packaged `fontmapping.cfg` sequentially, avoiding the
known truncated `/app0` size-query path, installs its exported alias-to-face/
style mappings before loading either root movie, and registers
`fontlib_extra.swf` alongside `fontlib.gfx`. Closure and roots-only packages now
include both required files. A built-in copy of the nine English aliases fills
any missing entries when the user-supplied configuration is unavailable or
partial, keeping the bootstrap diagnosable without redistributing font assets.
Bounded per-font diagnostics report the resolved face, flags, outline/texture
capabilities, and an explicit `empty` classification.

The next hardware gate is `scaleform font map config=1 aliases=9`, a real
`Stratum2` font with `empty=0`, non-zero temporary vector glyph shapes and text
draw batches, visible menu text, stable flips, and no arena exhaustion. Packed
font-atlas output may remain zero for these assets and is no longer a success
criterion.

Marker: `kisak-ps4: build marker scaleform_font_map_v379`.

The v3.79 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`7afe94adf5c4699c4bd594d91f5f2219ce23d39dbfd6b5a18897aa4bbb376a5b`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.80: Allocate compacted glyph shapes from a Scaleform heap

The first installed v3.79 run validated the font fix before crashing. It loaded
the packaged mapping sequentially (`config=1 aliases=9`), registered both font
libraries, and resolved the first text layout to `Stratum2 Bold` with 1,077
glyphs, vector outlines enabled, and `empty=0`. The log then stopped immediately
after that font record, before the first text summary or OpenGNM submission.

The first visible character exposed a Scaleform allocator contract violation in
the v3.76 vector bridge. `GlyphShape` owns its packed path buffer through
`ArrayLH_POD`; `AllocatorBaseLH` explicitly cannot be used for stack or global
objects because it discovers the owning heap from the container address. The
bridge constructed its temporary `GlyphShape` on the PS4 thread stack, so the
first compacted Stratum outline growth passed a non-Scaleform address to
`AllocAutoHeap` and faulted. GFx's native `GlyphCache` allocates the same object
with `SF_HEAP_NEW` and retains it through a `Ptr`.

The bridge now creates one reusable `GlyphShape` per text layout from
Scaleform's global heap and keeps its `Ptr` alive through synchronous
tessellation. A bounded first-glyph trace brackets texture lookup, temporary
shape reconstruction, and tessellation, so any later failure is separated from
the allocator fix. The next hardware gate is a complete first-glyph trace,
non-zero `shapes`, `vertices`, and `triangles` in `scaleform text capture`, a
non-zero vector-text draw, visible menu text, and stable flips.

Marker: `kisak-ps4: build marker scaleform_heap_glyph_v380`.

The v3.80 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`26b4641ccceb48afca80808d8c3ffe95cd97ae4738b42472994877819fbd9aa0`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.81: Compact retained Scaleform geometry per GPU pass

The v3.80 hardware run completed the first compacted glyph trace and remained
stable through frame 7,200. It resolved Stratum2 Bold and Regular, tessellated
799 vector glyph shapes into 33,740 vertices and 49,442 triangles, and retained
35,244 vertices, 154,188 indices, and 949 batches for the complete movie. The
screen changed to an opaque lime UI layer with a faint central panel, proving
that GFx geometry now covers the diagnostic scene, but no gradients or readable
text appeared. The submission log contained only the solid-shape draw
(`68` batches and `2,070` indices); neither the gradient-atlas nor vector-text
draw was emitted.

The missing passes were deterministic frame-arena exhaustion. OpenGNM divides
the final 6 MiB of the 64 MiB direct-memory block across two frames, leaving
3 MiB per frame. Command storage and the Source diagnostic dynamic buffers use
about 852 KiB before Scaleform. Each v3.80 emitter then uploaded the *entire*
retained vertex and index arrays even when its filter selected only a small
subset. The first solid pass consumed another 1.44 MiB. The gradient pass
allocated its full 705 KiB vertex copy before its full index copy and 516 KiB
atlas failed, consuming the remaining monotonic arena space; the following text
pass therefore failed before drawing. All four return values were ignored, so
the failure was visually misleading and absent from the log.

Each retained pass now pre-counts only its selected batches, validates that
every index belongs to the batch vertex range, copies only those vertex spans,
and rebases indices into a compact pass-local vertex buffer. Exact pass-sized
allocations preserve enough arena capacity for the solid, gradient, and vector
text submissions in one frame. Conservative capacity preflights prevent an
insufficient-arena pass from partially consuming the bump arena, while bounded
rejection and up to four text-bearing pass summaries expose transient startup
results and before/after/available arena bytes. The packed-font-atlas pass may
still report `font_atlas=0`: these CS:GO font libraries contain compacted vector
outlines and no packed glyph texture.

The next hardware gate is a pass summary with `solid=1 gradient=1 text=1`,
`font_atlas=0 font_items=0` for the current compacted-vector font assets,
non-zero `scaleform gradient atlas draw` and `scaleform text draw` records, no
arena-rejection breadcrumb, visible menu gradients and text, stable flips, and
no regression in the 60 Hz presentation path.

Marker: `kisak-ps4: build marker scaleform_compact_pass_v381`.

The v3.81 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`3dca333a57a4f4dae94caac92e2db5fdf657da2378f6148e185161eb9553fe55`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.82: Restore Source localization in both GFx translation paths

The v3.81 hardware run passed the retained-upload gate and produced the first
recognizable CS:GO Scaleform menu. The screenshot shows real gradient panels,
button outlines, and Stratum2 vector text. Its clean log records the solid draw
(`68` batches), gradient-atlas draw (`82` batches), and—after the one-time
dynamic-buffer probes retired—the complete vector-text draw (`799` batches,
33,740 vertices, and 148,326 indices). The steady pass summary is
`solid=1 gradient=1 text=1 font_atlas=0 font_items=0`, with 339,536 bytes still
available in the normal 3 MiB frame arena. This validates the v3.81 compaction
and confirms that the first-frame text rejection was transient rather than a
retained-geometry failure.

Most labels still displayed literal `#SFUI_...` keys, overflowed their authored
button widths, and made the layout appear heavily overlapped. The PS4 manager
had omitted the `ScaleformTranslatorAdapter` installed by Source and its
ActionScript `GameInterface.Translate` callback returned the input unchanged.
The required `csgo_english.txt`, `valve_english.txt`, `platform_english.txt`,
and `vgui_english.txt` files are already mounted under the console content
roots; `#SFUI_MainMenu_PlayButton` resolves to `PLAY` in that table.

The PS4 bootstrap now loads the same English-first `%language%` localization
sets through `ILocalize`, installs a GFx `Translator` before either root movie
is created, strips Source's optional `@fontSize` lookup suffix, and returns
localized wide strings from both automatic text-field translation and the
ActionScript callback. Bounded diagnostics report each localization file,
the `PLAY` probe, and the first eight GFx translation hits. Short translated
labels should also substantially reduce the current 850 text records and 799
per-frame vector glyph tessellations; retained-tree/text caching remains the
next performance step if the measured rate stays below 60 Hz.

The next hardware gate is `csgo=1 probe=PLAY`, translator hits for the visible
menu keys, English labels instead of raw `#SFUI_...` strings, materially cleaner
button layout, successful solid/gradient/text pass summaries, and stable flips.
The current borrowed solid shader still forces alpha to 0.5 and the capture HAL
does not yet apply cumulative color transforms or masks; those are separate
visual-correctness milestones after localization and frame-cost measurement.

Marker: `kisak-ps4: build marker scaleform_localization_v382`.

The v3.82 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`cc95563712921963ece32dce8e5d1457571907f707289b36e83aa979b8fcf5ee`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.83: Read localization tables sequentially on PS4

The v3.82 hardware run remained stable beyond frame 1,200, but the translated
layout gate failed. The screenshot still showed the raw `#SFUI_...` labels at
about 29 FPS. The clean log confirmed the intended package marker and reported
all four `AddFile` calls as successful, while the `PLAY` probe and every bounded
GFx translator lookup were missing. The console copy of `csgo_english.txt` is
6,800,190 bytes and has SHA-256
`dc5b525ff4f8869027289f5e3ef3639185480fec3ad9d0e5b08d16eb7aa5ade4`,
identical to the source asset, so the content itself is intact.

This is the localization equivalent of the packaged-movie truncation isolated
in v3.56-v3.58. `CLocalize::ReadLocalizationFile` trusted
`IFileSystem::Size()` and treated any non-zero `ReadEx()` result as a complete
read. On the OpenOrbis runtime, a truncated size therefore returns success but
only parses the beginning of the file; the required main-menu tokens are near
line 28,000 and never enter the lookup table.

The PS4 localization loader now ignores the unreliable size query. It grows a
temporary buffer in 64 KiB chunks, repeatedly calls `ReadEx()` until the real
EOF, rejects odd-length UTF-16 data and files beyond a 32 MiB safety bound, and
only then runs the unchanged Source token parser. Other platforms retain the
original optimal-buffer path.

The next hardware gate is `csgo=1 probe=PLAY`, translator `hit=1` records for
the visible menu keys, English labels instead of raw tokens, a materially
smaller retained text workload, successful solid/gradient/text passes, and
stable flips. After this gate, visibility pruning and authored-alpha support
remain the next visual-correctness steps.

Marker: `kisak-ps4: build marker localization_sequential_read_v383`.

The v3.83 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`99bb5f3f7516f45926e3f87abe7cc0ad0f2ac24ec50e31965ed5be9653a656f6`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.84: Preserve UTF-16 values in the OpenOrbis wchar ABI

The sequential reader fixes the missing late keys, but a target-ABI audit found
a second localization defect before hardware deployment. The
`x86_64-ps4-elf` compiler defines `wchar_t` as a 16-bit unsigned type. Source's
generic POSIX `V_UCS2ToUnicode` path nevertheless asks `iconv` for UCS-4LE and
writes those four-byte code units into a 16-bit destination. A value such as
`PLAY` consequently becomes an interleaved `P, NUL, L, NUL, ...` sequence and
appears truncated even after its key is found.

`V_UCS2ToUnicode` now performs a bounded direct code-unit copy on PS4, matching
the actual OpenOrbis ABI while leaving Windows and other POSIX targets
unchanged. The sequential reader also checks `IFileSystem::IsOk()` after its
terminal zero-byte read instead of `EndOfFile()`, because Source implements the
latter as `Tell() >= Size()` and the PS4 size value is the quantity being
bypassed.

The v3.84 gate supersedes the uninstalled v3.83 package: the log must show
`probe=PLAY`, bounded translator hits with complete English values, and a
translated menu rather than single-character or raw-token labels.

Marker: `kisak-ps4: build marker localization_utf16_v384`.

The v3.84 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`01b9f5b317cd3c3db027d89e6d2622bf5016f3f93ca7be3344b621fc99ff7ba1`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.85: Prune hidden GFx trees and preserve authored alpha

The v3.84 hardware run passes the complete localization gate. Its clean log
reports `probe=PLAY` and translator hits for `BACK`, `PLAY`, and `LOCAL PLAY`.
The screenshot shows those English labels, and the shorter strings cut retained
text from 799 to 344 batches, vertices from 33,740 to 13,100, and indices from
148,326 to 57,570. Presentation recovered from roughly 29 FPS to 58.81 FPS and
remained stable beyond frame 3,600.

The remaining screenshot overlap is renderer state rather than asset or
localization failure. The custom tree walker counted `TreeNode::IsVisible()`
but tessellated every node and recursively entered invisible containers. It
also used the diagnostic fragment shader that hard-coded output alpha to 0.5,
so authored alpha-zero shapes and text could never disappear.

The capture HAL now treats an invisible node as an invisible subtree and omits
its descendants from retained geometry. A structure-and-visibility signature
is evaluated on each captured menu snapshot; geometry is rebuilt only when
that signature changes, allowing menu visibility transitions without restoring
per-frame tessellation. Up to eight rebuilds are logged for hardware diagnosis.
The packaged solid/text fragment shader is now the validated pass-through RGBA
shader instead of the fixed-alpha diagnostic variant. GFx `Advance()` already
captures modified trees, so the redundant forced menu and HUD snapshots were
also removed from the render phase.

The next hardware gate is a non-zero `hidden_subtrees` count, a materially
smaller visible shape/text set, one coherent menu state instead of overlapping
hidden panels, authored transparency, stable text/gradient/solid draws, and a
steady 60 Hz frame rate. Cumulative color transforms, masks, clip boxes, and
authored interleaved draw order remain subsequent HAL milestones.

Marker: `kisak-ps4: build marker scaleform_visibility_alpha_v385`.

The v3.85 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`77f29768d7380b474292614aaf07bf7ccd543e1ab0411178f12fb1355a9e7ce3`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.86: Complete the MainMenu element-ready transition

The v3.85 screenshot confirmed that the visibility and authored-alpha changes
work, but also exposed a missing element lifecycle callback: only the top
`PLAY!`, `BACK`, and trial-time fields remained visible, while the main option
panels disappeared. The clean capture reduced the menu tree from 456 nodes and
48 text nodes to 98 nodes and four text nodes because six authored-invisible
containers correctly hid 358 descendants.

This is not a reason to render invisible subtrees again. Kisak's original
`SFUI_BEGIN_GAME_API_DEF` always supplies `OnReady`; `mainmenu.gfx` calls
`gameAPI.OnReady()` after `InitSelectMenu()` and resizing, and
`CCreateMainMenuScreenScaleform::FlashReady()` immediately invokes the loaded
element's `showPanel()`. The PS4 generic callback table omitted `OnReady`, so
the ActionScript lifecycle stopped before the C++ show transition. The earlier
HAL made that hidden content appear only by incorrectly walking invisible
containers.

Each PS4 per-element callback object now carries its element identity and
exports `OnReady`. When the `MainMenu` element becomes ready, the callback
resolves `_global.MainMenuMovie` and invokes `showPanel()` in the same callback
order as Source. Other elements do not reopen the main menu. The runtime gate
is `scaleform element ready name=MainMenu show=1`, followed by a visibility
rebuild with the intended menu panels present, translated labels, authored
alpha, and stable 60 Hz presentation.

Marker: `kisak-ps4: build marker scaleform_mainmenu_ready_v386`.

The v3.86 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`690845a3e716e120f947b19e49df41474e59f025eff9ca3c1090f30e4cc40817`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.87: Retain animated transforms and cumulative GFx color transforms

The v3.86 hardware run passes the MainMenu lifecycle gate. Its clean log
reports `element ready name=MainMenu show=1`; the screenshot restores the
translated `PLAY`, `AWARDS`, and `OPTIONS` navigation while holding 62.34 FPS.
This confirms the v3.85 disappearance was caused by the missing `OnReady`
bridge, not by hidden-subtree pruning.

The remaining sparse, dark-olive presentation exposes two retained-HAL gaps.
The capture signature only included node type, visibility, and child count, so
ActionScript changes to position, scale, alpha, and tint after `showPanel()` did
not rebuild geometry. Shape and text colors also bypassed every local and
ancestor `Render::Cxform`. The menu could therefore freeze at an intermediate
animation state and render authored colors with the wrong alpha and tint.

The menu signature now includes each visible node's 2D matrix and color
transform, while invisible subtrees remain excluded from churn. Retained
geometry is rebuilt during ActionScript transitions and becomes static again
when the animation settles. Capture now accumulates child and ancestor
`Cxform`s in Scaleform's documented `child.Append(parent)` order and applies the
result to solid vertices, gradient-atlas samples, vector text, and packed glyph
colors. Diagnostics report the number of non-identity color-transform nodes and
bounded rebuilds. Masks, clip rectangles, authored interleaved draw order, and
bitmap fills remain subsequent HAL milestones.

The next hardware gate is several bounded rebuilds during menu entrance,
non-zero `cxforms`, a settled coherent menu with corrected tint/alpha, and a
return to stable 60 Hz after the transition.

Marker: `kisak-ps4: build marker scaleform_cxform_animation_v387`.

The v3.87 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`f294aa25390c1533edae5bc3a2050ef76e819a550b21943b6a5491589e32997a`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.88: Select console UI localization without changing engine policy

The v3.87 hardware run validates cumulative color transforms. The selected
`PLAY` item is bright while `AWARDS` and `OPTIONS` use their authored dimmed
state, `cxforms=17` is reported, and the app remains stable through frame 420
at 62.35 FPS. The initial bars and spinning cube are the expected renderer
fallback before Scaleform completes initialization and replaces the frame.

Two remaining defects are console UI policy/data mismatches. PS4 deliberately
reports `IsPC()` so the engine retains little-endian PC VPK, filesystem, and
material behavior. VGUI localization consequently accepted the final
`[$WIN32]` empty value for `SFUI_MainMenu_Navigation_Root` instead of the PS3
controller prompt. Separately, `mainmenu.gfx` calls
`GetTrialTimeRemaining`, but that callback was absent from the stripped generic
element table, producing the visible `NaN:NaN` trial banner.

Localization condition parsing now treats `[$PS3]` as true and `[$WIN32]` and
`[$X360]` as false only for `PLATFORM_PS4`; the global `IsPC()` and
`IsGameConsole()` engine policy is unchanged. The MainMenu callback table now
implements `GetTrialTimeRemaining` and returns Kisak's retained retail-console
sentinel `-1`, meaning the full game is unlocked and the trial panel should be
hidden. Rebuild heartbeats at frames 60, 120, 300, and 600 report whether
matrix/color animation continues forcing tessellation after the menu settles.

The next hardware gate is a controller navigation-help string instead of the
malformed top label, no trial-time banner, stable input highlighting, and a
rebuild heartbeat whose `changed` value returns to zero. Masks, scissor clips,
and authored interleaved draw order remain the next OpenGNM HAL work.

Marker: `kisak-ps4: build marker scaleform_console_policy_v388`.

The v3.88 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`de7de6a5d4977567f35a6e0904e6c4ecdbe99badf61a1bdf2eae06e4c7712f0b`.
The PS4 link/package build completes and all 11 host tests pass.

### v3.89: Separate retained topology from looping movie animation

The v3.88 hardware run validates the retail callback: the trial caption is
gone, the visible tree drops from four text nodes to three, and retained
vertices drop from 1,656 to 628. The controller-help lookup is still empty, so
the localization conditional override does not yet displace the shipped final
empty value.

The rebuild heartbeats identify the 24-62 FPS fluctuation precisely. The menu
rebuild count reaches 21 at frame 60, 41 at frame 120, 101 at frame 300, and
201 at frame 600, with `changed=1` at every gate. The authored movie continues
changing matrix or color state at 20 Hz, and the capture HAL retessellates the
entire visible menu at that cadence even after its entrance has settled.

Retained capture now computes two signatures. The topology signature contains
node type, visibility, and child structure; its changes always rebuild geometry.
The visual signature additionally contains matrices and color transforms. Its
changes rebuild during the first 90 frames and during a 30-frame refresh window
after handled controller input, but looping animation no longer forces
unbounded tessellation. This preserves show/hide transitions and responsive
selection highlighting while allowing static retained geometry between input
events. A readable `[X] SELECT` fallback is returned only when the PS4 root
navigation-help localization is absent or empty.

The next hardware gate is a rebuild count that stops increasing after the
entrance window, heartbeat `changed=0` from frame 120 onward, stable 60 FPS,
working controller highlight refresh, no trial caption, and readable navigation
help. The longer-term HAL replacement remains cheap per-frame transform/color
buffer refresh without tessellation, followed by masks, scissor clips, and
authored interleaved draw order.

Marker: `kisak-ps4: build marker scaleform_retained_refresh_v389`.

The PS4 link/package build completes and all 11 host tests pass. The staged
monolithic package SHA-256 is
`d17eea97d686253da7f5940d49fe127606be02b36778ec4c27c3c2810f707d78`.

The v3.89 hardware run passes the retained-refresh gate. The controller help
now displays `[X] SELECT`, the app remains stable, and the observed rate is
58-62 FPS around the 60 Hz presentation cadence. Runtime heartbeats report
`total=21 changed=1` at frame 60, then `total=31 changed=0` at frames 120,
300, and 600. Retessellation therefore stops after the bounded entrance
window instead of continuing at 20 Hz; the remaining instantaneous FPS
variation is presentation/overlay sampling jitter rather than recurring menu
capture work.

The next visual-correctness milestone is authored draw ordering and clipping.
Current submission compacts all solid shapes, gradients, vector text, and
packed glyphs into four type-based passes, which can place later fills over
earlier UI layers. Preserve the captured tree's batch order while switching
the required solid/gradient/font pipeline per batch, then attach mask-derived
clip rectangles to batches and emit them through `CPs4GnmDrawState::SetScissor`.
Add diagnostics for reordered batches, mask owners, clipped batches, and
scissor changes before enabling the behavior by default.

### v3.90: Reject sparse per-vertex gradient substitution

The v3.90 experiment submitted all 55 current menu batches through the color
pipeline in captured tree order and reduced frame-arena use from roughly 2.34
MiB to 1.81 MiB. Hardware remained stable, but the screenshot became uniformly
lime and lost the dark authored panels. Although gradient batches retain a
sampled color at each tessellation vertex, those sparse samples do not preserve
the gradient matrix/ramp across large polygons. The experiment is reverted and
the validated v3.89 solid/gradient/text pass behavior restored. Full authored
ordering must switch pipelines per ordered batch while retaining the gradient
atlas; it must not replace textured gradients with vertex interpolation.

### v3.91: Replace the PS4 no-device input stub with DualShock 4 sampling

The first interactive menu screenshot also confirms that no controller event
can reach Scaleform: `inputsystem_ps4.cpp` still deliberately reports zero
devices and never polls. The PS4 input backend now initializes UserService,
obtains the logged-in user, initializes `libScePad`, opens controller slot zero,
and samples it from Source's existing `SampleDevices` boundary. Button edges
map Cross/Circle/Square/Triangle to `A/B/X/Y`, both shoulders and triggers to
their existing Source codes, Options to Start, touchpad to Back, stick clicks,
and the four D-pad directions. Disconnects synthesize releases, reconnect state
is observed on subsequent samples, and Source rumble drives the large/small
DualShock motors.

The axis layout follows the shipped PS3 `controller.ps3.cfg` under the supplied
console content: left X/Y feed `JOY_AXIS_X/Y`, right X feeds `JOY_AXIS_U`, and
right Y feeds `JOY_AXIS_R`. A small centered dead zone prevents idle stick
jitter from generating continuous events. Bounded diagnostics record pad init,
connection/button transitions, and the mapped Scaleform key plus handled state.

The next hardware gate is a nonnegative pad handle and `count=1`, a connected
sample near centered sticks, D-pad selection movement, Cross activation,
Circle cancellation where supported, bounded Scaleform input logs, and stable
58-62 FPS without renewed retained-tree rebuild churn.

Marker: `kisak-ps4: build marker dualshock4_input_v391`.

The PS4 link/package build completes and all 11 host tests pass. The staged
monolithic package SHA-256 is
`11f92fb3dd16fd9de8cda08b3ebda61e7a87ea96d07993c9079182a755d6cb64`.

The v3.91 hardware run restores the validated dark gradient menu and retains
the bounded rebuild result through frame 600. Pad initialization succeeds:
UserService and initial-user lookup return zero, `scePadInit` returns zero, and
`scePadOpen` returns handle `50726144` with `count=1`. No `pad sample` or
`scaleform input` line follows despite user input, so the remaining controller
fault is strictly after open: Source is either not calling the PS4 poll boundary
or `scePadRead` is not producing a connected packet. Controller work is
deferred by request and the initialized backend remains in place for a later
bounded poll/read-result split.

### v3.92: Measure GFx masks and ordered pipeline runs without changing output

The stable v3.89 pass ordering remains active. Before attempting another
ordering change, retained capture now counts visible mask owners, mask-tree
nodes, and valid 2D mask view bounds. Up to eight masks report owner type, mask
type, and transformed bounds. Mask ownership bits are included in both visual
and topology signatures so a runtime mask transition cannot leave stale
retained geometry.

Capture also classifies the existing ordered batch vector into solid,
gradient, vector-text, packed-text, and complex-fill kinds and reports the
number of contiguous pipeline runs. These measurements determine whether a
per-run command list is practical and identify which mask bounds can become
conservative scissor rectangles. No draw order, shader, atlas, blend, or
scissor behavior changes in this build.

The next hardware gate is unchanged menu output at 58-62 FPS plus one
`ordered diagnostics` line and mask counts/bounds. Use those results to
implement pipeline-preserving ordered runs first, then enable scissor only for
confirmed rectangular 2D masks.

Marker: `kisak-ps4: build marker scaleform_mask_diagnostics_v392`.

The PS4 link/package build completes and all 11 host tests pass. The staged
monolithic package SHA-256 is
`feaf902b1b067a5dcdcc3aa5d122cb4b1d2c46885cfdbbfa5fe919bd9c8e257a`.

The v3.92 hardware run is stable and visually unchanged as intended. The menu
capture contains 97 total/90 visible nodes, 55 retained batches, 31 solid
batches, four gradient batches, and 20 vector-text batches. It contains no
image, packed-text, complex-fill, mask-owner, mask-tree, or mask-bounds batch.
Those 55 batches form only six contiguous type runs. Retained refresh remains
bounded (`changed=1` at frame 60 and zero at frames 120, 300, and 600).
Therefore masks and scissor are not responsible for the current flattened
menu, and the next correction can target authored ordering without adding a
mask implementation.

### v3.93: Preserve GFx order and the real gradient atlas in one draw

The current menu's three supported kinds—solid shapes, atlas gradients, and
vector text—now share a dedicated ordered shader pair. Capture is compacted in
its original 55-batch sequence into one indexed stream. Each vertex carries
position, transformed color, gradient UV, and a fill-mode selector; the pixel
shader selects transformed vertex color for solid/text primitives and samples
the existing 512x256 GFx gradient atlas for gradient primitives. This preserves
primitive blending order without the rejected v3.90 sparse per-vertex gradient
approximation and removes the four global type passes for this menu.

The old solid/gradient/text emitters remain as a deterministic fallback when
the ordered shader, gradient atlas, or arena allocation is unavailable. Packed
glyphs remain a separate final pass, but v3.92 proves the current menu has zero
packed glyphs, so this does not affect its authored ordering. Complex/image
fills are intentionally excluded until texture capture exists. The new
offline shader build script compiles GLSL to SPIR-V with `glslc`, then packages
base-PS4 shader binaries with the local `psbc`; both binaries and binding
manifest entries are packaged with the monolithic executable.

The next hardware gate is successful loading of nine shader manifest entries,
an `ordered draw` line reporting 55 batches with 31 solid/four gradient/20 text,
`passes ordered=1` with all fallback passes zero, the dark menu panels and
gradient header drawn in correct overlap order, retained 58-62 FPS, and no
arena rejection or shader metadata failure. If shader loading fails, the
startup diagnostic should name the exact ordered stage/binding mismatch before
any draw submission.

Marker: `kisak-ps4: build marker scaleform_ordered_gradient_v393`.

The PS4 link/package build and all 11 host tests pass. The staged monolithic
package SHA-256 is
`fef4424e0afbdb0f804992d13b4c35ab8f862532bcb259423971aa5d5576611f`.
Hardware validation is pending the next launch.

The v3.93 hardware run passes the ordered-rendering gate. The runtime loads all
nine shader-manifest entries, emits one ordered draw containing all 55 captured
batches (31 solid, four gradient, and 20 vector-text), and reports
`passes ordered=1` with every fallback pass disabled. No arena, metadata, or
binding failure appears. The screenshot confirms the gradient highlight now
occupies its authored position inside the dark panel instead of flattening the
whole screen, while presentation remains stable at 58-62 FPS.

### v3.94: Retire bootstrap graphics after the Scaleform menu is complete

The red/green/blue/white bars, reference cube, and clipped triangle are renderer
bring-up diagnostics rather than part of the game. They were still being drawn
under the translucent GFx menu, tinting the background and making otherwise
correct UI colors look green. The presentation path now validates that every
captured batch is supported by the ordered atlas pipeline and that its shader,
geometry, and atlas resources are present. Once that complete capture exists,
the scene starts from opaque black and skips all three bootstrap overlays. If
capture or shader setup is incomplete, the established bars/cube/triangle scene
remains the deterministic fallback.

The next hardware gate is a stable 58-62 FPS menu with no color bars, cube, or
triangle after `Scaleform menu replaced bootstrap diagnostics`, plus the same
55-batch ordered draw and zero fallback passes. The black background is
temporary until the authored background movie/image fill path is captured.

Marker: `kisak-ps4: build marker scaleform_menu_background_v394`.

The PS4 link/package build completes and all 11 host tests pass. The staged
monolithic package SHA-256 is
`444e3f3eef6abf99708d28eff929a24fdf0169d1d9a11137ce5fdb7c1f304da0`.

The v3.94 hardware run remains stable and confirms
`Scaleform menu replaced bootstrap diagnostics` before the first ordered draw.
The final image is unchanged, so the diagnostic scene was already fully
covered by opaque menu geometry; the newly visible transition is the authored
GFx/ActionScript visibility animation. The retained tree still reports zero
image fills.

### v3.95: Resolve authored external-movie URLs against packaged flash assets

`background.swf` is present in the package and contains a 1280x720 lossless
bitmap plus JPEG resources, yet the live tree contains `image=0`. The root AS2
script loads it as relative, case-sensitive `Background.swf`, while package
closure staging stores it as lowercase `resource/flash/background.swf`.

The Scaleform file opener now canonicalizes packaged lookups only: it prefixes
relative movie names with `resource/flash/`, converts ASCII path characters to
lowercase, strips query/fragment suffixes, and rejects absolute, remote, and
parent-traversal paths. GFx continues to receive the canonical movie URL so
nested imports resolve from the same flash directory. A host test covers the
relative, mixed-case, `/app0`, query-string, traversal, remote-URL, and bounded
buffer cases.

The next hardware gate is a `packaged alias` line for `Background.swf`, a full
direct `/app0` read, and nonzero image fills/texture batches in the retained
menu tree. If the alias opens but image fills remain zero, the next split is
GFx bitmap resource creation rather than URL resolution.

Marker: `kisak-ps4: build marker scaleform_background_asset_v395`.

The PS4 link/package build completes and all 12 host tests pass. The staged
monolithic package SHA-256 is
`17226a93038f01e8d9afc75d6d4641a52db291905498eb8f6ed1fa4f0e9e300f`.

The v3.95 hardware run confirms packaged aliasing for `Background.gfx`,
`Grime.gfx`, and `MainMenu.gfx`, with no element-load error, but the final
image and retained `image=0` inventory remain unchanged. Inspection of the
stripped background movie identifies eleven referenced `Background_I*.dds`
textures. None are present in the package because closure mode selected only
GFX, SWF, and the font configuration.

### v3.96: Package and read stripped-GFx DDS sidecars

Scaleform closure mode now includes top-level DDS assets alongside GFX/SWF
movies. The file opener recognizes the standard 128-byte DDS header and uses
its validated linear payload size to read the complete packaged file
sequentially. This avoids the same incorrect OpenOrbis stat/end-position APIs
that previously truncated packaged movies. DDS and movie reads have independent
bounded diagnostics so background textures remain visible after root-movie log
budgets are exhausted.

The next hardware gate is at least one `direct app0 type=dds` line with matching
actual/declared sizes and a nonzero image-fill inventory. Image fills are not
yet emitted by the ordered OpenGNM batch; once GFx exposes them in the retained
tree, the following slice will capture their image resources, UV matrices, and
compressed texture format.

Marker: `kisak-ps4: build marker scaleform_dds_sidecars_v396`.

The PS4 link/package build completes and all 12 host tests pass. Closure
packaging includes all eleven background DDS sidecars and excludes the single
legacy DDS filename containing a space, which the current `create-gp4` argument
format cannot represent safely. The staged monolithic package SHA-256 is
`2c91e1a829d16746a9252f253df8c00aa3e3d70d8fa801ee5f4b7363606684f2`.

The v3.96 hardware run is stable but requests no DDS file. Alias resolution for
the three external GFX movies still succeeds, while the display tree remains at
`image=0`. This rules out package presence and direct DDS reading: the GFx
loader had no `ImageCreator` or image-file-handler registry state, so exported
image binding stopped before calling the file opener.

### v3.97: Install CPU image creation and the GFx DDS handler

The PS4 manager now retains a default `GFx::ImageFileHandlerRegistry` and a
`GFx::ImageCreator` without a GPU texture manager, and installs both on the
loader before any font or root movie is parsed. Scaleform's built-in DDS reader
can therefore decode DXT1/DXT5 resources into CPU-compatible images while the
OpenGNM texture-manager implementation is still pending. Shutdown and failed
initialization release both states after the loader.

The next hardware gate is the image-state marker followed by matching direct
DDS reads and nonzero image fills. Once CPU images appear in retained fill
styles, the next renderer slice will extract image planes, preserve image fill
matrices/UVs, upload DXT or decoded RGBA data through `CPs4GnmTexture`, and add
image mode to the ordered shader path.

Marker: `kisak-ps4: build marker scaleform_cpu_image_creator_v397`.

The PS4 link/package build completes and all 12 host tests pass. The staged
monolithic package SHA-256 is
`b306cd6202019d3d8a287df4fcc4a0822ccb196d2fda9d52775c230631c6cf1b`.

The v3.97 hardware run satisfies the image gate. Direct packaged DDS reads
report matching actual and declared sizes, and the retained menu tree now has
30 image fills: 55 batches total with 1 solid, 4 gradients, 20 shape-text
batches, and 30 image batches. CPU image creation is therefore working. The
diagnostic bars, cube, and clipped triangle returned because the ordered draw
treated every image batch as a fatal unsupported complex fill and rejected the
entire capture, not because movie loading regressed.

### v3.98: Defer image fills without restoring bootstrap diagnostics

Image fills now have an explicit retained-batch classification distinct from
unknown complex fills. The ordered renderer validates the complete capture but
selects the currently supported solid, gradient, and shape-text batches while
deferring image batches. A valid partially supported menu capture now replaces
the diagnostic bars/cube/triangle instead of falling back merely because image
textures are present. Host coverage verifies that image batches are deferred
and never accidentally consumed by the gradient atlas path.

This is an intentional staging boundary, not completion of image rendering.
The next slice retains each image resource and its fill matrix, decodes or maps
its level-zero pixels, computes normalized UVs, uploads through
`CPs4GnmTexture`, and emits image draws in original batch order. The v3.98
hardware gate is a stable dark menu background with the existing menu text and
shapes, no diagnostic scene, and a log showing `ordered=1` despite 30 deferred
image batches.

Marker: `kisak-ps4: build marker scaleform_deferred_images_v398`.

All 12 host tests pass and the PS4 monolithic link succeeds. The staged
monolithic package SHA-256 is
`f0412e5c4dc7b3d49ceb278dccd5d83de26d45349ea5ea836487c84383d9662a`.

The v3.98 hardware run passes this boundary. The screen is a stable black
background with the menu text and `[X] SELECT`; the diagnostic bars, spinning
cube, and clipped triangle are absent. The clean log reports 25 ordered draws
and all 30 image batches deferred, with `ordered=1` and no fallback passes.
It also exposes the relevant memory limit: only about 811 KiB remains after the
ordered draw, so a separate full-size per-frame image atlas is not acceptable.

### v3.99: Pack decoded DDS controls into the ordered atlas

The HAL now deduplicates GFx image objects, retains level-zero RGBA pixels, and
records an image index plus normalized fill UVs on each image batch. It handles
R/RGBA channel orders, alpha-only images, and CPU decompression of DXT1, DXT3,
and DXT5 DDS data. Image fill matrices convert tessellated shape coordinates
into normalized source-image coordinates before the view transform.

To stay inside the v3.98 frame-arena budget, decoded images are shelf-packed
with duplicated one-pixel borders into unused rows of the existing 512x256
gradient atlas. Image batches then use that same sampler and ordered shader, so
solid, gradient, text, and image geometry remains in original draw order with
no extra large texture allocation. Oversized images remain explicitly deferred
instead of invalidating the menu capture.

Marker: `kisak-ps4: build marker scaleform_image_atlas_v399`.

The v3.99 hardware gate is nonzero `image=` and zero `deferred_images=` in the
ordered draw marker, visible controller/image artwork without diagnostic
geometry, stable 58-60 FPS, and no increase in frame-arena allocation failure.
All 12 host tests pass and the PS4 monolithic link succeeds. The staged
monolithic package SHA-256 is
`91270fef963e4f53552b73643e2acb6cb0f0b257fbcc642236665edde2efd5bc`.

The v3.99 hardware run remains stable at about 59 FPS and renders the first
cyan image tile beside `PIAYA!!`, proving the DXT decode, atlas upload, sampler,
and image shader mode all execute on PS4. The ordered marker reports 4 rendered
image batches and 26 deferred image batches. Captions enter slowly while the
log records 31 visibility-driven geometry rebuilds during the ActionScript menu
transition; the old capture path decoded image objects again on every rebuild.

### v4.00: Cache image content across animated menu rebuilds

Decoded images now retain every GFx object alias and deduplicate by dimensions
and RGBA content instead of pointer identity alone. The cache survives retained
tree rebuilds, while atlas packing considers only image indices referenced by
the current capture. This removes repeated DXT decompression during the 31-frame
menu entrance and prevents duplicate movie image objects from consuming atlas
space. Bounded startup markers record each used image's dimensions, alias
count, packing result, and atlas position to split any genuinely oversized
assets from duplicate-instance pressure.

Marker: `kisak-ps4: build marker scaleform_image_cache_v400`.

The v4.00 hardware gate is a lower stable `cached_images=` count, more than four
rendered image batches, fewer deferred images, unchanged 58-60 FPS, and faster
completion of the caption transition. All 12 host tests pass and the PS4
monolithic link succeeds. The staged monolithic package SHA-256 is
`f0aa15b278ee466817939a1b5f3b6fd1e89d245cd64a48591f0a5a844e289e9f`.

The v4.00 hardware run confirms the cache change: the same partial menu appears
materially faster and remains near 59 FPS. Its packing inventory explains the
unchanged final image: eleven real assets exceed the shared 512x256 atlas—nine
are 1024x512 and two are 512x512—while the 16x16 and three 64x64 controls pack
successfully. These are not duplicate-instance pressure and cannot fit in the
roughly 809 KiB remaining frame arena as uncompressed textures.

### v4.01: Persist large menu images outside the frame arena

Large decoded images now allocate once from the existing 58 MiB persistent GPU
arena and build retained OpenGNM texture/sampler descriptors. Small controls
continue sharing unused gradient-atlas rows. The ordered renderer compacts all
supported geometry once but emits one indexed draw per original GFx batch,
switching between the shared atlas and persistent image samplers. This restores
the original solid/gradient/text/image ordering instead of painting large
background images over menu captions in a later pass.

The expected eleven RGBA textures consume about 20 MiB of persistent GPU memory
and no additional per-frame texture storage. Failed allocations stay deferred
and are reported rather than invalidating the menu capture.

Marker: `kisak-ps4: build marker scaleform_persistent_images_v401`.

The v4.01 hardware gate is `persistent_images=11`, `image=30`, and
`deferred_images=0`, visible full menu artwork, stable presentation, and no
persistent-arena or frame-arena allocation failure. All 12 host tests pass and
the PS4 monolithic link succeeds. The staged monolithic package SHA-256 is
`58782d5fe8ed0f32c5c608ea85e3b68e45269f42c82e67b8cd2effae4f874038`.

The v4.01 hardware run passes the persistent-image gate. All eleven large
textures allocate successfully, about 28 MiB remains in the persistent arena,
and the ordered marker reports all 55 batches, all 30 image batches, and zero
deferred images. The entrance transition is smooth, the gray header and black
panel backgrounds now render, and FPS remains near 60. Most detailed artwork
is still dark or flat, making image UV generation the next correctness split.

### v4.02: Apply exported-image UV adjustment

Image vertices now apply `Image::GetMatrixInverse()` after the GFx fill matrix
and before normalization by decoded image size. This mirrors the non-texture
portion of Scaleform's `GetUVGenMatrix` path and handles gfxexport scaling or
translation attached to embedded/sub-images. Bounded image markers now include
the normalized UV bounds used by every referenced image, allowing the next
hardware run to distinguish valid 0..1 coverage from constant, inverted, or
out-of-range sampling.

Marker: `kisak-ps4: build marker scaleform_image_uv_adjust_v402`.

The v4.02 hardware gate is more detailed menu artwork with all 30 image batches
still active, or—if unchanged—a UV marker that identifies the first invalid
asset mapping precisely. All 12 host tests pass and the PS4 monolithic link
succeeds. The staged monolithic package SHA-256 is
`62bf2cb2f33ce414803539b79d3317724a56c7ede4002eadb80e89a9846a2926`.

The v4.02 hardware image is unchanged, but its bounded diagnostics identify the
exact cause: every one of the fifteen referenced images reports normalized UV
bounds `0,0..0,0`. The persistent textures, sampler switching, batch counts,
and memory budgets remain healthy. Image meshes were resolved correctly at the
batch level, but their tessellated vertices did not carry the complex-style bit,
so vertex capture entered the solid-color branch before image UV generation.

### v4.03: Prioritize resolved batch fills during vertex capture

Vertex generation now applies the mesh's resolved gradient or image fill before
consulting per-vertex solid-style flags. Image vertices therefore execute the
fill matrix, exported-image adjustment, and size normalization even when the
tessellator omits `TessStyleIsComplex` on individual vertices. Solid and mixed
vertex colors remain the fallback only when no gradient or image is resolved.

Marker: `kisak-ps4: build marker scaleform_image_vertex_uv_v403`.

The v4.03 hardware gate is nonzero image UV ranges (normally covering roughly
0..1), detailed menu imagery instead of flat sampled colors, all 55 batches,
all 30 image batches, zero deferred images, and stable presentation. All 12
host tests pass and the PS4 monolithic link succeeds. The staged monolithic
package SHA-256 is
`99a92b0329532fd5b55cc9446d9fbd2ed9661e927d6490f79fb447de3651fbb0`.

The v4.03 hardware run passes the image gate. All fifteen decoded source
images report normalized UV bounds around `0..1`; the ordered submission keeps
all 55 batches, all 30 image batches, four atlas images, eleven persistent
images, and zero deferred images. The final menu now contains the complete
authored soldier background, snow artwork, icon, panels, gradients, and text at
roughly 59 FPS.

The background moves during entrance and then settles. This is the authored
behavior, not a capture or GPU stall: `background.swf` calls the finite
`PlayFadeIn()` timeline, while its ActionScript sets `SHOULD_SNOW = false`, so
`SnowThink` is not scheduled for continuous motion. Runtime heartbeats also
confirm the intended retained result: the visual rebuild count reaches 31,
then remains unchanged with `changed=0` at frames 120, 300, and 600. Do not
restore permanent whole-tree retessellation merely to make this static menu
background move.

With the base MainMenu image path validated, the next UI milestone is the
tracked `Legals -> StartScreen -> MainMenu` stage controller. Keep direct-to-
MainMenu as the development fallback and keep StartScreen non-blocking until
the existing PS4 pad backend's post-open polling gap is resolved.

### v4.04: Restore the classic Scaleform boot-stage controller

The menu root no longer requests `MainMenu` unconditionally from `LoadSlot`.
`CPs4ScaleformMovieManager` now owns explicit Legals-loading/playing,
StartScreen-loading/waiting, MainMenu-loading, and ready states. Every request
still receives a fresh element callback object. The callback table now covers
the Legals-specific `GetRatingsBoardForLegals`, `PlayAudio`, and
`AnimationCompleted` contract; PS4 selects ESRB deterministically until region
metadata exists and records the currently silent audio callback.

The controller removes elements through the root movie's real
`_global.RemoveElement` hook. Legals advances on its authored completion
callback, StartScreen invokes `ShowStartLogo` after `OnReady`, and Cross,
Enter, or Space can complete the start screen. Because the existing pad backend
still has a post-open polling gap, a bounded three-second development timeout
also advances StartScreen to MainMenu. Load and animation timeouts preserve a
non-blocking path when supplied assets or callbacks are incomplete. Defining
`KISAK_PS4_SCALEFORM_DIRECT_MENU=1` retains the previous direct boot escape
hatch.

Marker: `kisak-ps4: build marker scaleform_classic_boot_v404`.

The v4.04 hardware gate is the complete Legals artwork followed by the
StartScreen splash/logo and exactly one MainMenu request. Expected breadcrumbs
include stages `1` through `6`, the Legals rating/audio/completion callbacks,
and `MainMenu ready`. Until AudioOut and pad polling are completed, silent
Legals audio and the offline StartScreen timeout are expected. The final menu
must retain the v4.03 full background image and stable 58-62 FPS.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`998f516303f34479af16077c47642f8c52bfee98b537caa5fdd072e1a8f6d0c7`.

The v4.04 hardware run validates the stage controller but splits two content
issues. Legals immediately reports an element load error and remains on the
diagnostic cube until its 600-frame fallback. StartScreen then loads at frame
601, renders its full 2134x1200 soldier splash and vector logo, plays the
authored entrance, and advances through the offline timeout at frame 781.
MainMenu becomes ready exactly once at frame 782 and retains the complete v4.03
background. The visible prompt is still `Press ${start} to Start` because the
minimal PS4 translator returns the localized string without Source's later
glyph-keyword replacement.

### v4.05: Prefer the console Legals movie and replace the Start glyph

The original console Scaleform URL builder selects optimized GFX movies. The
PS4 file opener now maps the authored `Legals.swf` request to packaged
`legals.gfx`, whose external `legals_*.dds` closure is already present. This
avoids the full embedded SWF path that fails before frame one on the current
runtime. Element load errors now include the element name, MovieClipLoader
error code, and argument count so a remaining GFX failure is actionable.

The PS4 localization fallback also resolves
`#SFUI_PressStartPrompt@24` directly to `Press [X] to Start`, matching the
console Cross convention until the complete Source glyph-HTML replacement
service is shared with this manager.

Marker: `kisak-ps4: build marker scaleform_legals_gfx_v405`.

The v4.05 hardware gate is visible Legals artwork and its completion callback,
then a StartScreen prompt containing `[X]` with no literal `${start}`, followed
by one MainMenu request. If Legals still fails, the new named error/code marker
is the next repair boundary; the existing timeouts must continue reaching the
validated menu.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`895e5d90398b3c66cf2df873e936283da150f110c8204a9739f2895f79110e99`.

The v4.05 hardware log explains why Legals still did not appear: the root
ActionScript rejected the request before opening either movie with `No element
named Legals defined`. Its actual `_global.ElementLoaders` key is
`LegalAnimation`; that loader then opens `Legals.swf` under the panel name
`Legals`. StartScreen and MainMenu otherwise remained stable. The run also
confirmed a valid primary pad handle and `count=1`, but no read or Scaleform
input events occurred, while the automatic StartScreen timeout continued to
dismiss the prompt without user action.

### v4.06: Use the real legal element and require controller confirmation

The boot controller now requests `LegalAnimation` and recognizes that name in
its element-specific `OnReady` callback. The existing console GFX mapping is
therefore reached only after the root accepts the element ID.

PS4 sampling no longer inherits the desktop client's archived
`joystick_force_disabled=1` default. `SampleDevices` polls the mandatory
DualShock directly, and the monolithic launcher forwards every current Source
`InputEvent_t` to `IPs4ScaleformUI::HandleInputEvent` after each poll. Bounded
read-result markers split a remaining `scePadRead` issue from event mapping.
StartScreen no longer has a successful-load timeout: it remains visible until
Cross, Enter, or Space produces a real press event. Only missing/load-failed
content retains a bounded fallback.

The color bars, cube, and triangle remain available to standalone OpenGNM
renderer tests, but the registered Source frame callback now owns application
presentation. Before the first Scaleform batch, the application uses the
neutral bootstrap clear instead of exposing those diagnostics to players.

Marker: `kisak-ps4: build marker scaleform_boot_input_v406`.

The v4.06 hardware gate is no visible diagnostic cube during normal boot,
visible Legals artwork and completion, a StartScreen that remains indefinitely
without input, one `pad sample` plus `scaleform input` marker on Cross, and a
single transition to MainMenu only after that press.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`bb1ade7c77332d52f95fc65106b7386434d0f169b4b606c8ad43c6df2188e1b7`.

The v4.06 hardware run passes real controller input. The pad reports centered
sticks and connected state, Cross produces both Source and handled Scaleform
events, StartScreen remains present until that press, and MainMenu is requested
exactly once afterward. Normal boot no longer exposes the cube or color bars.

Legals now loads through `LegalAnimation`, captures eight batches including
five images, and reaches its audio callback. Its 180-frame nested animation runs
at the movie's 30 Hz rate, however, so the 360-application-frame timeout fired
at the exact six-second boundary before `AnimationCompleted`. The outgoing
movie was consequently still visible when StartScreen and MainMenu were added;
its large Hidden Path Entertainment frame leaked into the final menu.

### v4.07: Finish and synchronously hide the Legals layer

The Legals safety timeout now allows 480 application frames, leaving a two-
second margin after the nominal 180-at-30-Hz animation duration for its real
`AnimationCompleted` callback. Stage removal synchronously sets the outgoing
movie clip `_visible=false` before invoking `_global.RemoveElement`, so even a
failed or deferred ActionScript unload cannot enter the next retained capture.
Bounded removal markers record the global member and invoke result.

Marker: `kisak-ps4: build marker scaleform_legals_unload_v407`.

The v4.07 hardware gate is a visible/legal intro or its bounded diagnostics,
no Hidden Path/ratings artwork after StartScreen begins, real Cross-controlled
transition, and the clean v4.03 MainMenu background. The log should prefer
`Legals animation completed` over the timeout and report successful removal of
both `LegalsMovie` and `StartScreenMovie`.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`cd02596545eebd340226aafc2dc80a7e1d1fbf90c055a55d9ad7053e899fae78`.

The v4.07 hardware run validates synchronous boot-layer removal: both
`LegalsMovie` and `StartScreenMovie` report successful root-hook invocation,
and the Hidden Path legal frame no longer leaks into MainMenu. Legals itself
remains black even though capture reports eight ordered batches, including
five image batches. No ordered draw or persistent image allocation occurs for
that stage. MainMenu input is handled, but each animation visibly freezes after
the 30-frame dynamic refresh window and advances again only when another button
reopens the window.

### v4.08: Accept image-only ordered scenes and finish UI transitions

`HasScaleformOrderedCapture` incorrectly required a nonempty gradient atlas and
at least one gradient tile. Legals has valid solid and image batches but no
gradients, so that unrelated requirement rejected the entire scene before
texture allocation or draw emission. Ordered capture now requires only valid
geometry/batches and the ordered shader pipeline; its existing atlas builder
creates an empty backing texture when a scene has images but no gradients.

Topology changes and handled input now request 120 capture frames instead of
30. This gives entrance, selection, and panel transitions up to two seconds to
reach their authored final state. Idle looping animation still falls back to
retained geometry afterward, preserving the prior stable-frame optimization
instead of restoring permanent full-tree tessellation.

Marker: `kisak-ps4: build marker scaleform_legals_images_v408`.

The v4.08 hardware gate is visible Legals image content with an ordered-draw
summary, clean removal before StartScreen, and menu input animations that
finish without requiring repeated button presses. After two seconds of idle,
the menu should again hold stable retained geometry near 60 FPS.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`bbd55921b5e93dcfa22afccf437abe7c244d8f7f85ea5dc4d66e42ce8bee99b3`.

The v4.08 hardware run confirms that Legals image rendering is active: the
current frame produces eight ordered batches, including five image batches and
four persistent image allocations. It remains on the Mature rating artwork
because only eight early geometry rebuilds occur before the bounded refresh
window closes. The authored Legals package contains eleven pictures in total
(seven JPEGs and four PNGs); those assets are timeline frames/variants and are
not expected to be visible simultaneously. MainMenu similarly animates only
inside a refresh window, dropping as low as 22 FPS while full-tree
retessellation is active and freezing again when the window expires.

### v4.09: Keep Scaleform timelines live with throttled retained-tree capture

The movie manager now keeps dynamic capture armed throughout Legals,
StartScreen, and MainMenu. This allows the complete legal-card timeline to
advance and keeps the MainMenu snow loop running indefinitely without relying
on controller events to reopen a temporary capture window.

GFx still advances at the 60 Hz display rate, while the OpenGNM HAL limits
expensive full retained-tree geometry rebuilds to every other frame. Cached
geometry is reused between captures, producing a 30 Hz visual sample of the
timeline while preserving 60 Hz presentation and input. Tree-topology changes
bypass the throttle and rebuild immediately. This is the correctness-first
bridge toward the planned per-node transform/color update path, which will
remove full retessellation from steady animation entirely.

Marker: `kisak-ps4: build marker scaleform_continuous_timeline_v409`.

The v4.09 hardware gate is: more than the single Mature legal image appears
before StartScreen; the StartScreen still waits for a real Cross press; menu
snow continues without input for at least two minutes; menu animations never
pause awaiting another button; presentation remains near 60 FPS without the
repeated 22 FPS floor. Record minimum/average FPS during thirty seconds of
idle snow to select the next capture cadence or prioritize the lightweight
transform update.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`e9b62c357d2a88b80dae03a7fc6c54da08d58a8d1eee8feb8e2c75e25a034b2a`.

The v4.09 hardware run validates continuous retained capture: MainMenu snow
now loops indefinitely without controller input. Legals still appears stuck
on the Mature rating, although the log reaches 30 geometry rebuilds by frame
300, executes the ratings clip's frame-90 handoff, starts the main Panel, and
fires its audio callback. This proves the remaining fault is presentation of
the changing image alpha rather than a stopped GFx timeline.

### v4.10: Apply GFx color transforms to ordered image fills

The ordered pixel shader previously used the same sampled mode for gradients
and images. That mode replaced the interpolated vertex color completely, so an
image ignored the cumulative GFx color transform captured from its tree node.
In particular, the ratings bitmap remained opaque after its timeline faded it
to alpha zero and covered all subsequent legal artwork.

Ordered batches now encode solid, gradient, and image fills as distinct shader
modes. Gradients continue using their pre-transformed atlas texels, while image
samples multiply by the captured vertex color and alpha. This restores the
common GFx fade/tint path for both packed and persistent images without
changing draw order or the v4.09 continuous-timeline cadence.

Marker: `kisak-ps4: build marker scaleform_image_cxform_v410`.

The v4.10 hardware gate is that the Mature rating fades away and at least one
following legal splash becomes visible before StartScreen. MainMenu snow must
remain continuous. The longer-term image path should carry the complete GFx
multiply/add color transform separately; v4.10 restores the multiplicative
alpha/tint behavior needed by the current boot assets.

All 12 host tests pass, the ordered shader rebuild succeeds, and the PS4
monolithic link/package build completes. The staged package SHA-256 is
`447167a8b297578b3daacd40fb2b77995319f7cfcf839d87978f89a264c684ca`.

The v4.10 hardware run validates image alpha modulation: the Mature rating now
fades in and out correctly. No Panel artwork appears afterward. The authored
flow runs the 90-frame ratings sprite followed by the 180-frame Panel at 30
FPS, but the PS4 manager's 480-display-frame timeout expires after only eight
seconds—about one second before the complete nine-second authored sequence and
before `finishAnimation()` can call `AnimationCompleted()`.

### v4.11: Match the Legals duration and probe the ratings-to-Panel handoff

The Legals timeout is extended to 660 display frames, safely beyond the 540
frames required for its 270 authored frames at 30 FPS. This permits the Panel's
frame-180 completion action to execute instead of always forcing the timeout.

Bounded timeline probes sample the ratings and Panel `_currentframe`, `_alpha`,
and `_visible` properties around the expected handoff. These distinguish an
AS2 `Panel.gotoAndPlay(2)` failure from a renderer capture/presentation failure
without enabling verbose per-frame logging.

Marker: `kisak-ps4: build marker scaleform_legals_timeline_probe_v411`.

The v4.11 hardware gate is a naturally completed Legals sequence. If Panel
remains invisible, the fresh log must show whether its playhead left frame 1
and whether its alpha/visibility became drawable; that result directly selects
the AS2 or OpenGNM fix for v4.12.

All 12 host tests pass and the PS4 monolithic link/package build completes.
The staged package SHA-256 is
`b5e87bc8c32ff901618a951fffa6bdf0a0bb43b12507e184ff55e83ba6a42eda`.

### v3.49: Preserve bounded AS2 runtime errors in the PS4 release config

The v3.48 run still exposed no root hooks, but also no ActionScript error. The
shared Scaleform release configuration undefines `GFX_AS2_VERBOSE_ERRORS`, so
the retained non-suppressing `ActionControl` could not report a failed root
`DoAction`. The PS4 config now re-enables runtime error reporting after the
shared release header while leaving opcode tracing disabled. The next capture
should identify the first unsupported/missing AS2 dependency; the stable dark-
red spinning cube and clipped transparent triangle remain the fallback.

Marker: `kisak-ps4: build marker scaleform_as2_errors_v349`.

The v3.49 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`ec622c4863c4bd6db01ef7ffeb2d0f7c48059904f27606a6b9a86c39e8e30e39`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.50: Trace root action scheduling and script-global creation

The v3.49 capture still had no hooks or AS2 errors. The root now receives one
additional advance equal to its authored frame interval after the zero-delta
bootstrap. The script marker also records the definition's loaded-frame count
and whether `gfxExtensions`, `ElementLoaders`, and `resizeManager` were created.
This separates a missing frame-action schedule from an early script dependency
failure without changing the diagnostic fallback scene.

Marker: `kisak-ps4: build marker scaleform_action_schedule_v350`.

The v3.50 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`edd7d437f878e6dc0682ecd9a1df44f55600e6bde7a8d0e258ec7d69b2f5b73a`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.51: Load the console-optimized GFX roots with the corrected bootstrap

The v3.50 hardware run reported `loading=1` but none of the earliest root
globals, proving that timeline actions never became executable. Host inspection
showed the SWF parser stopping on the first nested sprite end before the root
`ShowFrame`, while the supplied optimized `.gfx` roots contain the expected
`gfxExtensions`, `ResizeManager`, `ElementLoaders`, `RequestElement`, and
`InitSlot` symbols. The live slots now load `mainuirootmovie.gfx` and
`gameuirootmovie.gfx`, retaining the deferred first advance, console globals,
per-element callback object, `SM_NoScale`, and bounded AS2 diagnostics.

The next hardware gate is non-zero script-global probes and a successful
`MainMenu` request. The dark-red cube and clipped transparent triangle remain
the fallback until the captured GFx tree contains drawable nodes.

Marker: `kisak-ps4: build marker scaleform_console_gfx_roots_v351`.

The v3.51 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`018d827e94c8febb677633c6eba93775cf22af032eacfe9641ffa55371a44557`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.52: Continue through optimized-root nested end tags

The v3.51 hardware run confirmed that the optimized GFX roots still stopped at
the first internal zero tag before any root ActionScript global was created.
The Scaleform loader now treats a non-terminal zero tag as an empty nested
sprite terminator only while the declared root frames are incomplete. It keeps
the original safety stop after all declared frames load, so malformed trailing
data is not accepted. This allows the root class `DoInitAction` blocks and final
`ShowFrame` to load before the first movie advance.

The next gate is `gfx=1`, followed by `elements=1`, `init=1`, and the initial
`MainMenu` request. Marker:
`kisak-ps4: build marker scaleform_nested_end_v352`.

The v3.52 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`509bc5b87e2873e05415d2c4b3540ac0b0c977c32faccb2988c2f6a5ff5647a8`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.46: Execute the first root timeline tick before querying hooks

The v3.45 hardware run shows that `LoadWaitFrame1` succeeds without using the
fallback, but the root globals remain absent and no AS2 error is emitted.
`CreateInstance(true)` guarantees frame-one display objects, not necessarily
execution of queued frame actions, and a zero-time advance can leave those
actions pending. Each root now advances by one 60 Hz tick after the console
globals and GameInterface are installed, then queries both the legacy short
paths and fully qualified `_global.InitSlot`/`_global.RequestElement` paths.

The movie remains a single frame, so this does not skip timeline content. The
next gate is a qualified global hook becoming available or a new AS2 loader
error that isolates the first failed action. Marker:
`kisak-ps4: build marker scaleform_first_tick_v346`.

The v3.46 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`233572e8560e24c9cb0652fb665871d7f74578dbc7fde92b499e5e34c116be88`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.47: Match BaseSlot scaling and isolate per-element callback state

The v3.46 capture kept both root movies stable but still reported no
`InitSlot`/`RequestElement` members. The bootstrap now attaches a retained
`GFx::ActionControl` with ActionScript errors unsuppressed before creating a
movie, so the next capture can identify an early `DoAction` failure instead of
silently falling back to the diagnostic scene. Root initialization also uses
the original `BaseSlot::Init` zero-delta `Advance(0)` after console globals and
`GameInterface` are installed.

The view scale mode is now `SM_NoScale`, matching Source's console slot. This
leaves authored 1280x720 coordinates intact so the ActionScript
`ResizeManager` owns the 1920x1080 safe-zone calculation. The new
`KisakPs4ScaleformUiRequestElement` entry point creates a fresh callback object
for every requested element before invoking `_global.RequestElement`, instead
of reusing the root `GameInterface`; this preserves per-panel callback state as
the menu starts requesting child movies. The solid dark-red spinning cube and
clipped transparent triangle remain the fallback regression image.

Marker: `kisak-ps4: build marker scaleform_hook_diagnostics_v347`.

The v3.47 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`517428e61ce043a307074ffdcc28825e0f46a812f1cbe0a33868b1982c6976b1`.
Host tests pass 11/11 and the PS4 link/package build completes. Hardware
validation is pending the next launch; inspect `scaleform log type=` lines
after the v347 marker, then look for the first element-load callback or a
bounded ActionScript error.

### v3.48: Defer the first root advance until console globals are installed

The v3.47 hardware run proved the diagnostic control is attached, but both
root movies still exposed no hooks and emitted no ActionScript error. With
`LoadWaitFrame1`, frame one is already resident when `CreateInstance` is
called; `CreateInstance(true)` can therefore execute the root `DoAction` before
`PlatformCode` and `GameInterface` exist. The manager now creates the instance
with `initFirstFrame=false`, installs the console globals and callback object,
then performs the original single `Advance(0)` bootstrap. This preserves the
Source ordering while avoiding an early script abort. The stable dark-red cube
and clipped transparent triangle remain the fallback image.

Marker: `kisak-ps4: build marker scaleform_deferred_first_advance_v348`.

The v3.48 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`6b18ec26c2b01fc642261ee33a8cd713e06544c54f358964b556e4c404270b17`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.45: Wait only for root frame one with a stable fallback

The v3.44 hardware run is the first clean root-movie parse: the file opener
selected `/app0`, both SWFs have the compressed flag cleared, and both report
one 1280x720 frame. The definitions still expose no ActionScript hooks because
the default `LoadAll` value returns before frame one is ready. Root creation now
uses `LoadWaitFrame1`, which is narrower than the previously crashing
`LoadWaitCompletion` path and is sufficient for the single-frame root scripts.

If GFx rejects the wait-frame-one load, the manager logs one breadcrumb and
automatically retries the proven asynchronous path instead of losing the
diagnostic frame. The next gate is `InitSlot`, `ForceResize`, and
`RequestElement` becoming available, followed by the first element-load or
drawable-tree marker. Marker:
`kisak-ps4: build marker scaleform_wait_frame1_v345`.

The v3.45 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`bf7c78d0cf01614cbcffcea4dc1470c157b90cb28f23bd8e0362f1b4fe9cd4a0`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.44: Prefer the validated app0 FWS movie closure

The v3.43 hardware run reached the runtime CWS path but direct bundled-zlib
`uncompress` failed before GFx, confirming an OpenOrbis zlib ABI/runtime problem
rather than missing loader state. The Scaleform file opener now tries the
validated `/app0/resource/flash` movie first for every `resource/flash/...`
request and falls back to the external GAME search path only when the packaged
asset is absent. This prevents uploaded compressed files from shadowing the
package-time FWS conversion while leaving the rest of Source's external content
layering unchanged.

The CWS fallback remains available and now logs the exact zlib result plus
input, declared, and produced sizes for later repair. Root loading remains
asynchronous. The next run must log `scaleform reading validated app0 movie`,
report an uncompressed SWF with one 1280x720 frame, and stop emitting premature
stream-end warnings for the roots. Marker:
`kisak-ps4: build marker scaleform_app0_fws_v344`.

The v3.44 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`53e17b8280695ef88b241089d3c284ba7d2e015b07fb80591d326b424d093bb7`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.42: Convert packaged compressed SWFs to uncompressed FWS

The v3.41 hardware run confirms that an explicitly retained `GFx::ZlibSupport`
state is present, but compressed roots still reach a premature stream-end tag
and expose zero frames/dimensions. Runtime zlib loading is therefore removed
from the UI critical path. A packaging helper now converts every user-supplied
`CWS` movie in the staged `resource/flash` closure to an equivalent `FWS`
movie in place, preserving its filename, SWF version, declared uncompressed
length, and body bytes. Existing `FWS` files are left unchanged and malformed
or unsupported inputs fail packaging.

Only the converter is committed; proprietary movie assets remain external and
are transformed solely inside the package staging directory. The next hardware
gate is SWF metadata with the compressed flag cleared, one 1280x720 frame, and
available root ActionScript hooks. Marker:
`kisak-ps4: build marker scaleform_fws_package_v342`.

The v3.42 package conversion validated and converted all 114 staged compressed
SWFs. `mainuirootmovie.swf` now begins with `FWS`, retains version 8 and declared
size 47,033 bytes, and contains the original decompressed body. The monolithic
package is staged at `/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with
SHA-256
`92688d4cc39dc78e2ceea5f87eec8e72da9af4325094c2f6b3a3e546e0bfb50a`.
Host tests pass 11/11 and the PS4 link/package build completes.

The umbrella graph index exceeded the worker's memory/file tolerance, so the
active port was indexed as three bounded codebase-memory projects:
`Kisak-Strike-PS4` (269 nodes/629 edges), OpenGNM (12,190 nodes/14,734 edges),
and `Scaleform-GFx` (28,428 nodes/108,677 edges). These graphs cover the code
currently being changed and the two implementation/reference backends used by
the Scaleform port.

### v3.43: Inflate external CWS movies at the Source filesystem boundary

The v3.42 hardware log still reported compressed SWF flags and zero metadata
even though the package contains validated FWS roots. Source's external GAME
search path is therefore shadowing `/app0/resource/flash` with the uploaded CWS
assets. `KisakScaleformFileOpener` now detects `CWS`, validates its declared
uncompressed size, inflates the body through the already-linked bundled zlib,
rewrites only the in-memory signature to `FWS`, and passes the owning memory
file to GFx. Inputs larger than 64 MiB, malformed headers, decompression errors,
and size mismatches fall back to the previous raw-file behavior with a bounded
failure breadcrumb.

This keeps both packaged and external content usable without modifying user
files on disk. Root creation remains asynchronous for the stability gate. The
next run must log `scaleform CWS inflated in file opener`, clear the compressed
metadata flag, and expose one 1280x720 frame. Marker:
`kisak-ps4: build marker scaleform_runtime_cws_v343`.

The v3.43 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`36dc6495a7801c16cc01702bf576bc53a872c9fc52dd4f55e808a1da818a96f3`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.41: Install the compressed-SWF zlib state explicitly

The v3.40 loader log shows that compressed SWF roots are not entering the zlib
stream path: each root reaches a premature stream-end tag and retains zero
frames/dimensions. The Scaleform zlib implementation and bundled zlib objects
are present in the PS4 libraries, but loader states are optional and are linked
only when explicitly constructed. The manager now creates a retained
`GFx::ZlibSupport`, installs it on the loader, and emits a one-shot state marker
before probing or creating any movie.

Root loading remains asynchronous to preserve the stable diagnostic scene. The
next gate is SWF `GetMovieInfo` reporting one 1280x720 frame without a premature
stream-end warning. Only after that passes will synchronous root creation be
retried. Marker:
`kisak-ps4: build marker scaleform_zlib_state_v341`.

The v3.41 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`c46e299c4c0085bb735972c3607719ff29e939e82d4e1c434e99e9ff0a0c4a48`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.40: Restore the stable roots and capture bounded GFx loader errors

The v3.39 hardware run proved that valid GFX header metadata does not guarantee
a loadable movie definition. Synchronous `CreateMovie` returned null for both
stripped roots, the manager reported both movies unavailable, and the process
crashed before its first frame. The live slots return to the v3.38 stable SWF
asynchronous path while retaining the SWF/GFX header probes.

A retained loader-local `GFx::Log` now captures at most 24 formatted Scaleform
messages into startup breadcrumbs. Failed-initialization and normal shutdown
release that logger before destroying `Scaleform::System`. This should expose
the exact parser, import, decompression, or bind error behind synchronous root
failure without perturbing the established diagnostic scene. Marker:
`kisak-ps4: build marker scaleform_loader_log_v340`.

The v3.40 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`2bedf42e7454fe943857fd77803022680aecbee4e3aedb4bb8ed93ebeca0132f`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.39: Load the validated stripped GFX roots synchronously

The v3.38 hardware probe separated the two asset formats. Both compressed SWF
roots returned `ok=1` but retained the compressed flag, zero frames, and zero
dimensions. Both stripped GFX roots returned the stripped flag, one frame, and
the expected 1280x720 dimensions. The live menu/HUD slots therefore switch to
`mainuirootmovie.gfx` and `gameuirootmovie.gfx`, and `LoadWaitCompletion` is
applied only to these validated stripped roots. The side-by-side probe remains
bounded so the log continues to expose the compressed-SWF limitation.

The next hardware gate is non-zero GFX metadata plus available `InitSlot` and
`RequestElement` hooks. Child movies requested by those hooks may still expose
the compressed-SWF decompression defect; that is deliberately isolated as the
following gate. The solid dark-red spinning cube and clipped transparent
triangle remain the fallback regression image. Marker:
`kisak-ps4: build marker scaleform_gfx_roots_v339`.

The v3.39 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`8be669ca28df6bf5743c58fff4a28c9e48ad77f47c6839ce2069127448c82751`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.37: Wait for root movie payloads before invoking ActionScript

The v3.36 hardware log identified a loader-state issue rather than an invalid
root file: both SWF roots reported `avm=1` and the expected URLs, but zero
frames and zero dimensions. GFx's `LoadAll` value is the default asynchronous
mode; the manager now ORs `LoadWaitCompletion` into the font and root movie
loads so frame metadata, timeline scripts, and imported element bindings are
ready before `CreateInstance`, `Advance`, and the Source slot hooks run. The
diagnostic scene remains the solid dark-red spinning cube and clipped
transparent triangle. Marker:
`kisak-ps4: build marker scaleform_wait_completion_v337`.

The next hardware run should show non-zero frame/dimension metadata and either
`InitSlot`/`RequestElement` availability or an explicit GFx load error.

The v3.37 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`507e4e446fa2364dc16b77e7fd79ab300623f6a3612661d485c77e9c66c4ca85`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.38: Recover from synchronous SWF failure and compare root formats

The v3.37 hardware run reached the new marker, initialized `fontlib.gfx`, then
reported both root movies unavailable and crashed before the first diagnostic
frame. This disproves the assumption that the zero metadata was only an
asynchronous timing artifact: forcing `LoadWaitCompletion` exposes a full-parse
failure for the compressed SWF roots. The manager restores the stable
asynchronous root/font behavior and hardens the all-roots-failed cleanup by
releasing the local font definition and retained callback handler before the
Scaleform system is destroyed.

Each slot now performs bounded, read-only `GetMovieInfo` probes for both its
`.swf` and `.gfx` root before creating the existing SWF instance. The probe
records success, version, flags, frame count, and dimensions. If GFX succeeds
while SWF fails, the next slice switches roots and validates its ActionScript;
if both fail, the next slice repairs the Scaleform file/decompression path.
The stable solid dark-red spinning cube and clipped transparent triangle remain
the expected image. Marker:
`kisak-ps4: build marker scaleform_root_info_probe_v338`.

The v3.38 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`76ddec3b1f3841d8dcbc76adea8cabb11d1a2b04076e037d188c5a38ce5b2ac7`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.33: Use Source's SWF root movies

The v3.32 capture proved that the `.gfx` root files load but expose no
ActionScript slot hooks: `InitSlot`, `ForceResize`, and `RequestElement` were
all absent and both trees remained four empty containers. Source's own
`g_szDefaultScaleformMovieName` and `g_szDefaultScaleformClientMovieName`
identify the `.swf` roots, which are packaged alongside the console `.gfx`
assets. The manager now loads `mainuirootmovie.swf` and `gameuirootmovie.swf`
while retaining the Source root-slot/`MainMenu` request sequence. The build
marker is `kisak-ps4: build marker scaleform_swf_roots_v333`.

The next hardware gate is non-zero drawable tree statistics or a precise SWF
loader failure; the diagnostic cube and clipped transparent triangle remain
the regression image.

The v3.33 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`f5e5aad816f21abc23babb3752fa6ca21e6e81aad34a5bdc3a7fb2063ced57fc`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.

### v3.34: Use the decompiled ActionScript contract as the GFx gate

The supplied analysis at `/Volumes/Untitled/Counter Strike Global Offensive`
confirms the runtime contract without adding proprietary sources to the
repository. `gfx_decompiled` contains converted SWF structure and
`gfx_scripts` contains 3,519 extracted ActionScript files; `gfx_export` is a
5.3 GiB visual export with 53,814 files and is not a package input. The root
scripts define `_global.InitSlot`, `_global.ForceResize`, and
`_global.RequestElement`. `RequestElement("MainMenu", gameAPI)` starts
asynchronous `Background.swf` and `MainMenu.swf` loads through
`MovieClipLoader`; therefore the capture gate must run after subsequent
`Advance` calls, not only immediately after the request.

The layout analysis also identifies the required console behavior: inject
`PlatformCode=2`, preserve `wantControllerShown`, honor the 0.85 safezone, and
retain the complete root `.gfx`/`.swf` closure plus font libraries. Before real
OpenGNM primitive emission, add bounded no-op/API callbacks for the element
load lifecycle (`OnLoadFinished`, `OnLoadProgress`, `OnLoadError`, `OnUnload`)
and verify that a later capture contains shapes, meshes, or text. The build
marker for this investigation remains
`kisak-ps4: build marker scaleform_swf_roots_v333`.

### v3.35: Bridge asynchronous Scaleform element callbacks

The PS4 GFx manager now installs a retained `GameInterface` function table on
both root movies. It covers the element lifecycle (`OnLoadFinished`,
`OnLoadProgress`, `OnLoadError`, `OnUnload`), Source root helpers, and the small
MainMenu API surface with deterministic no-op/default behavior. The first
successful child load and first UI event emit bounded breadcrumbs. The HAL also
logs a separate drawable-tree marker when a later capture contains shape, mesh,
or text nodes, so asynchronous `MovieClipLoader` completion is not hidden by
the first four-container snapshot. The diagnostic cube and clipped transparent
triangle remain unchanged. Marker:
`kisak-ps4: build marker scaleform_async_callbacks_v335`.

The v3.35 package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`097a0e334c9f083438c9b5472455da4cc47d733844da0844926fe17cf7dbea69`.
Host tests pass 11/11 and the PS4 link/package build completes. The next
hardware run must show `scaleform element load finished` followed by a
`scaleform drawable tree` breadcrumb, or an explicit element-load error.

### v3.36: Record GFx movie metadata before changing the stable scene

The next Scaleform gate adds one bounded breadcrumb per menu/HUD root with its
resolved URL, SWF version, frame count, dimensions, AVM version, and current
frame. A second breadcrumb records whether `InitSlot` and `RequestElement` are
available through the movie's ActionScript namespace immediately after the
initial zero-time advance. This separates a wrong/unsupported asset or AVM
format from a global-object lookup problem without enabling primitive emission
or perturbing the validated solid dark-red spinning cube and clipped
transparent triangle. The marker is
`kisak-ps4: build marker scaleform_movie_metadata_v336`.

The next hardware run must include the two metadata/script lines for each root;
they determine whether to keep the SWF roots, fall back to the console GFX
files, or repair AS2 execution before the asynchronous element callback gate.

The v3.36 monolithic package is staged at
`/data/pkg/IV0000-KISK00002_00-KISAKMONOLITHIC0.pkg` with SHA-256
`1a93a1e279fc82e9d7b0d60a9f50fa18aebc922c862330c1c6b1ba94635d41ec`.
Host tests pass 11/11 and the PS4 link/package build completes; hardware
validation is pending the next launch.
