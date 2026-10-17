# Generates the Win12 XP Plasma Styles (taskbar: panel-background + tasks 9-slice SVGs, colors, metadata).
# All schemes x substyles:  python3 xptheme.py <outdir>       (install.sh -> ~/.local/share/plasma/desktoptheme)
# One custom colour:        python3 xptheme.py --custom '#3a6ea5' --sub classic --out <dir>   (main.qml)
# Window decorations (T26a): python3 xptheme.py --deco <outdir>  |  --deco --custom <hex> --sub s --out <dir>
# Colour schemes (T26b):     python3 xptheme.py --scheme <outdir> |  --scheme --custom <hex> --sub s --out <file.colors>
# Any-style taskbar (T31):  python3 xptheme.py --taskbar colour=#101010 alpha=0.8 radius=8 line=0 buttons=win11 accent=3d8ee6 solid=0
#                             fallback=default --out <dir>   (main.qml; only panel/tasks/colors, rest = fallback)
#                             [image=<png/jpg> mode=tile|stretch|top|middle|bottom imagealpha=0.3 tint=3daee9] (T33)
#                             [gradient=none|left-right|right-left|top-bottom|bottom-top strength=30] (W14)
import argparse, base64, json, os, re, struct

def frame(prefix, x0, y0, W, H, c, border, stops, hi=None, margins=(4, 4, 4, 4), gid="g", center="", under=None, img="", horiz=False, vpad=0, edge=None):
    """9 pieces of a rounded box. stops: [(offset, color, opacity)] vertical gradient.
    border: (color, opacity) or None. hi: top highlight (color, opacity) or None.
    edge: (outer colour, opacity, inner colour, opacity) = border + a 1 px inner ring on all four sides (readable glass).
    under: (color, opacity, height, full) bar on the bottom edge (T34 task indicator); full = also in the
    corners, else only the stretched middle piece with round ends (Win11 pill, inset by c).
    W14: img = a second fill (e.g. 'fill="url(#img)" fill-opacity=".5"') painted over every body shape; horiz = the
    gradient runs x0 -> x0 + W instead of top -> bottom."""
    p = (prefix + "-") if prefix else ""
    if edge:
        border = edge[:2]
    y0 += vpad  # W15 vpad: the top and bottom pieces get vpad px of transparent space (task buttons inside the gap ring)
    defs = '<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="%d" y1="%d" x2="%d" y2="%d">%s</linearGradient>' % (
        gid, *((x0, 0, x0 + W, 0) if horiz else (0, y0, 0, y0 + H)), "".join('<stop offset="%s" stop-color="%s" stop-opacity="%s"/>' % s for s in stops))
    b = 1 if border else 0
    F = 'fill="url(#%s)"' % gid
    def FF(s):  # the body shape, then the same shape again in the img fill
        return s + (s.replace(F, img) if img else "")
    B = ('fill="%s" fill-opacity="%s"' % border) if border else ""
    E = ('fill="%s" fill-opacity="%s"' % edge[2:]) if edge else ""
    x1, y1, x2, y2 = x0 + c, y0 + c, x0 + W - c, y0 + H - c
    out = []
    ext = {"topleft": (x0, c, -1), "top": (x1, W - 2 * c, -1), "topright": (x2, c, -1),
           "bottomleft": (x0, c, 1), "bottom": (x1, W - 2 * c, 1), "bottomright": (x2, c, 1)}
    def g(name, body):
        if vpad and name in ext:
            ex, ew, d = ext[name]
            body = '<rect x="%s" y="%s" width="%s" height="%s" fill="none"/>' % (ex, y0 - vpad if d < 0 else y0 + H, ew, vpad) + body
        out.append('<g id="%s%s">%s</g>' % (p, name, body))
    def rect(x, y, w, h, f):
        return '<rect x="%s" y="%s" width="%s" height="%s" %s/>' % (x, y, w, h, f) if w > 0 and h > 0 else ""
    def corner(cx, cy, sx, sy, r, f):
        # quarter disc of radius r around (cx,cy), toward (sx,sy) signs, filling the c-box
        if r <= 0:
            return ""
        sweep = 0 if sx * sy < 0 else 1
        return '<path d="M %s,%s L %s,%s A %s %s 0 0 %d %s,%s Z" %s/>' % (
            cx, cy, cx + sx * r, cy, r, r, sweep, cx, cy + sy * r, f)
    def piece_corner(name, cx, cy, sx, sy, bx, by):
        body = ""
        if c == 0 or (border is None):
            pass
        # transparent box keeps the element size exactly c x c
        body += rect(bx, by, c, c, 'fill="none"')
        sw = 0 if sx * sy < 0 else 1
        def ring(ro, ri, f):  # quarter ring between radii ro and ri
            return '<path d="M %s,%s A %s %s 0 0 %d %s,%s L %s,%s A %s %s 0 0 %d %s,%s Z" %s/>' % (
                cx + sx * ro, cy, ro, ro, sw, cx, cy + sy * ro, cx, cy + sy * ri, ri, ri, 1 - sw, cx + sx * ri, cy, f)
        if border:  # quarter ring, so translucent fills do not double up
            body += ring(c, c - b, B)
        body += FF(corner(cx, cy, sx, sy, c - b, F))
        if edge:  # readable glass: the dark outer line shows over white, this light inner one over black
            body += ring(c - b, max(0, c - b - 1), E)
        if under and under[3] and sy > 0:
            body += rect(bx, by + c - under[2], c, under[2], 'fill="%s" fill-opacity="%s"' % under[:2])
        g(name, body)
    piece_corner("topleft", x1, y1, -1, -1, x0, y0)
    piece_corner("topright", x2, y1, 1, -1, x2, y0)
    piece_corner("bottomleft", x1, y2, -1, 1, x0, y2)
    piece_corner("bottomright", x2, y2, 1, 1, x2, y2)
    top = (rect(x1, y0, W - 2 * c, b, B) if border else "") + FF(rect(x1, y0 + b, W - 2 * c, c - b, F))
    if edge:
        top += rect(x1, y0 + b, W - 2 * c, 1, E)
    elif hi:
        top += rect(x1, y0 + b, W - 2 * c, 1, 'fill="%s" fill-opacity="%s"' % hi)
    g("top", top)
    bar = ""
    if under:
        bar = '<rect x="%s" y="%s" width="%s" height="%s" rx="%s" fill="%s" fill-opacity="%s"/>' % (
            x1, y0 + H - under[2], W - 2 * c, under[2], 0 if under[3] else under[2] / 2, under[0], under[1])
    g("bottom", FF(rect(x1, y2, W - 2 * c, c - b, F)) + (rect(x1, y0 + H - b - 1, W - 2 * c, 1, E) if edge else "") + (rect(x1, y0 + H - b, W - 2 * c, b, B) if border else "") + bar)
    g("left", (rect(x0, y1, b, H - 2 * c, B) if border else "") + FF(rect(x0 + b, y1, c - b, H - 2 * c, F))
      + (rect(x0 + b, y1, 1, H - 2 * c, E) if edge else ""))
    g("right", FF(rect(x2, y1, c - b, H - 2 * c, F)) + (rect(x0 + W - b - 1, y1, 1, H - 2 * c, E) if edge else "") + (rect(x0 + W - b, y1, b, H - 2 * c, B) if border else ""))
    g("center", FF(rect(x1, y1, W - 2 * c, H - 2 * c, F)) + center)
    l, t, r, bm = margins
    t, bm, hy = t + vpad, bm + vpad, y0 + H + vpad + 2
    for i, (side, w, h) in enumerate([("left", l, 1), ("top", 1, t), ("right", r, 1), ("bottom", 1, bm)]):
        out.append('<rect id="%shint-%s-margin" x="%d" y="%d" width="%g" height="%g" fill="none"/>' % (p, side, x0 + i * 10, hy, w, h))
    return defs, "".join(out)

