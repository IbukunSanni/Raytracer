#!/usr/bin/env bash
#
# Style gate for src/ and tests/. Two checks, and they answer different
# questions:
#
#   formatting  -- clang-format, from .clang-format. Whitespace, braces,
#                  line breaks, include order, LF endings.
#   naming      -- clang-tidy, from .clang-tidy. Needs to see types, so it
#                  needs a compiler that can parse the tree.
#
# Exit 0 clean, 1 if anything is off. Pass --fix to rewrite instead of
# report (formatting only; naming changes are never applied unattended).
#
# Run it directly, as `cmake --build build --target style`, or let the
# pre-commit hook in .githooks/ run it on staged files.
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1
ROOT=$(pwd)
FIX=0
[ "${1:-}" = "--fix" ] && FIX=1

# Third-party code keeps upstream's formatting, so a version bump diffs
# against upstream rather than against our rules.
mapfile -t FILES < <(find src tests -name '*.cc' -o -name '*.h' | sort)
if [ ${#FILES[@]} -eq 0 ]; then
  echo "no sources found -- run from the repo root" >&2
  exit 1
fi

find_tool() {
  local name=$1
  if command -v "$name" >/dev/null 2>&1; then command -v "$name"; return 0; fi
  # Visual Studio ships both, but not on PATH.
  local vs="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin/$name.exe"
  [ -x "$vs" ] && { echo "$vs"; return 0; }
  return 1
}

status=0

# ---------------------------------------------------------------- format
CF=$(find_tool clang-format) || {
  echo "FAIL  clang-format not found. Install LLVM, or use the copy in"
  echo "      Visual Studio at VC/Tools/Llvm/bin." >&2
  exit 1
}

if [ "$FIX" = 1 ]; then
  "$CF" -i "${FILES[@]}"
  echo "ok    formatting rewritten in ${#FILES[@]} files"
else
  bad=()
  for f in "${FILES[@]}"; do
    "$CF" --dry-run --Werror "$f" >/dev/null 2>&1 || bad+=("$f")
  done
  if [ ${#bad[@]} -gt 0 ]; then
    echo "FAIL  ${#bad[@]} file(s) are not clang-format clean:"
    printf '        %s\n' "${bad[@]}"
    echo "      fix with: scripts/check_style.sh --fix"
    status=1
  else
    echo "ok    formatting: ${#FILES[@]} files match .clang-format"
  fi
fi

# ---------------------------------------------------------------- naming
# clang-tidy has to compile each file, so it needs the standard headers.
# CI gives it a compile_commands.json; on a MinGW box it needs to be aimed
# at GCC's headers by hand, because the Visual Studio clang-tidy ships
# without clang's own. RT_TIDY_FLAGS overrides the guess.
CT=$(find_tool clang-tidy) || CT=""
if [ -z "$CT" ]; then
  echo "skip  naming: clang-tidy not found (formatting was still checked)"
  exit $status
fi

# uname -s on git bash reports a versioned name (MINGW64_NT-10.0-26200), so
# match a prefix rather than comparing for equality.
case "$(uname -s)" in
  MINGW* | MSYS* | CYGWIN*) ON_WINDOWS=1 ;;
  *) ON_WINDOWS=0 ;;
esac

if [ -n "${RT_TIDY_FLAGS:-}" ]; then
  read -r -a FLAGS <<< "$RT_TIDY_FLAGS"
elif [ -f build/compile_commands.json ] && [ "${RT_TIDY_USE_DB:-1}" = 1 ] \
     && [ "$ON_WINDOWS" = 0 ]; then
  # A database written by a MinGW build points at GCC and uses @response
  # files, neither of which clang-tidy can follow -- hence the guard above.
  FLAGS=()
else
  GCC_ROOT=${RT_GCC_ROOT:-/c/msys64/mingw64}
  GCC_VER=$(ls "$GCC_ROOT/include/c++" 2>/dev/null | head -1)
  if [ -z "$GCC_VER" ]; then
    echo "skip  naming: no compile_commands.json and no GCC headers found."
    echo "      Set RT_TIDY_FLAGS to the compiler flags for this tree."
    exit $status
  fi
  W=$GCC_ROOT/include/c++/$GCC_VER
  FLAGS=(-std=c++17 -target x86_64-w64-windows-gnu -DNDEBUG
         # glm's SIMD path includes GCC intrinsics headers that clang cannot
         # parse. PURE picks the scalar path; it changes no identifier, and
         # never reaches the real build.
         -DGLM_FORCE_PURE
         -I"$ROOT/src" -I"$ROOT/third_party" -I"$ROOT/tests"
         -DRAYTRACER_EXE='"raytracer.exe"'
         -isystem "$GCC_ROOT/lib/gcc/x86_64-w64-mingw32/$GCC_VER/include"
         -isystem "$W" -isystem "$W/x86_64-w64-mingw32"
         -isystem "$W/backward" -isystem "$GCC_ROOT/include")
fi

# One TU per .cc. tests/support/main.cc is skipped: it is two lines handing
# main() to doctest, whose header drags in those same intrinsics, and it
# declares nothing of ours.
mapfile -t UNITS < <(find src tests -name '*.cc' ! -path 'tests/support/main.cc' | sort)
crashed=0
report=$(mktemp)
for u in "${UNITS[@]}"; do
  if [ ${#FLAGS[@]} -eq 0 ]; then
    out=$("$CT" --quiet -p build "$u" 2>&1)
  else
    out=$("$CT" --quiet "$u" -- "${FLAGS[@]}" 2>&1)
  fi
  if grep -q "Stack dump\|PLEASE submit a bug report" <<< "$out"; then
    crashed=$((crashed + 1))
    continue
  fi
  if grep -q "clang-diagnostic-error" <<< "$out"; then
    echo "FAIL  naming: $u did not parse"
    grep "clang-diagnostic-error" <<< "$out" | head -3 | sed 's/^/        /'
    status=1
    continue
  fi
  # A header is re-parsed by every TU that includes it, so the same
  # diagnostic arrives many times. Normalise the path separators and
  # dedupe, or one bad name in a common header reads as thirty failures.
  grep -E "warning:|error:" <<< "$out" \
    | sed 's/\\/\//g; s|.*/src/|src/|; s|.*/tests/|tests/|' >> "$report"
done

sort -u -o "$report" "$report"
violations=$(wc -l < "$report")

if [ "$crashed" -gt 0 ]; then
  echo "warn  naming: clang-tidy crashed on $crashed file(s); those are unchecked"
fi
if [ "$violations" -gt 0 ]; then
  echo "FAIL  naming: $violations distinct violation(s) of .clang-tidy"
  sed 's/^/        /' "$report"
  status=1
else
  echo "ok    naming: ${#UNITS[@]} translation units match .clang-tidy"
fi
rm -f "$report"

exit $status
