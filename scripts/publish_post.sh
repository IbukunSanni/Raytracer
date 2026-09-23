#!/usr/bin/env bash
# Turn a draft in posts/ into a dev.to-ready copy under build/posts/.
#
# Drafts are written in portable LaTeX -- $$ ... $$ for display math, $ ... $
# inline -- because that is what VS Code's preview and GitHub both render, so a
# draft can be checked without uploading it anywhere. dev.to renders neither;
# it wants Forem's liquid tags. This script is the one-way trip between them.
#
#   scripts/publish_post.sh posts/malleys-method.md
#
# The output is disposable and lives under build/, which is gitignored. The
# draft stays the source of truth -- never edit the generated copy.
set -euo pipefail

SRC="${1:?usage: publish_post.sh posts/<name>.md}"
[ -f "$SRC" ] || { echo "no such file: $SRC" >&2; exit 1; }

OUT_DIR="build/posts"
OUT="$OUT_DIR/$(basename "${SRC%.md}").devto.md"
mkdir -p "$OUT_DIR"

# Math outside fenced code only: a shell snippet using $VAR must survive, and
# in this repo that is not hypothetical -- the logging posts quote RT_LOG=.
awk '
  BEGIN { fence = 0; math = 0; front = 0 }

  # Front matter passes through untouched; dev.to parses it before Markdown.
  NR == 1 && /^---[[:space:]]*$/ { front = 1; print; next }
  front == 1 && /^---[[:space:]]*$/ { front = 0; print; next }
  front == 1 { print; next }

  /^```/ { fence = !fence; print; next }
  fence  { print; next }

  /^\$\$[[:space:]]*$/ {
    if (math == 0) { print "{% katex %}"; math = 1 }
    else           { print "{% endkatex %}"; math = 0 }
    next
  }
  math { print; next }

  {
    line = $0; out = ""
    while (match(line, /\$[^$]+\$/)) {
      out = out substr(line, 1, RSTART - 1) \
            "{% katex inline %}" substr(line, RSTART + 1, RLENGTH - 2) "{% endkatex %}"
      line = substr(line, RSTART + RLENGTH)
    }
    print out line
  }

  END { if (math) { print "ERROR: unclosed $$ block" > "/dev/stderr"; exit 1 } }
' "$SRC" > "$OUT"

echo "wrote $OUT"

# Pre-upload checklist. These are not failures -- they are the things that are
# easy to forget between the draft and the dev.to editor.
warn() { printf '  %s\n' "$1"; }
issues=0
note() { if [ "$issues" -eq 0 ]; then echo "before uploading:"; issues=1; fi; warn "$1"; }

if grep -q '^cover_image:[[:space:]]*$' "$SRC"; then
  note "cover_image: is empty -- dev.to needs a URL, not a local path"
fi
n=$(grep -c '<!-- IMAGE' "$SRC" || true)
[ "$n" -gt 0 ] && note "$n unfilled <!-- IMAGE --> slot(s)"
if grep -q 'docs/images/' "$SRC"; then
  note "links to docs/images/ -- dev.to cannot read repo paths, upload first"
fi
if grep -q '^published:[[:space:]]*true' "$SRC"; then
  note "published: true -- this uploads live, not as a draft"
fi
exit 0
