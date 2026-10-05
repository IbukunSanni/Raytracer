# Raytracer

A multithreaded CPU path tracer in C++, with scenes described in Lua. It reads
a scene, traces it on every core and writes a PNG.

![A rendered scene: low-poly trees, spheres and a sun over a green plane](docs/images/sample.png)

## Features

- Path tracing with Russian roulette, and energy-conserving BSDFs (Lambertian,
  Blinn-Phong, mirror, metal, dielectric) verified by furnace tests
- Image-based lighting, point-light shadow rays, thin-lens depth of field
- Spheres, boxes and OBJ meshes in a hierarchical scene graph
- Progressive accumulation in linear colour, tone-mapped once at write-out
- In progress: a BVH (step 8), then textures (step 9)

## Build and run

C++17 and CMake 3.16+. Every dependency is vendored under `third_party/`.
On Windows with MSYS2, add `-G "MinGW Makefiles"` to the first command.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/raytracer assets/scenes/macho-cows.lua    # run from the repo root
ctest --test-dir build --output-on-failure        # tests
```

Output file, resolution and camera come from the scene's `gr.render` call.
To write a scene, copy [`assets/scenes/template.lua`](assets/scenes/template.lua).

| Environment variable | Effect |
|---|---|
| `RT_LOG=debug`, `RT_LOG=off,geom:debug` | Log verbosity, overall or per category |
| `BVH_TRAVERSAL=linear\|recursive\|iterative` | Which intersection path meshes use |
| `BVH_VERIFY=1` | Check the BVH against the linear scan on every ray |

## Docs

- [WALKTHROUGH](docs/WALKTHROUGH.md): one ray, from `main()` to a byte in a PNG
- [ROADMAP](docs/ROADMAP.md): status, the plan, and the measurements
- [ENGINEERING](docs/ENGINEERING.md): deadline, scope and blog writeups
- [REFERENCE](docs/REFERENCE.md): logging, scene format, layout, tests, style
  (Google C++, enforced), performance and compiler portability

## Provenance and licensing

Began as assignment 4 of Waterloo's CS488 and extended well beyond it; some
scaffolding (`polyroots.cc`, the Lua bindings, `Image`) is course-provided.
Vendored code keeps its licence: glm (MIT), lodepng (zlib), Lua (MIT). No
top-level licence has been chosen for the original work yet.
