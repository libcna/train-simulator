# Colour-pipeline calibration

The default production renderer still uses CNA BasicEffect. Do not apply a blanket gamma correction to
the finished frame: the sky, HUD, avatars, fog, transparent surfaces and authored textures do not
necessarily share the same input encoding or lighting assumptions.

An explicit project-owned diagnostic compares three rows of opaque colour patches:

1. CNA BasicEffect with vertex colour and ambient-only lighting.
2. A custom OpenGL 3.3 shader multiplying the same colour by the same illumination directly.
3. The same shader decoding the colour from sRGB, applying illumination in linear light, and
   encoding the result back to sRGB.

Columns contain neutral grey, a masonry palette colour and a vegetation palette colour, each
at illumination factors 0.25, 0.5 and 1. The row positions are fixed; the diagnostic image has no
labels or UI. The tool checks all 81 RGB channel results against independent CPU calculations
with a two-byte tolerance, then saves the image. A mismatch or shader failure returns nonzero.

```sh
cmake --build build --target trainsim_colour_probe --parallel 4
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LP_NUM_THREADS=4 \
  ./build/trainsim_colour_probe build/colour-pipeline.png
```

For private real-GPU verification using the read-only sibling CNA runner:

```sh
env LP_NUM_THREADS=4 ../cna/tools/platform/run_gpu_tests_private.sh --exec \
  env SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
  ./build/trainsim_colour_probe build/colour-pipeline-gpu.png
```

The target is excluded from normal builds and installation. It uses the existing build tree,
mandatory ccache, the simulator's mesh vertex layout and public CNA APIs. GLSL 330 source lives
in OpenGL33-specific render code; simulation and default production materials are unaffected.

## Verified result

At CNA `97783d1e0` and sharp-runtime `21c06910`, both offscreen Mesa and private GPU-backed
Weston/Xwayland passed all 27 patches. Neutral input 128 at factor 0.5 produced 64 in both the
stock and direct-multiply rows, and 92 in the linear-light row. Full-illumination patches agree.
Logs: `build/colour-pipeline.log` and `build/colour-pipeline-gpu.log`.

This validates the difference between these workflows for opaque untextured patches. It is not
a proof that switching the entire game will look correct, nor a dependency defect: BasicEffect
retains its legacy XNA-style arithmetic. The next comparison should isolate an exterior material
with normal-based lighting, fog and a representative texture, retaining a reversible baseline.
Do not compensate simultaneously by retuning every palette and light intensity, which would
make the comparison inconclusive. Alpha blending, emissive displays and cabin materials need
separate checks before any production migration.

## Textured directional-light and fog comparison

```sh
env SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LP_NUM_THREADS=4 \
  ./build/trainsim_colour_probe build/colour-materials.png --materials
```

The rows retain the stock/control/linear meanings. Each three-column group uses upward,
sun-facing wall and sun-away wall normals with the simulator's sunny exterior lighting. The
groups use 0, 50 and 100 metres of view depth with a 0–100 m fog interval. All patches use the
actual generated masonry detail texture and one authored masonry tint. Point filtering and no
MSAA deliberately expose sample-coordinate mismatches; this is not a production filter preset.

The control shader matches CNA's non-MSAA OpenGL33 XNA pixel-centre displacement (63/128 pixel).
Without it, point-sampled mortar/brick boundaries differ even though the lighting math agrees.
This is a calibration alignment, not a colour correction or a dependency fix. The custom shader
declares only the simulator mesh's public vertex layout and uses ShaderEffect texture binding.
`--materials-uv` substitutes a coordinate ramp to isolate texture-coordinate discrepancies.

The material test checks stock/control RGB agreement within two bytes, identical fully fogged
endpoints, and brighter shaded results under the selected sub-unit illuminances. Unlike the
untextured test, it is not an independent CPU reference for every linear-material output pixel.
It treats the authored tint-times-detail product as encoded surface colour and blends fog in
linear light before encoding. Alternative texture encoding and mip generation policies still
need evaluation before migrating the game's materials.

