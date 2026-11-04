# Experiments

Throwaway programs for trying an idea before it goes near the renderer. They
live outside `src/` and `tests/`, so the style gate, the pre-commit hook and
CI never see them, and they are their own CMake project, so the renderer's
build never sees them either.

## Build and run

Use the same compiler the renderer was built with (the one in
`build/CMakeCache.txt`), so a number here means the same thing there.

```bash
cmake -S experiments -B build/experiments -G "MinGW Makefiles" \
      -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/c++.exe
cmake --build build/experiments
build/experiments/stratified_pi
```

Configure once. After that a new `.cc` here is picked up by the next
`cmake --build build/experiments`.

## Rules

- One `.cc` per experiment. The file name is the executable name.
- Include renderer headers the way the renderer does: `"render/sampling.h"`.
  Only header-only code links; nothing from `src/*.cc` is compiled in.
- Print to stdout, or write files by an explicit path under
  `build/experiments/`. `build/` is ignored, so nothing written there can
  reach git by accident. A bare filename lands in the directory you ran from.
- Nothing here is part of the renderer. To keep an idea, write it again in
  `src/` in the project's style; do not move the experiment.
