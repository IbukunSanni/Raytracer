"""Pushes the opted-in drafts in posts/ to dev.to. Run by
.github/workflows/devto.yml on every push to main; it can run locally too.

    DEVTO_API_KEY=... python scripts/sync_devto.py --image-base \
        https://raw.githubusercontent.com/IbukunSanni/Raytracer/<commit>
    python scripts/sync_devto.py --dry-run     # convert and check, no API

A draft takes part only when its front matter says so:

    devto: draft      # kept on dev.to as an unpublished draft
    devto: live       # published
    devto_id: 123456  # optional: the article to update

Without devto_id, the article is the one of mine with the same title, or a
new one; the run prints the id to pin, so a later title change cannot fork
the post into two articles.

The repo is the source of truth: each run overwrites the article, so an edit
made in the dev.to editor is lost on the next push. Two guards:

  - A post with no devto: field is never touched, so a post published by
    hand stays as it is until it opts in.
  - A post that says draft is never pushed over a live article: dev.to
    would unpublish it. It is skipped with a warning instead.

Each draft goes through qmd_to_devto.py and publish_post.sh, the same as by
hand. Images become raw.githubusercontent.com URLs pinned to the commit, so
GitHub hosts them, and a changed image is a new URL that dev.to's image
cache has never seen.
"""

import glob
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request

API = "https://dev.to/api"

args = sys.argv[1:]
image_base, dry_run = None, False
while args:
    flag = args.pop(0)
    if flag == "--image-base":
        image_base = args.pop(0).rstrip("/")
    elif flag == "--dry-run":
        dry_run = True
    else:
        raise SystemExit(f"unknown flag {flag}")
key = os.environ.get("DEVTO_API_KEY", "")


def Field(header, name):
    found = re.search(rf"^{name}:\s*(.*)$", header, re.M)
    return found.group(1).strip().strip('"') if found else ""


def Call(method, path, body=None):
    request = urllib.request.Request(
        API + path, method=method,
        data=json.dumps(body).encode() if body is not None else None,
        headers={"api-key": key, "Content-Type": "application/json",
                 "Accept": "application/vnd.forem.api-v1+json",
                 "User-Agent": "raytracer-sync-devto"})
    try:
        with urllib.request.urlopen(request) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        raise SystemExit(f"{method} {path}: {error.code} {error.read().decode()}")


def MyArticles():
    """Every article of mine, drafts included, by page."""
    articles, page = [], 1
    while True:
        batch = Call("GET", f"/articles/me/all?per_page=1000&page={page}")
        articles += batch
        if len(batch) < 1000:
            return articles
        page += 1


def Convert(src):
    """The dev.to Markdown for one draft, through the same two scripts used
    by hand. Fails on a figure with no URL or an image not in the checkout:
    a raw URL to a file that was never committed is a broken image."""
    command = [sys.executable, "scripts/qmd_to_devto.py", src]
    if image_base:
        command += ["--image-base", image_base]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    name = os.path.basename(src)[:-len(".qmd")]
    subprocess.run(["bash", "scripts/publish_post.sh", f"build/posts/{name}.md"],
                   check=True, stdout=subprocess.DEVNULL)
    markdown = open(f"build/posts/{name}.devto.md", encoding="utf-8").read()
    if image_base:
        if "UPLOAD-URL" in markdown:
            raise SystemExit(f"{src}: a figure has no URL")
        for path in re.findall(re.escape(image_base) + r'/([^"]+)"', markdown):
            if not os.path.isfile(path):
                raise SystemExit(f"{src}: {path} is not in the repo; commit it")
    return markdown


posts = []
for src in sorted(glob.glob("posts/*.qmd")):
    text = open(src, encoding="utf-8").read().replace("\r\n", "\n")
    header = re.match(r"---\n(.*?)\n---\n", text, re.S)
    mode = Field(header.group(1), "devto") if header else ""
    if mode not in ("draft", "live"):
        if mode:
            raise SystemExit(f"{src}: devto: must be draft or live, not {mode}")
        continue
    posts.append((src, mode, Field(header.group(1), "title"),
                  Field(header.group(1), "devto_id")))

if not posts:
    print("no post opts in (devto: draft or devto: live); nothing to do")
    sys.exit(0)
# Checked only now, so a push with nothing opted in passes without a key.
if not dry_run and not key:
    raise SystemExit("DEVTO_API_KEY is not set (or pass --dry-run)")
if not dry_run and not image_base:
    raise SystemExit("--image-base is required: dev.to cannot read repo paths")

mine = [] if dry_run else MyArticles()
by_id = {str(a["id"]): a for a in mine}
by_title = {a["title"]: a for a in mine}

for src, mode, title, pinned in posts:
    markdown = Convert(src)
    if dry_run:
        print(f"{src}: converts cleanly ({mode}); not sent")
        continue
    existing = by_id.get(pinned) if pinned else by_title.get(title)
    if pinned and not existing:
        raise SystemExit(f"{src}: devto_id {pinned} is not one of my articles")
    if existing and existing["published"] and mode == "draft":
        print(f"::warning::{src}: article {existing['id']} is live and the post "
              "says devto: draft; skipped rather than unpublish it")
        continue
    body = {"article": {"body_markdown": markdown}}
    if existing:
        article = Call("PUT", f"/articles/{existing['id']}", body)
        print(f"{src}: updated {article['id']} ({mode}) {article['url']}")
    else:
        article = Call("POST", "/articles", body)
        print(f"{src}: created {article['id']} ({mode}) {article['url']}")
    if not pinned:
        print(f"::notice::{src}: add  devto_id: {article['id']}  to its front matter")
