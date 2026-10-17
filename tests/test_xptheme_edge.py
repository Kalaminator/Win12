#!/usr/bin/env python3
# Readable glass (spec 12.1): xptheme.py --taskbar edge=1 adds shadow-* tiles (floating/docked bar) or the painted
# ring shadow + two edge strokes (gap mode), in all 3 variants, never in the mask; without edge=1 none of it.
# Run: python3 tests/test_xptheme_edge.py
import os, subprocess, sys, tempfile, xml.etree.ElementTree as E

X = os.path.join(os.path.dirname(__file__), "..", "package/contents/ui/code/xptheme.py")
ARGS = "colour=3d8ee6 alpha=0.15 radius=12 line=0 buttons=win11 gap=8 fallback=default".split()

def gen(tmp, name, *extra):
    out = os.path.join(tmp, name)
    subprocess.run([sys.executable, X, "--taskbar", *ARGS, *extra, "--out", out], check=True)
    return out

def count(svg, gid=None):
    """(number of shadow-* ids, number of stroked shapes inside the group gid)"""
    t = E.parse(svg).getroot()
    ids = sum(1 for e in t.iter() if (e.get("id") or "").startswith("shadow-"))
    g = [e for e in t.iter() if e.get("id") == gid]
    return ids, sum(1 for e in g[0].iter() if e.get("stroke")) if g else 0

with tempfile.TemporaryDirectory() as tmp:
    ring = ("ring=8", "size=1200x44")
    cases = {gen(tmp, "plain"): (0, 0), gen(tmp, "ring", *ring): (0, 0),
             gen(tmp, "edge", "edge=1"): (12, 0), gen(tmp, "edge-ring", "edge=1", *ring): (0, 8),  # 6 shadow rings + 2 edges
             gen(tmp, "edge-r2", "edge=1", "radius=2"): (12, 0)}
    for out, want in cases.items():
        for v in ("", "translucent/", "opaque/"):
            p = os.path.join(out, v, "widgets/panel-background.svg")
            assert count(p, "center") == want, (p, count(p, "center"), want)
            assert count(p, "mask-center")[1] == 0, p
print("test_xptheme_edge OK")
