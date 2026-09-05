# Code walkthrough — how one pixel gets its colour

Follow a single ray from `main()` to a byte in a PNG. Every file:line reference
is real; open them alongside this.

---

## The whole flow in one picture

```
main.cpp
   └─ run_lua(scene.lua)                       Lua interprets the scene file
        └─ gr.render(...)  →  gr_render_cmd    the only Lua call that renders
             ├─ Image im(w, h)                 the output buffer
             └─ Render(root, im, eye, ...)     src/render/Renderer.cpp
                  ├─ build camera basis (u, v, w)
                  ├─ Framebuffer accum(w, h)   running sum, not an image yet
                  │
                  └─ for each chunk of passes:
                       spawn N threads, each running renderBand()
                          └─ for each pass, each pixel in this thread's rows:
                               ├─ build a jittered ray through the pixel
                               ├─ rayTraceRGB()  ────┐  returns radiance
                               └─ accum.add(x, y, radiance)
                       join
                       accum.addSamples(chunk)
                       (optional snapshot: resolve + savePng)
                                                     │
   rayTraceRGB()  ───────────────────────────────────┘
        ├─ root->isHit(ray)        walk the scene graph, find nearest hit
        ├─ shade: ambient + per-light diffuse & specular
        ├─ shadow ray per light
        └─ recurse for the reflection ray

   accum.resolve(image)      divide sums by sample count (stays linear)
   im.savePng(filename, cfg) tone map -> sRGB encode -> ×255 -> bytes
```

Two things worth internalising before the detail:

- **Rays are traced from the eye, not from the lights.** Light transport runs
  backwards. `rayTraceRGB` asks "what do I see along this direction", and only
  when it finds a surface does it ask "which lights can reach here".
- **Nothing is an image until the very end.** During rendering there is only a
  sum of radiance per pixel plus a count. `Image` appears twice: once as the
  buffer `resolve()` writes into, once as the thing that encodes a PNG.

---

## Stage 1 — startup: Lua builds the scene

**`src/main.cpp:7`** picks a scene file (default `assets/scenes/simple.lua`) and
hands it to `run_lua`.

From there **the Lua interpreter is in charge**. It executes the scene file top
to bottom. Every `gr.*` call in that file is a C++ function registered in the
table at **`src/lua/scene_lua.cpp:577`**:

| Lua | C++ | effect |
|---|---|---|
| `gr.node('x')` | `gr_node_cmd` | `new SceneNode` |
| `gr.nh_sphere(...)` | `gr_nh_sphere_cmd` | `new GeometryNode` wrapping a `NonhierSphere` |
| `gr.mesh(...)` | `gr_mesh_cmd` | parses the OBJ, builds a `Mesh` |
| `gr.material(...)` | `gr_material_cmd` | `new PhongMaterial` |
| `gr.light(...)` | `gr_light_cmd` | `new Light` |
| `gr.set_samples(n)` | `gr_set_samples_cmd` | sets `g_samplesPerPixel` |
| `gr.set_tonemap{...}` | `gr_set_tonemap_cmd` | sets the write-out `tonemap::Config` |
| `gr.render(...)` | Lua shim → `gr_render_cmd` | **runs the renderer** |

Between registering that table and loading the scene, `run_lua` executes a
small Lua **prelude** (`GR_PRELUDE`, embedded in `scene_lua.cpp`). It renames
the raw ten-argument C binding to `gr._render` and defines `gr.render` in Lua
so scenes can pass a named table. The prelude validates field names, so a
typo is an error at the scene line rather than a silent default. The
positional form still forwards straight through.

So a scene file is not data being parsed — it is a program that builds a C++
object graph by calling constructors, and then calls the renderer once.

By the time `gr.render` runs, the scene graph already exists in memory.

## Stage 2 — `gr_render_cmd` unpacks the arguments

**`src/lua/scene_lua.cpp:314-352`**. It pulls the root node, filename,
resolution, eye/view/up, fov, ambient and the light list off the Lua stack,
then:

