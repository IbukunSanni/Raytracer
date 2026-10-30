"""Turns a Quarto draft in posts/ into a Markdown draft for publish_post.sh.

    python scripts/qmd_to_devto.py posts/energy-conservation.qmd
    scripts/publish_post.sh build/posts/energy-conservation.md

Quarto's own gfm output drops the front matter and the figure captions, so
this reads the .qmd directly instead. It does two things and leaves the rest
of the Markdown alone:

  - Front matter: keeps title and description, sets published: false (so the
    upload is a draft) unless the draft says devto: live, and turns
    categories into dev.to tags (four at most).
  - Figures: ![caption](../docs/images/x.png){fig-alt="..."} becomes the
    <figure> block the earlier posts use, with an <!-- IMAGE --> slot naming
    the file to upload. publish_post.sh counts the slots that are left. A
    link in the caption becomes an <a>: dev.to reads no Markdown inside HTML.
  - Callouts: ::: {.callout-note title="..."} becomes a blockquote with the
    title in bold, the nearest thing dev.to has.

Math is left as $...$ for publish_post.sh to convert.

Two ways to fill the images instead of leaving UPLOAD-URL slots:

    --image-base https://raw.githubusercontent.com/IbukunSanni/Raytracer/$(git rev-parse HEAD)
        Each image's src becomes that base plus its repo path, e.g.
        .../docs/images/furnace/furnace-diagram.png. The images must be pushed at
        that commit; pin a commit hash, not a branch, so they never change.
        A base with <, > or a space in it is refused: an unfilled <commit>
        placeholder made every image a "bad URI" in dev.to.
    --urls urls.txt
        One URL per line, in the order the figures appear: upload the images
        to dev.to in that order and paste each returned URL on its own line.
"""

import os
import re
import sys

args = sys.argv[1:]
src = args.pop(0)
image_base, urls = None, None
while args:
    flag, value = args.pop(0), args.pop(0)
    if flag == "--image-base":
        image_base = value.rstrip("/")
        # The usage line's <commit>, pasted unfilled, makes every src a bad URI.
        if re.search(r"[<>\s]", image_base):
            raise SystemExit(f"--image-base {value}: put the commit hash in, "
                             "e.g. $(git rev-parse HEAD)")
    elif flag == "--urls":
        urls = [l.strip() for l in open(value, encoding="utf-8") if l.strip()]
    else:
        raise SystemExit(f"unknown flag {flag}")
text = open(src, encoding="utf-8").read()

match = re.match(r"---\n(.*?)\n---\n", text, re.S)
if not match:
    raise SystemExit(f"{src}: no front matter")
header, body = match.group(1), text[match.end():]


def Field(name):
    found = re.search(rf"^{name}:\s*(.*)$", header, re.M)
    return found.group(1).strip() if found else ""


tags = [t.strip() for t in Field("categories").strip("[]").split(",") if t.strip()]
# Live only when the draft says so (devto: live); see scripts/sync_devto.py.
published = "true" if Field("devto") == "live" else "false"
front = "\n".join([
    "---",
    f"title: {Field('title')}",
    f"published: {published}",
    f"description: {Field('description')}",
    f"tags: {', '.join(tags[:4])}",
    "---",
])

# A caption may hold [text](url) links, so its brackets nest one level.
FIGURE = re.compile(r"!\[(?P<caption>(?:[^\[\]]|\[[^\]]*\]\([^)]*\))*)\]"
                    r"\((?P<path>[^)]+)\)(\{(?P<attrs>[^}]*)\})?")
LINK = re.compile(r"\[([^\]]*)\]\(([^)]*)\)")


def ToFigure(m):
    path = os.path.normpath(os.path.join(os.path.dirname(src), m["path"]))
    path = path.replace(os.sep, "/")
    alt = re.search(r'fig-alt="([^"]*)"', m["attrs"] or "")
    alt = alt.group(1) if alt else LINK.sub(r"\1", m["caption"])
    caption = LINK.sub(r'<a href="\2">\1</a>', m["caption"])
    width = re.search(r"width=(\d+)", m["attrs"] or "")
    width = f' width="{width.group(1)}"' if width else ""
    lines = ['<figure align="center">']
    if image_base:
        url = f"{image_base}/{path}"
    elif urls is not None:
        if not urls:
            raise SystemExit(f"--urls ran out at {path}")
        url = urls.pop(0)
    else:
        url = "UPLOAD-URL"
        lines.insert(0, f"<!-- IMAGE: upload {path}, then replace UPLOAD-URL below -->")
    return "\n".join(lines + [
        f'  <img src="{url}" alt="{alt}"{width}>',
        f"  <figcaption><em>{caption}</em></figcaption>",
        "</figure>",
    ])


body = FIGURE.sub(ToFigure, body)
if urls:
    raise SystemExit(f"--urls has {len(urls)} more URL(s) than figures")


def StartsBlock(line):
    """A line that must stay on its own: blank, a heading, a list item, a
    table row, HTML, a fence, a callout fence, or a rule."""
    s = line.strip()
    return (not s or s.startswith(("#", "<", "|", "```", ":::"))
            or line.startswith("- ") or s in ("$$", "---"))


def Unwrap(markdown):
    """Joins hard-wrapped lines, so dev.to cannot render a wrap as a break.

    Code fences and $$ blocks pass through untouched. A line indented under a
    list item joins that item.
    """
    out, fenced = [], False
    for line in markdown.split("\n"):
        s = line.strip()
        if s.startswith("```") or s == "$$":
            fenced = not fenced
            out.append(line)
            continue
        prev = out[-1].strip() if out else ""
        if (not fenced and not StartsBlock(line) and prev
                and not prev.startswith(("#", "<", "|", "```", ":::"))
                and prev not in ("$$", "---")):
            out[-1] = out[-1].rstrip() + " " + s
        else:
            out.append(line)
    return "\n".join(out)


body = Unwrap(body)

# After Unwrap, so the callout's text is already one line per paragraph.
CALLOUT = re.compile(r'^::: *\{\.callout-\w+(?: +title="(?P<title>[^"]*)")?\}\n'
                     r"(?P<content>.*?)\n:::$", re.M | re.S)


def ToBlockquote(m):
    lines = [f"> **{m['title']}**", ">"] if m["title"] else []
    return "\n".join(lines + [f"> {l}" if l else ">"
                              for l in m["content"].split("\n")])


body = CALLOUT.sub(ToBlockquote, body)

os.makedirs("build/posts", exist_ok=True)
out = os.path.join("build/posts", os.path.basename(src)[:-len(".qmd")] + ".md")
open(out, "w", encoding="utf-8", newline="\n").write(front + "\n" + body)
print("wrote", out)
