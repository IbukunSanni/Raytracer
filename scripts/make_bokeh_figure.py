"""Stack the two bokeh renders into the post's comparison figure.

    python scripts/make_bokeh_figure.py

Reads docs/images/bokeh-disk.png and bokeh-hexagon.png, writes
docs/images/bokeh-comparison.png. Regenerate the inputs with
assets/scenes/bokeh.lua -- once as shipped, once with the hexagon
rejection test swapped into SampleUnitDisk.
"""
from PIL import Image, ImageDraw, ImageFont

GAP, PAD, BAR = 12, 12, 34
BG, FG = (26, 30, 42), (214, 219, 232)


def label_font():
    for path in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        try:
            return ImageFont.truetype(path, 19)
        except OSError:
            continue
    return ImageFont.load_default()


def main():
    left = Image.open("docs/images/bokeh-disk.png").convert("RGB")
    right = Image.open("docs/images/bokeh-hexagon.png").convert("RGB")
    if left.size != right.size:
        raise SystemExit(f"size mismatch: {left.size} vs {right.size}")

    w, h = left.size
    out = Image.new("RGB", (PAD * 2 + w * 2 + GAP, PAD * 2 + BAR + h), BG)
    out.paste(left, (PAD, PAD + BAR))
    out.paste(right, (PAD + w + GAP, PAD + BAR))

    draw = ImageDraw.Draw(out)
    font = label_font()
    for x, text in ((PAD, "Disk aperture"), (PAD + w + GAP, "Hexagonal aperture")):
        draw.text((x + 2, PAD + 5), text, font=font, fill=FG)

    out.save("docs/images/bokeh-comparison.png")
    print(f"wrote docs/images/bokeh-comparison.png  {out.size[0]}x{out.size[1]}")


if __name__ == "__main__":
    main()