```cpp
Image im(width, height);              // allocate the output
SetOutputPath(filename);              // so snapshots can be named beside it
Render(root->node, im, ...);          // <- everything below happens here
im.savePng(filename, GetToneMap());   // tone map + encode
```

Note `savePng` is called **here**, not inside the renderer. The renderer fills
`im`; the Lua binding writes it, reading the tone-map config back from the
renderer. Snapshots are the exception — written inside `Render`, which is why it
needs `SetOutputPath` and holds the config.

## Stage 3 — the camera basis

**`src/render/Renderer.cpp:332-338`**. This is the part most people find
opaque, so slowly:

```cpp
vec3 wVec = normalize(view);            // forward
vec3 uVec = normalize(cross(up, view)); // right
vec3 vVec = cross(uVec, wVec);          // true up
float dFloat = h/2 / tan(radians(fovy/2));
const vec3 initDirVec = wVec*dFloat - uVec*(w/2) - vVec*(h/2);
```

Imagine the image plane floating in front of the eye, `dFloat` units away.
`dFloat` is chosen so that a plane `h` pixels tall subtends exactly `fovy`
degrees — that's the whole trigonometry: `tan(fovy/2) = (h/2) / d`.

`initDirVec` is the direction from the eye to **one corner** of that plane.
Then in `renderBand` (**:240**):

```cpp
centreDirVec = initDirVec + (w - x)*uVec + y*vVec;
```

Add `x` steps right and `y` steps up and you have the direction to any pixel.
One pixel is exactly one unit of `uVec` or `vVec` — which is why jittering by
±0.5 of those vectors stays inside the pixel's footprint.

**This direction is not normalised, on purpose.** `t` in every intersection
test is measured in units of this vector's length. Normalising here would
silently change what `t` means everywhere downstream.

## Stage 4 — passes, threads, bands

**`src/render/Renderer.cpp:364-435`**.

```
Framebuffer accum(w, h);      running sum + sample count
while (done < totalSamples):
    chunk = how many passes before the next snapshot
    split rows into N bands, one thread each, each running `chunk` passes
    join
    accum.addSamples(chunk)
    optionally resolve + write a snapshot
resolve into `image`
```

Three deliberate choices:

- **Passes are the outer loop.** After pass 4, *every* pixel has exactly 4
  samples, so the buffer is a coherent (noisy) image. If pixels were finished
  one at a time instead, a half-done render would be half-final, half-black,
  and "the image at 4 samples" would not exist.
- **Bands own disjoint rows**, so `accum.add()` needs no locking. Two threads
  never touch the same pixel.
- **Threads are respawned per chunk**, not per pass. With snapshots off that's
  a single spawn.

## Stage 5 — one sample

**`renderBand`, `src/render/Renderer.cpp:210-275`**. For one pixel:

1. Compute `centreDirVec` (stage 3).
2. Jitter: `+ (rng.next()-0.5)*uVec + (rng.next()-0.5)*vVec`. A pixel is a
   *square*, not a point; its true value is the average over that square, and a
   jittered sample is an unbiased estimate of that average. Always sampling the
   centre is exactly what makes edges alias.
3. Build the ray — origin at the eye, or on the aperture disk if the thin lens
   is enabled.
4. `rayTraceRGB(...)` → radiance.
5. `accum.add(x, y, radiance)`.

## Stage 6 — finding what the ray hits

`rayTraceRGB` (**`src/render/Renderer.cpp:83`**) starts with `root->isHit(ray, EPS, MAX_T, record)`.

**This is where the coordinate systems live, and it is the subtlest part of the
codebase.**

Objects are not transformed into world space. Instead **the ray is transformed
into each object's local space**:

- `SceneNode::toLocal` (**`src/scene/SceneNode.cpp:146`**) multiplies the ray's
  origin and direction by this node's inverse transform.
- `hitChildren` (**:160**) passes that local ray to each child, which applies
  *its own* inverse in turn. So descending the graph composes inverses.
- `toWorld` (**:154**) converts the hit back on the way out.