def shadow_tiles(x0, y0, S=12, op=.3):
    """Readable glass: Breeze-style shadow-* tiles (a 3x3 grid without its center, black op -> 0 going outward;
    corners radial) and the 4 shadow-hint-*-margin elements. plasmashell hands them to KWin as the panel's shadow
    (plasma-workspace shell/panelshadows.cpp); a floating panel's negative shadow margins make them hug the bar."""
    st = '<stop offset="0" stop-color="#000000" stop-opacity="%s"/><stop offset="1" stop-color="#000000" stop-opacity="0"/>' % op
    defs, out = [], []
    for name, col, row in (("topleft", 0, 0), ("top", 1, 0), ("topright", 2, 0), ("left", 0, 1),
                           ("right", 2, 1), ("bottomleft", 0, 2), ("bottom", 1, 2), ("bottomright", 2, 2)):
        dx, dy = col - 1, row - 1
        ix, iy = x0 + S * (1 + (dx > 0)), y0 + S * (1 + (dy > 0))  # where the tile touches the panel
        if dx and dy:
            defs.append('<radialGradient id="sh-%s" gradientUnits="userSpaceOnUse" cx="%d" cy="%d" r="%d">%s</radialGradient>' % (name, ix, iy, S, st))
        else:
            defs.append('<linearGradient id="sh-%s" gradientUnits="userSpaceOnUse" x1="%d" y1="%d" x2="%d" y2="%d">%s</linearGradient>' % (
                name, ix, iy, ix + dx * S, iy + dy * S, st))
        out.append('<g id="shadow-%s"><rect x="%d" y="%d" width="%d" height="%d" fill="url(#sh-%s)"/></g>' % (name, x0 + col * S, y0 + row * S, S, S, name))
    out += ['<rect id="shadow-hint-%s-margin" x="%d" y="%d" width="%d" height="%d" fill="none"/>' % (sd, x0 + i * (S + 10), y0 + 3 * S + 10, S, S)
            for i, sd in enumerate(("top", "right", "bottom", "left"))]
    return "".join(defs), "".join(out)

def svg(path, frames, extra="", size=(400, 0)):
    defs, body = zip(*frames)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="%d" height="%d">\n<defs>%s</defs>\n%s%s\n</svg>\n'
                % (size[0], size[1] or len(frames) * 60 + 20, "".join(defs), "\n".join(body), extra))

W, K = "#ffffff", "#000000"
OPT = {"radius": -1, "title": 0}   # T41 window options (--radius/--title), read by deco_frame and the rc writers
ORANGE = [(0, "#fcc77b", 1), (1, "#f39a32", 1)]

def rgb(h):
    h = h.lstrip("#")[-6:]
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

def mix(a, b, t):
    """a blended toward b by t (0..1), as #rrggbb."""
    return "#%02x%02x%02x" % tuple(round(x + (y - x) * t) for x, y in zip(rgb(a), rgb(b)))

def light(h):
    r, g, b = rgb(h)
    return 0.299 * r + 0.587 * g + 0.114 * b > 150

# Luna blue, exactly as before.
BLUE = {
    "classic": dict(
        panel=[(0, "#3f80ee", 1), (0.1, "#2a66df", 1), (0.5, "#245edb", 1), (0.92, "#1f53c4", 1), (1, "#1941a5", 1)],
        edge=("#6aa6f6", 1), c=4, base="#245edb",
        tasks={
            "normal": (("#1a4bb5", 1), [(0, "#5a9bfb", 1), (1, "#3c81f3", 1)], (W, 0.35)),
            "hover": (("#1a4bb5", 1), [(0, "#7ab4ff", 1), (1, "#4f93ff", 1)], (W, 0.45)),
            "focus": (("#10368a", 1), [(0, "#1a48a8", 1), (1, "#2a63cf", 1)], None),
            "attention": (("#c76d10", 1), ORANGE, (W, 0.4)),
            "minimized": (("#1a4bb5", 1), [(0, "#4f90f7", 1), (1, "#3577ea", 1)], (W, 0.25)),
        }),
    "modern": dict(
        panel=[(0, "#3a7bea", 1), (1, "#2c63cc", 1)],
        edge=(W, 0.35), c=8, base="#2c63cc",
        tasks={
            "normal": ((W, 0.18), [(0, W, 0.14), (1, W, 0.08)], None),
            "hover": ((W, 0.32), [(0, W, 0.26), (1, W, 0.16)], None),
            "focus": ((W, 0.6), [(0, W, 0.38), (1, W, 0.26)], None),
            "attention": (("#fcc77b", 0.9), [(0, "#fcc77b", 0.9), (1, "#f39a32", 0.9)], None),
            "minimized": ((W, 0.1), [(0, W, 0.07), (1, W, 0.04)], None),
        }),
}
# Other schemes: taskbar body colour; the rest is derived from it.
BASES = {"olive": "#7f9656", "silver": "#c9c9d9"}
# "luna" (settings: Classic XP): the classic buttons on the glossy bars measured from real XP screenshots.
LUNA = {
    "blue": ([(0, "#3c8ae8", 1), (0.06, "#188ced", 1), (0.15, "#0570e2", 1), (0.32, "#0b54dd", 1), (0.6, "#085be3", 1),
              (0.88, "#0262ec", 1), (0.94, "#055ce3", 1), (1, "#064cc4", 1)], ("#5aa6ff", 1), "#245edb"),
    "olive": ([(0, "#e1e8b9", 1), (0.1, "#b9c68e", 1), (0.22, "#acb984", 1), (0.55, "#acb984", 1), (0.8, "#c3ce96", 1),
               (0.92, "#bcca90", 1), (1, "#a9b781", 1)], ("#f4f7de", 1), "#acb984"),
    "silver": ([(0, "#ffffff", 1), (0.08, "#f3f3f7", 1), (0.25, "#e3e3eb", 1), (0.55, "#d6d6e0", 1), (0.8, "#e4e4ec", 1),
                (0.92, "#cfcfdc", 1), (1, "#a9a9be", 1)], (W, 1), "#d6d6e0"),
}

def luna(sch):
    panel, edge, B = LUNA[sch]
    return dict(BLUE["classic"] if sch == "blue" else derive(B, "classic"), panel=panel, edge=edge, base=B)

def derive(B, sub):
    if sub != "modern":   # classic, luna (custom colour: same as classic)
        return dict(
            panel=[(0, mix(B, W, .25), 1), (0.1, mix(B, W, .08), 1), (0.5, B, 1), (0.92, mix(B, K, .12), 1), (1, mix(B, K, .3), 1)],
            edge=(mix(B, W, .45), 1), c=4, base=B,
            tasks={
                "normal": ((mix(B, K, .3), 1), [(0, mix(B, W, .3), 1), (1, mix(B, W, .15), 1)], (W, 0.35)),
                "hover": ((mix(B, K, .3), 1), [(0, mix(B, W, .45), 1), (1, mix(B, W, .3), 1)], (W, 0.45)),
                "focus": ((mix(B, K, .5), 1), [(0, mix(B, K, .3), 1), (1, mix(B, K, .1), 1)], None),
                "attention": (("#c76d10", 1), ORANGE, (W, 0.4)),
                "minimized": ((mix(B, K, .3), 1), [(0, mix(B, W, .22), 1), (1, mix(B, W, .08), 1)], (W, 0.25)),
            })
    ink = "#1f2440" if light(B) else W  # light panel: dark translucent buttons instead of white ones
    k = 0.6 if light(B) else 1
    return dict(
        panel=[(0, mix(B, W, .12), 1), (1, B, 1)], edge=(W, 0.35), c=8, base=B,
        tasks={
            "normal": ((ink, .18 * k), [(0, ink, .14 * k), (1, ink, .08 * k)], None),
            "hover": ((ink, .32 * k), [(0, ink, .26 * k), (1, ink, .16 * k)], None),
            "focus": ((ink, .6 * k), [(0, ink, .38 * k), (1, ink, .26 * k)], None),
            "attention": (("#fcc77b", 0.9), [(0, "#fcc77b", 0.9), (1, "#f39a32", 0.9)], None),
            "minimized": ((ink, .1 * k), [(0, ink, .07 * k), (1, ink, .04 * k)], None),
        })