At the same dependency revisions, all 27 material patches passed on offscreen and private GPU
paths. One wall sample changed from stock/control RGB `(96,88,79)` to `(123,109,94)` in the linear
row. Full-fog colour remains the same. Logs/images use `build/colour-materials{,-gpu}.*`.
The original untextured mode and the UV control also passed after the extension.

One first-use discrepancy is kept separate: `--materials-cold` omits the first stock-quad repeat
and deliberately reproduces a failing first textured draw on both tested paths. The ordinary
material mode repeats only that quad before measurement; see `bugs.md` for exact symptoms and
the unconfirmed dependency attribution. Failures retain their image and return nonzero.

## Opt-in in-game facade integration

`--facade-control` replaces only the masonry and rendered-plaster facade batches with a
project-owned GLSL 330 effect using direct colour multiplication. `--linear-facades` uses the
same effect with linear-light illumination and fog. Neither flag changes the default renderer,
palette, lighting strengths, interiors, avatar effects, UI or transparent surfaces. These are
diagnostic launch options, not yet a recommended graphics preset. The last supplied flag wins.

The effect copies the BasicEffect material/light parameters and uses public ShaderEffect APIs.
In addition to pixel-centre alignment, custom GLSL must convert XNA matrix clip depth from
`[0,w]` to OpenGL `[-w,w]`. CNA stock shaders perform that conversion internally; omitting it
in this project shader caused opaque building bodies to cover the thin facade overlays. This
was a simulator integration error, not a dependency bug. No depth bias or disabled culling is
used to hide it. The CPU-combined world/view/projection matrix matches the stock transform.

Reproduce the fixed street view and image regression at Medium or High:

```sh
python3 tools/test_facade_lighting.py build/train-simulator build/facade-comparison --quality 1
ctest --test-dir build --output-on-failure -R trainsim_facade_lighting
```

The script captures stock/control/linear images and execution logs. It verifies both facade
batches ran, stock/control near-agreement, and a substantial visible linear-light difference.
The test fixture disables camera vibration and reduces route time scale to one millionth;
otherwise variable startup/update timing shifts the scene between separate process captures.
This changes only the comparison fixture, not player comfort defaults.
The thresholds deliberately catch lost depth overlays, not just successful shader compilation.
The control is not certified pixel-identical; remaining differences include sampling/edge
behaviour. Both it and the Medium/High stock material use per-fragment lighting.
Before end-wall coverage was added, fixed-view RGB mean differences were 0.093 (Medium) and
0.129 (High) byte levels; about 10% of the image changed by over 20 levels in linear mode.

The visible shaded masonry is lighter with more legible detail. That does not alone establish
photorealism: roof materials, vegetation and sparse urban
layout still expose inconsistent art direction. Before enabling this by default, evaluate
multiple headings, sun-facing surfaces, distance/fog, tunnel transitions and material seams.

The stabilized High comparison also passed on private GPU-backed Xwayland with CNA OPENGL33:
control RGB mean difference 0.093, major-difference pixel fraction 0.000149, and linear-light
changed fraction 0.100309 (`build/facade-gpu-comparison.log`). This is a three-frame image check,
not a long gameplay or physical-input test. The first GPU attempt with camera sway enabled
failed comparison and prompted the fixed-camera fixture; thresholds were not relaxed.

After extending the material to end/rear elevations, the geometry uses complete four-sided
material shells instead of overlays backed by duplicate opaque wall boxes. The initial end-wall
overlay version failed the High comparison (mean 0.298); replacing duplicate surfaces restored
agreement without relaxed tolerances. Current shell results: control mean 0.130/0.191 at
Medium/High offscreen, 0.142 at High on the private GPU; linear differences cover about 12.6%
of the street view. Evidence: `build/facade-ends-tests.log`, `build/facade-ends-gpu.log`.