Why this direction? Because a unit sphere test is trivial and a
transformed-ellipsoid test is not. Move the ray instead of the object and every
primitive only ever has to intersect its own canonical shape.

Two details in `toLocal`/`toWorld`:

- The origin is transformed as a **point** (`vec4(o, 1)`), the direction as a
  **vector** (`vec4(d, 0)`) — the `0` is what stops translation being applied
  to a direction.
- The normal comes back through the **inverse transpose**, not the transform.
  Under non-uniform scale a surface normal does not transform like a direction;
  squash a sphere and the naive normal stops being perpendicular to the surface.

`t0`/`t1` bound the search along the ray. As closer hits are found, `t1` is
tightened so anything further away is rejected immediately — that's how "the
nearest hit wins" is implemented, and it is also the mechanism a BVH will lean
on heavily.

The leaf of all this is `Primitive::isHit` — `NonhierSphere` solves a quadratic
(`src/math/polyroots.cpp`), `Mesh` currently tests **every triangle** (that's
what the BVH will fix).

## Stage 7 — shading

Back in `rayTraceRGB`, on a hit:

1. **Offset the hit point** by `normal * EPS`. Without this, the shadow ray
   starts exactly on the surface and immediately re-hits it through
   floating-point error — the classic "shadow acne" black speckle.
2. **Ambient**: `diffuse * ambient`.
3. **Per light**: cast a shadow ray toward it. Any hit → skip the light. Else
   add Lambert diffuse (`max(0, N·L)`) plus Blinn specular (`max(0, N·H)^shininess`,
   where `H` is the half-vector between view and light).
4. **Reflection**: mirror the direction about the normal, recurse with
   `reflectionHits - 1`, and `mix` the result in at `REFLECTION_COEFF`.

On a **miss**, the background texture is sampled (`:155-190`) and `decodeSRGB`'d
into linear radiance, the same space as everything else.

> One known problem here, scheduled: the shadow ray passes `MAX_T` as its far
> bound, so geometry *behind* a light still shadows it. Step 10 work.

## Stage 8 — sum becomes image becomes PNG

- `Framebuffer::add` (**`src/render/Framebuffer.cpp:11`**) accumulates into a
  `dvec3`. **Double, not float**: once the running sum is large, a float
  accumulator rounds away the low bits of each new sample and the image quietly
  stops converging.
- `Framebuffer::resolve` (**:21**) divides by the sample count into an `Image`.
  It is `const`, and stays **linear** — `mean(f(x)) != f(mean(x))`, so averaging
  a tone curve converges on the wrong image.
- `Image::savePng` (**`src/core/Image.cpp`**) is the only place bytes are made:
  `tonemap::apply` (which also clamps), then `tonemap::encodeSRGB` unless
  `srgb = false`, then `×255 + 0.5`. The config comes from `gr.set_tonemap` via
  the renderer; snapshots use the same one.

`core/ToneMap.hpp` covers why the two stages stay separate. That was staircase
step 2.

---

## If you remember five things

1. **The ray moves into the object's space, not the object into the world.**
   That is what `toLocal`/`toWorld` are for, and why normals need the inverse
   transpose.
2. **Ray directions are deliberately unnormalised**, so `t` is in units of the
   direction vector.
3. **Passes are the outer loop** so that a partial render is a whole noisy
   image rather than a partly-finished one.
4. **Jitter is not optional decoration** — it is what makes each sample an
   unbiased estimate of the pixel's true average.
5. **`Image` is only an output format.** The renderer's real state is
   `Framebuffer`: a sum and a count.

## Where to put a breakpoint

Tracing one pixel by hand is the fastest way to make this concrete. Set
`gr.set_samples(1)`, render something tiny, and break in `rayTraceRGB` guarded
on a single pixel:

```cpp
if (x == 128 && y == 128) { /* breakpoint here */ }
```

Then step through `root->isHit` and watch the ray's origin and direction change
as it descends the scene graph. Once you have seen a ray get transformed into a
sphere's local space and the hit come back out, the rest of the codebase stops
being mysterious.
