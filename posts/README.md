# Posts

Publishable drafts, one file per post. `docs/ENGINEERING.md` holds the raw
notes these are written from; a file lands here once it is being drafted for
an actual publication rather than captured for later.

## Getting a post onto dev.to

Three routes, from least to most manual. The first is the one in use; the
second is built but not switched on.

### 1. Paste, with images served from GitHub (in use)

Used for part 5, the textures post, on 29 October. The images stay in the
repo and dev.to loads them from `raw.githubusercontent.com`, so nothing is
uploaded by hand.

1. **Commit and push every image the post uses.** The URLs point at a
   commit, and an image that is not in it is a broken link.
2. Convert, with the base pinned to that commit:

   ```bash
   python scripts/qmd_to_devto.py posts/textures.qmd --image-base https://raw.githubusercontent.com/IbukunSanni/Raytracer/$(git rev-parse HEAD)
   ```
   ```bash
   bash scripts/publish_post.sh build/posts/textures.md
   ```

3. Paste `build/posts/textures.devto.md` into dev.to's editor. It arrives
   as an unpublished draft.

Pin a commit hash, never `main`: a published post's images must not change
under it, and a changed image at a new commit is a new URL, so dev.to's
image cache never serves the old one. `$(git rev-parse HEAD)` fills the hash
in; a literal `<commit>` makes every image a "bad URI" in dev.to, and the
converter now refuses it.

### 2. Publishing from git (built, not switched on)

A `.qmd` draft reaches dev.to by being pushed. `.github/workflows/devto.yml`
runs `scripts/sync_devto.py` on every push to `main` that touches a post or
an image, and syncs each draft that opts in through its front matter:

```yaml
devto: draft      # an unpublished draft on dev.to
devto: live       # published
devto_id: 123456  # the article it updates; the first run prints it
```

A draft with no `devto:` line is never sent, so the posts published by hand
before this stay as they are. Opting one of them in makes the repo its
source of truth, and the next push overwrites it.

- **Edit here, not on dev.to.** Every run overwrites the article, so an edit
  made in the dev.to editor is lost on the next push.
- **Commit the images.** They are served from `raw.githubusercontent.com` at
  the pushed commit, so an image that is not committed is a broken link, and
  the run fails rather than publish one. Pinning to the commit also means a
  changed image gets a new URL, which dev.to's image cache has not seen.
- **Pin `devto_id` after the first run.** Until then the article is found by
  its exact title, so a renamed draft would become a second article.
- **`draft` never unpublishes.** A post that says `draft` over an article
  that is live is skipped with a warning.

The workflow needs one repository secret, `DEVTO_API_KEY` (dev.to: Settings
-> Extensions -> DEV Community API Keys). `python scripts/sync_devto.py
--dry-run` converts and checks every opted-in draft locally without sending
anything. Until the secret is set and a draft opts in, a push does nothing.

### 3. Upload by hand

Without `--image-base`, the converter leaves an `UPLOAD-URL` slot per figure:
upload each image in dev.to's editor and paste its URL over the slot, in
order. `--urls urls.txt` takes those URLs in one go instead. The sections
below describe this route's details.

## Front matter

dev.to reads the YAML block at the top. `published: false` uploads it as a
draft, which is the safe default -- flip it in the dev.to editor once the
images are in. Tags are lowercase, no spaces, four maximum.

## Images

**dev.to will not read `docs/images/` paths.** Markdown image links have to
point at a URL that resolves publicly: GitHub's raw URL at a pushed commit
(routes 1 and 2), or an image uploaded to dev.to's editor (route 3).

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
