# Posts

Publishable drafts, one file per post. `docs/ENGINEERING.md` holds the raw
notes these are written from; a file lands here once it is being drafted for
an actual publication rather than captured for later.

## Front matter

dev.to reads the YAML block at the top. `published: false` uploads it as a
draft, which is the safe default -- flip it in the dev.to editor once the
images are in. Tags are lowercase, no spaces, four maximum.

## Images

**dev.to will not read `docs/images/` paths.** Markdown image links have to
point at a URL that resolves publicly, so each image needs uploading to
dev.to's editor (or any host) first and the returned URL pasted in.

Drafts here mark the gaps with `<!-- IMAGE: ... -->` comments rather than broken
links, so a draft never renders a missing-image icon and each slot carries the
recipe for the image that belongs in it.

A comment is invisible in preview, so the preview is not what catches an
unfilled slot -- `publish_post.sh` is. It counts the `<!-- IMAGE` markers and
lists them before upload.

`cover_image:` in the front matter is the social/banner image and takes a URL
the same way.

## Math

Drafts are written in **portable LaTeX**: `$$ ... $$` for display math, `$ ... $`
inline. VS Code's built-in Markdown preview renders it (`Ctrl+K V`, no
extension needed) and so does GitHub, which keeps the preview check below
honest.

dev.to renders neither. Forem wants its own liquid tags, so the conversion is a
build step rather than something to type by hand:

```bash
scripts/publish_post.sh posts/malleys-method.md   # -> build/posts/*.devto.md
```

Upload the generated file; keep editing the draft. `build/` is gitignored, so
the copy is disposable by design, and the converter leaves fenced code blocks
alone -- a shell snippet containing `$RT_LOG` is not math.

Liquid tags are also the one syntax that travels nowhere else. Anything with a
KaTeX plugin -- Hugo, Astro, Jekyll, Hashnode, Substack -- reads `$$` directly,
so a draft written this way moves to a personal site later without a rewrite.

Symbols in prose (θ, π, ρ, r²) stay as plain Unicode. They need no math mode
and render identically everywhere.

## Checking a draft before upload

dev.to's renderer is close enough to GitHub's that previewing the file on
GitHub catches most layout problems. Tables and fenced code blocks with a
language tag both work as written.