def colors(B):
    """Plasma colour scheme: text readable on the panel colour B."""
    c = lambda h: "%d,%d,%d" % rgb(h)
    lt = light(B)
    fg = "#1f2440" if lt else W
    acc, acc2 = ("#a85a00", "#7a4000") if lt else ("#fcc77b", "#f39a32")
    group = "\n".join([
        "BackgroundNormal=" + c(B), "BackgroundAlternate=" + c(mix(B, K, .12)),
        "ForegroundNormal=" + c(fg), "ForegroundInactive=" + c(mix(fg, B, .25)), "ForegroundActive=" + c(fg),
        "ForegroundLink=" + c(acc), "ForegroundVisited=" + c(acc2),
        "ForegroundNegative=" + ("170,30,20" if lt else "255,120,100"), "ForegroundNeutral=" + c(acc),
        "ForegroundPositive=" + ("30,110,30" if lt else "140,230,130"),
        "DecorationFocus=" + c(acc), "DecorationHover=" + c(mix(B, W, .4))])
    out = "".join("[Colors:%s]\n%s\n\n" % (g, group) for g in ("Window", "View", "Button", "Header", "Complementary", "Tooltip"))
    return out + """[Colors:Selection]
BackgroundNormal=243,154,50
BackgroundAlternate=252,199,123
ForegroundNormal=0,0,0
ForegroundInactive=40,40,40
ForegroundActive=0,0,0
ForegroundLink=0,19,107
ForegroundVisited=0,19,107
ForegroundNegative=150,20,10
ForegroundNeutral=90,50,0
ForegroundPositive=20,90,20
DecorationFocus=243,154,50
DecorationHover=252,199,123
"""

def write(base, t, label):
    name = os.path.basename(os.path.normpath(base))
    # panel: square, 2px edge rows; top edge = light line
    svg(os.path.join(base, "widgets/panel-background.svg"),
        [frame("", 10, 10, 60, 40, 2, None, t["panel"], hi=t["edge"], margins=(2, 2, 2, 2), gid="panel")],
        '<rect id="hint-stretch-borders" x="200" y="10" width="1" height="1" fill="none"/>')
    frames = [frame(pfx, 10, 10 + i * 60, 60, 40, t["c"], bd, st, hi=hi, margins=(6, 4, 6, 4), gid="t" + pfx)
              for i, (pfx, (bd, st, hi)) in enumerate(t["tasks"].items())]
    svg(os.path.join(base, "widgets/tasks.svg"), frames,
        '<rect id="hint-stretch-borders" x="200" y="10" width="1" height="1" fill="none"/>')
    with open(os.path.join(base, "colors"), "w") as f:
        f.write(colors(t["base"]))
    with open(os.path.join(base, "plasmarc"), "w") as f:
        f.write("[Settings]\nFallbackTheme=default\n\n[AdaptiveTransparency]\nenabled=false\n")
    with open(os.path.join(base, "metadata.json"), "w") as f:
        json.dump({"KPlugin": {"Authors": [{"Name": "Win12"}], "Description": "Windows XP taskbar for the Win12 XP menu",
                               "Id": name, "License": "LGPL", "Name": "Win12 XP " + label, "Version": "1.0"},
                   "X-Plasma-API": "5.0"}, f, indent=4)

def imgsize(data):
    """(w, h) of PNG / JPEG bytes."""
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", data[16:24])
    i = 2
    while i < len(data) - 9:  # JPEG: walk the segments to the SOFn frame header
        seg, ln = data[i + 1], struct.unpack(">H", data[i + 2:i + 4])[0]
        if 0xC0 <= seg <= 0xCF and seg not in (0xC4, 0xC8, 0xCC):
            h, w = struct.unpack(">HH", data[i + 5:i + 9])
            return w, h
        i += 2 + ln
    raise ValueError("not a PNG or JPEG image")

def taskbar(base, colour="#101010", alpha="0.8", radius="0", line="0", fallback="default",
            image="", mode="tile", imagealpha="1", tint="", buttons="win10", accent="", solid="0", gap="",
            gradient="none", strength="30", ring="0", size="", edge="0"):
    """Plasma Style that only replaces panel-background (plain, opaque/ and translucent/ for blur), tasks
    and colors; everything else comes from FallbackTheme (the user's own style).
    T33 texture: image (PNG/JPEG) in the 9-slice center; mode tile (native size, hint-tile-center) | stretch |
    top/middle/bottom (scaled to cover a panel-shaped box, cropped at that edge); tint = colour multiplied over.
    W14: the texture is a userSpace <pattern> painted over every piece (rounded corners clip it, Qt >= 6.7);
    gradient left-right|right-left|top-bottom|bottom-top = bar colour -> strength % toward white (dark bar) / black.
    Readable glass: edge=1 = a two-tone 1 px edge (black .35 outside, white .25 inside: one shows on any background)
    and a soft shadow (shadow-* tiles; gap mode paints it into the ring, its docked panel has no shadow margin)."""
    B = "#" + colour.lstrip("#")[-6:]
    rgb(B)
    a, r = max(0.0, min(1.0, float(alpha))), max(2, min(40, int(radius)))
    hi = (mix(B, W, .35), 1) if line not in ("0", "", "false") else None
    ed = (K, .35, W, .25) if edge not in ("0", "", "false") else None
    fw, fh, tex, hints = max(60, 2 * r + 20), max(40, 2 * r + 2), "", '<rect id="hint-stretch-borders" x="200" y="10" width="1" height="1" fill="none"/>'
    pat = ""
    if image:
        with open(image, "rb") as f:
            data = f.read()
        iw, ih = imgsize(data)
        mime = "png" if data[:4] == b"\x89PNG" else "jpeg"
        if mode == "tile":  # center and borders tile at the image size (no hint-stretch-borders), seamless
            fw, fh = iw + 2 * r, ih + 2 * r
            x, y, w, h = 10 + r, 10 + r, iw, ih
            hints = '<rect id="hint-tile-center" x="200" y="20" width="1" height="1" fill="none"/>'
        else:
            # ponytail: the frame is panel-shaped (48:1), Plasma stretches it; exact for a ~1920x40 panel
            fw, fh = 960 + 2 * r, 20 + 2 * r
            x, y, w, h = 10, 10, fw, fh
        par = {"stretch": "none", "top": "xMidYMin slice", "middle": "xMidYMid slice", "bottom": "xMidYMax slice"}.get(mode, "none")
        tex = '<image x="0" y="0" width="%d" height="%d" preserveAspectRatio="%s" xlink:href="data:image/%s;base64,%s"/>' % (
            w, h, par, mime, base64.b64encode(data).decode())
        if tint:
            tex += '<rect x="0" y="0" width="%d" height="%d" fill="#%s" comp-op="multiply"/>' % (w, h, tint.lstrip("#")[-6:])
        pat = '<pattern id="img" patternUnits="userSpaceOnUse" x="%d" y="%d" width="%d" height="%d">%s</pattern>' % (x, y, w, h, tex)
    g2 = mix(B, K if light(B) else W, max(0, min(100, int(strength))) / 100)
    horiz = gradient in ("left-right", "right-left")
    e = r / fw if horiz else 0  # horizontal: the colour change happens in the stretched middle, caps stay flat
    def stops(op):
        if gradient not in ("left-right", "right-left", "top-bottom", "bottom-top"):
            return [(0, B, op), (1, B, op)]
        c1, c2 = (B, g2) if gradient in ("left-right", "top-bottom") else (g2, B)
        return [(0, c1, op), ("%.4f" % e, c1, op), ("%.4f" % (1 - e), c2, op), (1, c2, op)]
    n = int(ring or 0)
    if n > 0:  # W15 gap mode: docked panel, the island inside a transparent ring of n px
        RW, RH = (max(2 * n + 4, int(v or 0)) for v in (size.split("x") + ["", ""])[:2])
        def ring_frame(prefix, x0, y0, st, gid, img="", edge=None):
            """The whole island in the center piece at the exact panel size RW x RH: a docked bottom panel drops
            its bottom/left/right edge pieces (enabledBorders), so a 9-slice ring cannot be drawn."""
            p = prefix + "-" if prefix else ""
            ix, iy, iw, ih = x0 + n, y0 + n, RW - 2 * n, RH - 2 * n
            defs = '<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="%d" y1="%d" x2="%d" y2="%d">%s</linearGradient>' % (
                gid, *((ix, 0, ix + iw, 0) if horiz else (0, iy, 0, iy + ih)), "".join('<stop offset="%s" stop-color="%s" stop-opacity="%s"/>' % x for x in st))
            rr = min(r, ih // 2)
            box = '<rect x="%d" y="%d" width="%d" height="%d" rx="%d" %%s/>' % (ix, iy, iw, ih, rr)
            def outline(d, rx, col, o):  # 1 px stroke d px outside (d < 0: inside) the island
                return '<rect x="%s" y="%s" width="%s" height="%s" rx="%s" fill="none" stroke="%s" stroke-opacity="%s" stroke-width="1"/>' % (
                    ix - d, iy - d, iw + 2 * d, ih + 2 * d, rx, col, o)
            sh = ln = ""
            if edge:
                # shadow: 1 px rings outside the island, each as dark as k nested .05 black rects would make it
                # (rings, not filled rects: the stack would show through the glass and darken it)
                k = min(6, n - 1)
                sh = "".join(outline(i - .5, rr + i - .5, K, "%.3f" % (1 - .95 ** (k + 1 - i))) for i in range(1, k + 1))
                ln = outline(-.5, max(0, rr - .5), *edge[:2]) + outline(-1.5, max(0, rr - 1.5), *edge[2:])
            body = '<g id="%scenter"><rect x="%d" y="%d" width="%d" height="%d" fill="none"/>%s%s%s%s</g>' % (
                p, x0, y0, RW, RH, sh, box % ('fill="url(#%s)"' % gid), box % img if img else "", ln)
            m = max(2, r - 10)
            body += "".join('<rect id="%shint-%s-margin" x="%d" y="%d" width="%d" height="%d" fill="none"/>' % (p, sd, x0 + i * 40, y0 + RH + 2, w, h)
                            for i, (sd, w, h) in enumerate([("left", n + m, 1), ("top", 1, n + 2), ("right", n + m, 1), ("bottom", 1, n + 2)]))
            return defs, body
    for pre, op in (("", a), ("translucent/", a), ("opaque/", 1)):
        t = 'fill="url(#img)" fill-opacity="%.3f"' % (op * max(0.0, min(1.0, float(imagealpha)))) if pat else ""
        if n > 0:
            svg(os.path.join(base, pre + "widgets/panel-background.svg"),
                [(pat, ""), ring_frame("", 10, 10, stops(op), "panel", t, ed), ring_frame("mask", 10, RH + 50, [(0, K, 1), (1, K, 1)], "mask")],
                size=(RW + 20, 2 * RH + 100))
            continue
        svg(os.path.join(base, pre + "widgets/panel-background.svg"),
            [(pat, ""), frame("", 10, 10, fw, fh, r, None, stops(op), hi=hi, margins=(max(2, r - 10), 2, max(2, r - 10), 2), gid="panel", img=t, horiz=horiz, edge=ed),
             frame("mask", 10, 20 + fh, fw, fh, r, None, [(0, K, 1), (1, K, 1)], gid="mask")]
            # W12 gap: Plasma reads a floating panel's margins from the "floating" prefix (else 8 px each side);
            # a 0-high hint is "missing" to KSvg (it then uses the 4 px edge piece), .01 rounds to 0
            + ([frame("floating", 10, 30 + 2 * fh, 60, 40, 4, None, [(0, K, 0), (1, K, 0)], margins=(8, 8, 8, int(gap) or .01), gid="float")] if gap else [])
            + ([shadow_tiles(10, 30 + 2 * fh + 60)] if ed else []),  # below the floating frame and its hints
            hints.replace('x="200"', 'x="%d"' % (fw + 40)), size=(fw + 60, 2 * fh + (180 if ed else 100)))
    ink = "#000000" if light(B) else W
    acc = "#" + (accent.lstrip("#")[-6:] or "3d8ee6")
    fa = 2 if solid not in ("0", "", "false") else 1  # T34 Start11 "Paint focused item backgrounds less transparent"
    def fill(col, op, op2=None):
        return [(0, col, min(1, op)), (1, col, min(1, op if op2 is None else op2))]
    clear = (None, fill(ink, 0), None)
    att = (None, fill("#f39a32", .8), None)
    # T34 buttons: (corner, {prefix: (border, stops, hi, under)}); plasma = no tasks.svg, the fallback's is used.
    # ponytail: indicators sit on the bottom edge (bottom panel); a left/top panel would need north-/west- prefixes.
    tasks = {
        "win10": (2, {"normal": clear + ((ink, .45, 2, True),), "minimized": clear + ((ink, .3, 2, True),),
                      "hover": (None, fill(ink, .12), None, (ink, .45, 2, True)),
                      "focus": (None, fill(ink, .18 * fa), None, (acc, 1, 2, True)), "attention": att + (None,)}),
        "win11": (6, {"normal": clear + ((ink, .4, 3, False),), "minimized": clear + ((ink, .25, 3, False),),
                      "hover": (None, fill(ink, .08), None, (ink, .4, 3, False)),
                      "focus": ((ink, .08), fill(ink, .1 * fa), None, (acc, 1, 3, False)), "attention": att + (None,)}),
        "win7": (3, {"normal": ((W, .25), fill(W, .14, .02), (W, .3), None),
                     "minimized": ((W, .15), fill(W, .07, .01), None, None),
                     "hover": ((W, .45), fill(W, .3, .08), (W, .5), None),
                     "focus": ((W, .55), fill(W, .22 * fa, .06 * fa), (W, .6), None),
                     "attention": ((W, .5), fill("#f39a32", .85, .55), (W, .5), None)}),
    }.get(buttons)
    if tasks:
        c, states = tasks
        frames = [frame(pfx, 10, 10 + i * (60 + 2 * n), 60, 40, c, bd, st, hi=h, margins=(6, 4, 6, 4), gid="t" + pfx, under=u, vpad=n)
                  for i, (pfx, (bd, st, h, u)) in enumerate(states.items())]
        svg(os.path.join(base, "widgets/tasks.svg"), frames,
            '<rect id="hint-stretch-borders" x="200" y="10" width="1" height="1" fill="none"/>')
    with open(os.path.join(base, "colors"), "w") as f:
        # No XP-orange Selection group here: selection/highlight stay the system's (blue by default).
        f.write(colors(B).split("[Colors:Selection]")[0])
    with open(os.path.join(base, "plasmarc"), "w") as f:
        f.write("[Settings]\nFallbackTheme=%s\n\n[AdaptiveTransparency]\nenabled=false\n" % fallback)
    name = os.path.basename(os.path.normpath(base))
    with open(os.path.join(base, "metadata.json"), "w") as f:
        json.dump({"KPlugin": {"Authors": [{"Name": "Win12"}], "Description": "Win12 taskbar on top of " + fallback,
                               "Id": name, "License": "LGPL", "Name": "Win12 Taskbar " + B, "Version": "1.0"},
                   "X-Plasma-API": "5.0", "X-Plasma-FallbackTheme": fallback}, f, indent=4)

# T26a window decorations: Aurorae SVG themes (~/.local/share/aurorae/themes/<name>), same names as the Plasma Styles.
DECO_BASES = {"blue": "#0054e3", "olive": BASES["olive"], "silver": BASES["silver"]}
BUTTONS = ("close", "minimize", "maximize", "restore", "help", "alldesktops", "keepabove", "keepbelow", "shade")
GLYPHS = {  # unit box 0..1, stroked
    "close": "M .2,.2 L .8,.8 M .8,.2 L .2,.8", "minimize": "M .2,.78 L .62,.78",
    "maximize": "M .18,.2 H .82 V .82 H .18 Z M .18,.3 H .82", "restore": "M .32,.18 H .84 V .62 M .16,.38 H .66 V .84 H .16 Z M .16,.46 H .66",
    "help": "M .3,.32 C .3,.1 .72,.1 .72,.32 C .72,.5 .5,.5 .5,.66 M .5,.82 V .86", "alldesktops": "M .3,.5 H .7 M .5,.3 V .7",
    "keepabove": "M .2,.66 L .5,.3 L .8,.66", "keepbelow": "M .2,.34 L .5,.7 L .8,.34", "shade": "M .2,.6 L .5,.3 L .8,.6 M .2,.82 H .8"}

def deco_frame(prefix, y0, T, S, ry, stops, side, line, hi, gid):
    """Window frame: title piece T high, borders S wide (= the rc borders, Aurorae aligns them), top corners
    rounded (rx S, ry), bottom corners square. Box 100 x T+40 at (10, y0). T41: OPT radius (>= 0) = rx = ry
    (corner box widened to fit; the client covers the extra border)."""
    rx = S
    if OPT["radius"] >= 0:
        rx = ry = OPT["radius"]
        S = max(S, rx)
    x0, W, H = 10, 100, T + 40
    x2, yb, F, L, Sd = x0 + W - S, y0 + H - S, 'fill="url(#%s)"' % gid, 'fill="%s"' % line, 'fill="%s"' % side
    r = lambda x, y, w, h, f: '<rect x="%s" y="%s" width="%s" height="%s" %s/>' % (x, y, w, h, f)
    def corner(cx, sx):
        """Top corner at outer edge cx, toward the inside by sx: the 1px outline as a ring and the fill inside it,
        never overlapping (translucent frames), all inside the S x T box."""
        e, sw = lambda d: cx + sx * d, 1 if sx > 0 else 0
        if rx > 1 and ry > 1:
            ring = 'M %s,%s V %s A %s %s 0 0 %d %s,%s H %s V %s H %s A %s %s 0 0 %d %s,%s V %s Z' % (
                cx, y0 + T, y0 + ry, rx, ry, sw, e(rx), y0, e(S), y0 + 1, e(rx), rx - 1, ry - 1, 1 - sw, e(1), y0 + ry, y0 + T)
            fill = 'M %s,%s V %s A %s %s 0 0 %d %s,%s H %s V %s Z' % (
                e(1), y0 + T, y0 + ry, rx - 1, ry - 1, sw, e(rx), y0 + 1, e(S), y0 + T)
        else:
            ring = 'M %s,%s V %s H %s V %s H %s V %s Z' % (cx, y0 + T, y0, e(S), y0 + 1, e(1), y0 + T)
            fill = 'M %s,%s V %s H %s V %s Z' % (e(1), y0 + T, y0 + 1, e(S), y0 + T) if S > 1 else ""
        return '<path d="%s" %s/>' % (ring, L) + ('<path d="%s" %s/>' % (fill, F) if fill else "")
    defs = '<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="0" y1="%d" x2="0" y2="%d">%s</linearGradient>' % (
        gid, y0, y0 + T, "".join('<stop offset="%s" stop-color="%s" stop-opacity="%s"/>' % (tuple(s) + (1,))[:3] for s in stops))
    p = prefix + "-"
    g = lambda n, b: '<g id="%s%s">%s</g>' % (p, n, b)
    out = [g("topleft", r(x0, y0, S, T, 'fill="none"') + corner(x0, 1)),
           g("topright", r(x2, y0, S, T, 'fill="none"') + corner(x0 + W, -1)),
           g("top", r(x0 + S, y0, W - 2 * S, 1, L) + r(x0 + S, y0 + 1, W - 2 * S, 1, 'fill="%s"' % hi) + r(x0 + S, y0 + 2, W - 2 * S, T - 2, F)),
           g("left", r(x0, y0 + T, 1, H - T - S, L) + r(x0 + 1, y0 + T, S - 1, H - T - S, Sd)),
           g("right", r(x2, y0 + T, S - 1, H - T - S, Sd) + r(x0 + W - 1, y0 + T, 1, H - T - S, L)),
           g("bottomleft", r(x0, yb, 1, S, L) + r(x0 + 1, y0 + H - 1, S - 1, 1, L) + r(x0 + 1, yb, S - 1, S - 1, Sd)),
           g("bottomright", r(x0 + W - 1, yb, 1, S, L) + r(x2, y0 + H - 1, S - 1, 1, L) + r(x2, yb, S - 1, S - 1, Sd)),
           g("bottom", r(x0 + S, yb, W - 2 * S, S - 1, Sd) + r(x0 + S, y0 + H - 1, W - 2 * S, 1, L)),
           g("center", r(x0 + S, y0 + T, W - 2 * S, H - T - S, Sd))]
    return defs, "".join(out)

def write_deco(base, B, sub, label):
    """Aurorae theme for panel/title colour B: decoration.svg, one svg per button, <name>rc, metadata.desktop."""
    name = os.path.basename(os.path.normpath(base))
    lt = light(B)
    ink = "#1f2440" if lt else W
    Bi = mix(B, "#d4d8e4", .55)  # inactive: washed out
    classic, luna = sub != "modern", sub == "luna"
    # luna (Classic XP): taller glossy title, rounder top, bigger buttons, XP's red-orange close.
    S, ry, T, bs, c = (4, 9, 30, 22, 3) if luna else (4, 7, 28, 21, 3) if classic else (5, 10, 30, 20, 5)
    T = OPT["title"] or T   # T41
    bs = min(bs, T - 2)
    def stops(b):
        if luna:
            return [(0, mix(b, W, .5)), (.08, mix(b, W, .2)), (.18, b), (.6, mix(b, K, .03)), (.88, mix(b, W, .16)), (1, mix(b, K, .25))]
        return ([(0, mix(b, W, .4)), (.12, mix(b, W, .08)), (.5, b), (.85, mix(b, W, .1)), (1, mix(b, K, .15))] if classic
                else [(0, mix(b, W, .14)), (1, b)])
    svg(os.path.join(base, "decoration.svg"),
        [deco_frame("decoration", 10, T, S, ry, stops(B), mix(B, K, .05), mix(B, K, .45), mix(B, W, .5), "da"),
         deco_frame("decoration-inactive", T + 70, T, S, ry, stops(Bi), Bi, mix(Bi, K, .3), mix(Bi, W, .5), "di")], size=(130, 2 * T + 140))
    red = [(0, "#f2a68c", 1), (.45, "#e04343", 1), (1, "#b02a1c", 1)] if luna else [(0, "#e8906c", 1), (1, "#c13c13", 1)]
    for btn in BUTTONS:
        frames = []
        for i, st in enumerate(("active", "hover", "pressed", "deactivated", "inactive", "hover-inactive", "pressed-inactive", "deactivated-inactive")):
            act, b = not st.endswith("inactive"), B if not st.endswith("inactive") else Bi
            k = {"hover": .25, "pressed": -.2}.get(st.split("-")[0], 0)
            tone = lambda h: mix(h, W, k) if k > 0 else mix(h, K, -k)
            gl = W if (btn == "close" or not lt) else ink
            if classic:
                fill = ([(o, tone(col), 1) for o, col, _ in red] if btn == "close" else [(0, tone(mix(b, W, .55)), 1), (.45, tone(mix(b, W, .18)), 1), (1, tone(mix(b, K, .05)), 1)] if luna
                        else [(0, tone(mix(b, W, .4)), 1), (1, tone(mix(b, W, .02)), 1)])
                bd = (W, 1 if act else .6)
            else:  # modern: translucent, close turns red on hover/press
                hot = btn == "close" and st.split("-")[0] in ("hover", "pressed")
                fill = [(o, tone(col), 1) for o, col, _ in red] if hot else [(0, ink, {"hover": .32, "pressed": .12}.get(st.split("-")[0], .18)), (1, ink, {"hover": .24, "pressed": .08}.get(st.split("-")[0], .1))]
                bd = None
            op = .45 if st.startswith("deactivated") else 1 if act else .7
            s = bs - 2 * c - 2 if classic else bs - 2 * c  # glyph stays inside the center piece
            gly = '<path d="%s" transform="translate(%s,%s) scale(%s)" fill="none" stroke="%s" stroke-opacity="%s" stroke-width="%s" stroke-linecap="round" stroke-linejoin="round"/>' % (
                GLYPHS[btn], 10 + c + (bs - 2 * c - s) / 2, 10 + i * 40 + c + (bs - 2 * c - s) / 2, s, gl, op, round(2.0 / s, 3))
            frames.append(frame(st, 10, 10 + i * 40, bs, bs, c, bd, fill, gid="b%d" % i, margins=(c, c, c, c), center=gly))
        svg(os.path.join(base, btn + ".svg"), frames, size=(60, 8 * 40 + 20))
    rgbs = lambda h: "%d,%d,%d" % rgb(h)
    with open(os.path.join(base, name + "rc"), "w") as f:
        f.write("[General]\nActiveTextColor=%s\nInactiveTextColor=%s\nUseTextShadow=%s\nActiveTextShadowColor=%s\n"
                "InactiveTextShadowColor=%s\nTextShadowOffsetX=1\nTextShadowOffsetY=1\nTitleAlignment=Left\n"
                "TitleVerticalAlignment=Center\nAnimation=0\nShadow=true\n\n[Layout]\nBorderLeft=%d\nBorderRight=%d\n"
                "BorderBottom=%d\nTitleEdgeTop=%d\nTitleEdgeBottom=%d\nTitleEdgeLeft=%d\nTitleEdgeRight=%d\nTitleBorderLeft=4\n"
                "TitleBorderRight=4\nTitleHeight=%d\nButtonWidth=%d\nButtonHeight=%d\nButtonSpacing=2\nButtonMarginTop=0\n"
                "ExplicitButtonSpacer=8\nPaddingTop=0\nPaddingBottom=0\nPaddingLeft=0\nPaddingRight=0\n"
                % (rgbs(ink), rgbs(mix(ink, Bi, .35)), "false" if lt else "true", rgbs(mix(B, K, .6)), rgbs(mix(Bi, K, .3)),
                   S, S, S, max(0, T - 22) // 2, max(0, T - 22) // 2, S + 2, S + 2, min(22, T), bs, bs))
    with open(os.path.join(base, "metadata.desktop"), "w") as f:
        f.write("[Desktop Entry]\nName=Win12 XP %s\nComment=Windows XP window decoration for the Win12 XP menu\n"
                "X-KDE-PluginInfo-Author=Win12\nX-KDE-PluginInfo-Name=%s\nX-KDE-PluginInfo-Version=1.0\n"
                "X-KDE-PluginInfo-License=LGPL\n" % (label, name))

# T26b window colours: a Plasma colour scheme (~/.local/share/color-schemes/<name>.colors) per XP scheme, same names.
# XP: beige windows, white views, selection in the scheme colour (blue Luna #316ac5), title bar = scheme colour.
SCHEME_COLOURS = {"blue": ("#ece9d8", "#316ac5"), "olive": ("#ece9d8", "#93a070"), "silver": ("#e0dfe3", "#b2b4bf")}

def write_scheme(path, B, win, sel, label, view=W):
    """T37: also the Windows 10/11/7 schemes; a dark `win` gives white text, light links, dark tooltips."""
    c = lambda h: "%d,%d,%d" % rgb(h)
    lt = light(win)
    fg, selfg = (K if lt else W), (K if light(sel) else W)
    def group(bg, alt, f=fg, focus=sel):
        return "\n".join(["BackgroundNormal=" + c(bg), "BackgroundAlternate=" + c(alt), "ForegroundNormal=" + c(f),
                          "ForegroundInactive=" + c(mix(f, bg, .45)), "ForegroundActive=" + c(f), "ForegroundLink=" + c("#0000ff" if lt else "#4cc2ff"),
                          "ForegroundVisited=" + c("#800080" if lt else "#c79bff"), "ForegroundNegative=" + c("#c00000"), "ForegroundNeutral=" + c("#b06000"),
                          "ForegroundPositive=" + c("#007000"), "DecorationFocus=" + c(focus), "DecorationHover=" + c(mix(focus, W, .3))])
    g = {"Window": group(win, mix(win, K, .04)), "View": group(view, mix(win, view, .5)), "Button": group(win, mix(win, K, .06)),
         "Header": group(win, mix(win, K, .04)), "Tooltip": group("#ffffe1", "#f5f5d0") if lt else group("#2b2b2b", "#323232"),
         "Selection": group(sel, mix(sel, W, .2), selfg, sel),
         "Complementary": group(B, mix(B, K, .12), "#1f2440" if light(B) else W, mix(B, W, .4))}
    tfg, Bi = ("#000000" if light(B) else W), mix(B, "#d4d8e4", .55)
    name = os.path.splitext(os.path.basename(path))[0]
    with open(path, "w") as f:
        f.write("".join("[Colors:%s]\n%s\n\n" % kv for kv in g.items()))
        f.write("[General]\nColorScheme=%s\nName=Win12 %s\n\n[KDE]\ncontrast=4\n\n" % (name, label))
        f.write("[WM]\nactiveBackground=%s\nactiveBlend=%s\nactiveForeground=%s\ninactiveBackground=%s\ninactiveBlend=%s\n"
                "inactiveForeground=%s\n" % (c(B), c(B), c(tfg), c(Bi), c(Bi), c(mix(tfg, Bi, .3))))

# T37 the other menu styles: Windows 10 (flat, square, wide buttons), Windows 11 (rounded top, small rounded buttons;
# modern/app/pro/launcher use it too), Windows 7 (Aero glass blue, red close). 10/11 come light and dark (main.qml
# picks by the user's own scheme brightness). Names = Aurorae theme dir = colour scheme file.
WIN_DECOS = {"win12-win10-light": ("win10", False), "win12-win10-dark": ("win10", True),
             "win12-win11-light": ("win11", False), "win12-win11-dark": ("win11", True), "win12-win7": ("win7", False)}
WIN_SCHEMES = {"win10": dict(B=W, win="#f0f0f0", sel="#0078d7"), "win10-dark": dict(B="#202020", win="#202020", sel="#0078d7", view="#191919"),
               "win11": dict(B="#f3f3f3", win="#f3f3f3", sel="#005fb8"), "win11-dark": dict(B="#202020", win="#202020", sel="#60cdff", view="#2b2b2b"),
               "win7": dict(B="#6f97c8", win="#f0f0f0", sel="#3399ff")}

FLAT_GLYPHS = {"close": "M .1,.1 L .9,.9 M .9,.1 L .1,.9", "minimize": "M .05,.5 H .95", "maximize": "M .05,.05 H .95 V .95 H .05 Z",
               "restore": "M .25,.05 H .95 V .75 M .05,.25 H .75 V .95 H .05 Z"}  # thin Windows 10/11 caption glyphs

def write_win_deco(base, kind, dark, accent=None):
    """T38/T39 accent: "accent colour on title bars and window borders", the active title bar and frame in that colour."""
    name = os.path.basename(os.path.normpath(base))
    sch = WIN_SCHEMES[kind + ("-dark" if dark else "")]
    if kind == "win7":  # glass: translucent blue gradient, thick border, glassy buttons, wide red close
        T, S, ry, border, bw, bh, c, cw, gs = 28, 6, 6, 6, 26, 18, 3, 43, 8
        g = lambda a, o=.92: [(0, mix(a, W, .45), o), (.5, a, o), (1, mix(a, K, .1), o)]
        act, ina = g("#7ea6d6"), g("#c3d3e6")
        side, sidei, line, linei, hi = "#7ea6d6", "#c3d3e6", "#1f3d66", "#7d8ea3", "#eaf3fc"
        ink, inki, glyph = K, "#55606e", W
        if accent:  # T39 glass tinted by the accent
            act, side, hi = g(mix(accent, W, .25)), mix(accent, W, .25), mix(accent, W, .85)
    else:
        T, S, ry, border, bw, bh, c, gs = (30, 1, 0, 1, 46, 30, 0, 10) if kind == "win10" else (32, 8, 8, 1, 36, 26, 4, 10)
        cw = bw
        bg = sch["B"]
        act = ina = [(0, bg)]
        side = sidei = bg
        line = "#3a3a3a" if dark else ("#1883d7" if kind == "win10" else "#c4c4c4")
        linei = "#3a3a3a" if dark else "#aaaaaa"
        hi, ink = bg, (W if dark else K)
        inki, glyph = mix(ink, bg, .55), ink
        if accent:
            act, side, line, hi = [(0, accent)], accent, accent, accent
            ink = glyph = K if light(accent) else W
    glyphi = glyph if not accent else (W if dark else K)
    if OPT["title"]:  # T41: own title height; Win10 buttons fill it, the others stay centred / on top
        T = OPT["title"]
        bh = T if kind == "win10" else min(bh, T - 4)
    defs_a = deco_frame("decoration", 10, T, S, ry, act, side, line, hi, "da")
    defs_i = deco_frame("decoration-inactive", T + 70, T, S, ry, ina, sidei, linei, hi, "di")
    svg(os.path.join(base, "decoration.svg"), [defs_a, defs_i], size=(130, 2 * T + 140))
    states = ("active", "hover", "pressed", "deactivated", "inactive", "hover-inactive", "pressed-inactive", "deactivated-inactive")
    for btn in BUTTONS:
        w, frames = cw if btn == "close" else bw, []
        for i, st in enumerate(states):
            mode, on = st.split("-")[0], not st.endswith("inactive")
            hot = mode in ("hover", "pressed")
            if kind == "win7":
                base_c = ("#e0866f" if on else "#d9a79a") if btn == "close" else ("#9cbde3" if on else "#d0dbe8")
                top = mix(base_c, W, .5 if mode == "hover" else .15 if mode == "pressed" else .35)
                bot = mix(base_c, K, .25 if mode == "pressed" else .05)
                if btn == "close" and mode == "hover":
                    top, bot = "#f6b19c", "#e0401f"  # red glow
                stops, bd, gop = [(0, top, 1), (.45, bot, 1), (1, mix(bot, W, .3), 1)], ("#1f3d66", .8 if on else .4), 1 if on else .7
            else:
                red = "#e81123" if kind == "win10" else "#c42b1c"
                bar = accent if accent and on else sch["B"]
                grey = mix(bar, K if light(bar) else W, .12 if mode == "hover" else .2)
                col = red if btn == "close" and hot else grey
                stops, bd = [(0, col, (1 if btn == "close" else .9) if hot else 0)], None
                if btn == "close" and mode == "pressed":
                    stops = [(0, mix(red, K, .15), 1)]
                gop = 1 if on else .45
            if st.startswith("deactivated"):
                gop = .35
            gl = W if (kind == "win7" or (btn == "close" and hot)) else glyph if on else glyphi
            sw = 2.0 if kind == "win7" else 1.0
            frames.append(frame(st, 10, 10 + i * (bh + 20), w, bh, c, bd, stops, gid="b%d" % i, margins=(c, c, c, c),
                                center='<path d="%s" transform="translate(%s,%s) scale(%s)" fill="none" stroke="%s" stroke-opacity="%s" '
                                       'stroke-width="%s" stroke-linecap="round" stroke-linejoin="round"/>' % (
                                           FLAT_GLYPHS.get(btn, GLYPHS[btn]) if kind != "win7" else GLYPHS[btn], 10 + (w - gs) / 2, 10 + i * (bh + 20) + (bh - gs) / 2, gs, gl, gop, round(sw / gs, 3))))
        svg(os.path.join(base, btn + ".svg"), frames, size=(w + 20, 8 * (bh + 20) + 20))
    rgbs = lambda h: "%d,%d,%d" % rgb(h)
    with open(os.path.join(base, name + "rc"), "w") as f:
        f.write("[General]\nActiveTextColor=%s\nInactiveTextColor=%s\nUseTextShadow=false\nTitleAlignment=Left\n"
                "TitleVerticalAlignment=Center\nAnimation=0\nShadow=true\n\n[Layout]\nBorderLeft=%d\nBorderRight=%d\n"
                "BorderBottom=%d\nTitleEdgeTop=0\nTitleEdgeBottom=0\nTitleEdgeLeft=%d\nTitleEdgeRight=%d\nTitleBorderLeft=6\n"
                "TitleBorderRight=6\nTitleHeight=%d\nButtonWidth=%d\nButtonWidthClose=%d\nButtonHeight=%d\nButtonSpacing=%d\n"
                "ButtonMarginTop=%d\nExplicitButtonSpacer=8\nPaddingTop=0\nPaddingBottom=0\nPaddingLeft=0\nPaddingRight=0\n"
                % (rgbs(ink), rgbs(inki),
                   border, border, border, border + 6, border, T, bw, cw, bh, 0 if kind != "win11" else 2,
                   (T - bh) // 2 if kind == "win11" else 0))
    with open(os.path.join(base, "metadata.desktop"), "w") as f:
        f.write("[Desktop Entry]\nName=Win12 %s\nComment=Window decoration for the Win12 menu styles\n"
                "X-KDE-PluginInfo-Author=Win12\nX-KDE-PluginInfo-Name=%s\nX-KDE-PluginInfo-Version=1.0\n"
                "X-KDE-PluginInfo-License=LGPL\n" % (name[8:].replace("-", " ").title(), name))

# T41 window glass: any shipped/accent/custom deco by name, with OPT radius/title, then translucency + blur mask.
PIECES = "topleft|topright|top|left|right|bottomleft|bottomright|bottom|center"

def deco_for_name(out, name):
    m = re.fullmatch(r"win12-xp-(classic|modern|luna)(?:-(olive|silver))?(?:-custom-([0-9a-fA-F]{6}))?", name)
    if m:
        sub, sch, hexc = m.groups()
        return write_deco(out, "#" + hexc if hexc else DECO_BASES[sch or "blue"], sub, "%s %s" % (sub.title(), hexc or (sch or "blue").title()))
    m = re.fullmatch(r"win12-(win10|win11|win7)(?:-(light|dark))?(?:-accent-([0-9a-fA-F]{6}))?", name)
    if not m:
        raise SystemExit("unknown deco name: " + name)
    kind, mode, hexc = m.groups()
    os.makedirs(out, exist_ok=True)
    write_win_deco(out, kind, mode == "dark" and kind != "win7", "#" + hexc if hexc else None)

def glassify(path, alpha, blur):
    """Whole frame (decoration-* and decoration-inactive-*) at alpha; title text and buttons are other files and
    stay opaque. blur: Aurorae only asks KWin to blur behind mask-* pieces, so add them (active frame shape, black)."""
    s = open(path).read()
    s = re.sub(r'stop-opacity="([0-9.]+)"', lambda m: 'stop-opacity="%.3g"' % (float(m.group(1)) * alpha), s)
    s = re.sub(r'<g id="decoration[^"]*">.*?</g>', lambda m: re.sub(r'fill="(#[0-9a-fA-F]{6})"', r'fill="\1" fill-opacity="%.3g"' % alpha, m.group(0)), s)
    if blur:
        mask = "".join('<g id="mask-%s">%s</g>' % (k, re.sub(r'fill="(?!none)[^"]*"', 'fill="#000000"', b))
                       for k, b in re.findall(r'<g id="decoration-(%s)">(.*?)</g>' % PIECES, s))
        s = s.replace("\n</svg>", "\n" + mask + "\n</svg>")
    open(path, "w").write(s)

def imagify(path, image, mode="stretch", alpha=1.0, tint=""):
    """W14: picture over the title bar fill (decoration-[inactive-]topleft/top/topright shapes filled with the da/di
    gradient), as a userSpace <pattern> so rounded corners clip it; alpha = glass alpha x image opacity.
    stretch: covers the frame's title row (hint-stretch-borders, Aurorae stretches the top piece); tile: one copy
    per top piece (Aurorae tiles borders without the hint)."""
    s = open(path).read()
    data = open(image, "rb").read()
    imgsize(data)  # rejects anything that is not a PNG/JPEG
    mime = "png" if data[:4] == b"\x89PNG" else "jpeg"
    href = "data:image/%s;base64,%s" % (mime, base64.b64encode(data).decode())
    defs = ""
    for st, gid in (("", "da"), ("inactive-", "di")):
        y1, y2 = map(float, re.search(r'id="%s"[^>]*y1="([0-9.]+)"[^>]*y2="([0-9.]+)"' % gid, s).groups())
        box = lambda k: re.search(r'<g id="decoration-%s%s"><rect x="([0-9.]+)" y="[0-9.]+" width="([0-9.]+)"' % (st, k), s).groups()
        tl, tr, top = box("topleft"), box("topright"), box("top")
        x, w = (float(top[0]), float(top[1])) if mode == "tile" else (float(tl[0]), float(tr[0]) + float(tr[1]) - float(tl[0]))
        defs += ('<pattern id="i%s" patternUnits="userSpaceOnUse" x="%g" y="%g" width="%g" height="%g"><image x="0" y="0" width="%g" '
                 'height="%g" preserveAspectRatio="xMidYMid slice" xlink:href="%s"/>%s</pattern>') % (
            gid, x, y1, w, y2 - y1, w, y2 - y1, href,
            '<rect x="0" y="0" width="%g" height="%g" fill="#%s" comp-op="multiply"/>' % (w, y2 - y1, tint.lstrip("#")[-6:]) if tint else "")
        f = 'fill="url(#%s)"' % gid
        s = re.sub(r'<g id="decoration-%s(topleft|top|topright)">.*?</g>' % st, lambda m: re.sub(
            r'<(rect|path) [^>]*%s/>' % re.escape(f), lambda e: e.group(0) + e.group(0).replace(
                f, 'fill="url(#i%s)" fill-opacity="%.3g"' % (gid, alpha)), m.group(0)), s)
    s = s.replace("</defs>", defs + "</defs>", 1)
    if mode != "tile":
        s = s.replace("\n</svg>", '\n<rect id="hint-stretch-borders" x="0" y="0" width="1" height="1" fill="none"/>\n</svg>')
    open(path, "w").write(s)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir", nargs="?")
    ap.add_argument("--custom")
    ap.add_argument("--sub", choices=("classic", "modern", "luna"), default="classic")
    ap.add_argument("--out")
    ap.add_argument("--taskbar", nargs="*", metavar="key=value")
    ap.add_argument("--deco", action="store_true", help="write Aurorae window decorations instead (T26a)")
    ap.add_argument("--scheme", action="store_true", help="write Plasma colour schemes instead (T26b)")
    ap.add_argument("--win10", choices=("light", "dark"), help="with --deco/--scheme --custom: Win10/11/7 accent title bar (T38)")
    ap.add_argument("--kind", choices=("win10", "win11", "win7"), default="win10", help="with --win10: which look (T39)")
    ap.add_argument("--name", help="with --deco: make this deco (any win12-* name) into --out, with the T41 options")
    ap.add_argument("--opacity", type=int, default=100, help="T41: frame opacity in percent")
    ap.add_argument("--blur", type=int, default=1, help="T41: blur behind a translucent frame (1/0)")
    ap.add_argument("--radius", type=int, default=-1, help="T41: top corner radius in px (-1 = the style's)")
    ap.add_argument("--title", type=int, default=0, help="T41: title bar height in px (0 = the style's)")
    ap.add_argument("--image", help="W14: with --deco --name: PNG/JPEG over the title bar")
    ap.add_argument("--imagemode", choices=("stretch", "tile"), default="stretch")
    ap.add_argument("--imagealpha", type=int, default=100, help="W14: image opacity in percent")
    ap.add_argument("--tint", default="", help="W14: hex colour multiplied over the image")
    a = ap.parse_args()
    if a.scheme:
        if a.custom and a.win10:  # T38: Win10 light/dark scheme with the accent title bar
            B = "#" + a.custom.lstrip("#")[-6:]
            rgb(B)
            k = a.kind + ("-dark" if a.win10 == "dark" and a.kind != "win7" else "")
            write_scheme(a.out, label="%s %s Accent" % (a.kind.title(), a.win10.title()), **dict(WIN_SCHEMES[k], B=B, sel=B))   # selection = the accent that names it (no feedback loop)
        elif a.custom:  # --out is the .colors file; custom: beige windows, selection and title bar in the custom colour
            rgb(a.custom)
            B = "#" + a.custom.lstrip("#")[-6:]
            write_scheme(a.out, B, SCHEME_COLOURS["blue"][0], B, "XP %s %s" % (a.sub.title(), a.custom))
        else:
            os.makedirs(a.outdir, exist_ok=True)
            for sub in ("classic", "modern", "luna"):
                for sch, (win, sel) in SCHEME_COLOURS.items():
                    write_scheme(os.path.join(a.outdir, "win12-xp-" + sub + ("" if sch == "blue" else "-" + sch) + ".colors"),
                                 DECO_BASES[sch], win, sel, "XP %s %s" % (sub.title(), sch.title()))
            for name, (kind, dark) in WIN_DECOS.items():
                write_scheme(os.path.join(a.outdir, name + ".colors"), label=name[8:].replace("-", " ").title(), **WIN_SCHEMES[kind + ("-dark" if dark else "")])
    elif a.deco:
        OPT.update(radius=min(a.radius, 24), title=max(0, min(a.title, 64)) and max(16, a.title))
        if a.name:
            deco_for_name(a.out, a.name)
            if a.opacity < 100:
                glassify(os.path.join(a.out, "decoration.svg"), max(10, a.opacity) / 100, a.blur)
            if a.image:
                imagify(os.path.join(a.out, "decoration.svg"), a.image, a.imagemode,
                        min(100, max(10, a.opacity)) / 100 * max(0, min(100, a.imagealpha)) / 100, a.tint)
        elif a.custom and a.win10:
            B = "#" + a.custom.lstrip("#")[-6:]
            rgb(B)
            os.makedirs(a.out, exist_ok=True)
            write_win_deco(a.out, a.kind, a.win10 == "dark" and a.kind != "win7", B)
        elif a.custom:
            rgb(a.custom)
            write_deco(a.out, "#" + a.custom.lstrip("#")[-6:], a.sub, "%s %s" % (a.sub.title(), a.custom))
        else:
            for sub in ("classic", "modern", "luna"):
                for sch, B in DECO_BASES.items():
                    write_deco(os.path.join(a.outdir, "win12-xp-" + sub + ("" if sch == "blue" else "-" + sch)), B, sub,
                               "%s %s" % (sub.title(), sch.title()))
            for name, (kind, dark) in WIN_DECOS.items():
                write_win_deco(os.path.join(a.outdir, name), kind, dark)
    elif a.taskbar is not None:
        taskbar(a.out, **dict(kv.split("=", 1) for kv in a.taskbar))
    elif a.custom:
        rgb(a.custom)  # rejects anything that is not a hex colour
        write(a.out, derive("#" + a.custom.lstrip("#")[-6:], a.sub), "%s %s" % (a.sub.title(), a.custom))
    else:
        for sch in LUNA:
            write(os.path.join(a.outdir, "win12-xp-luna" + ("" if sch == "blue" else "-" + sch)), luna(sch), "Luna " + sch.title())
        for sub in ("classic", "modern"):
            write(os.path.join(a.outdir, "win12-xp-" + sub), BLUE[sub], sub.title())
            for sch, B in BASES.items():
                write(os.path.join(a.outdir, "win12-xp-%s-%s" % (sub, sch)), derive(B, sub), "%s %s" % (sub.title(), sch.title()))
