#!/usr/bin/env python3
"""Parse the speaker-layout table out of src/IO/SpeakerLayout.cpp.

The reference generators in this directory take speaker positions from here, so the oracles follow
the real `layoutDefs` table instead of a hand-typed copy of it.  Offline tool, NOT run in CI.

Run directly to print one line per layout:  name  numSpeakers  lfeIdx  totalChs  [(az, el), ...]
"""
import os
import re

import numpy as np

# LayoutID order -- must match the row order of layoutDefs[] in src/IO/SpeakerLayout.cpp.
NAMES = ["Quad", "S5_0", "S5_1", "S7_0", "S7_1", "S9_1", "S5_1_2", "S5_1_4", "S7_1_2", "S7_1_4",
         "S7_1_6", "S9_1_4", "S9_1_6", "Octaphonic", "SML13_1"]

# tests/reference -> tests -> repo root
REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SPEAKER_LAYOUT_CPP = os.path.join(REPO_ROOT, "src", "IO", "SpeakerLayout.cpp")


def load():
    """Returns {name: {"n", "lfe", "total", "spk": [(azDeg, elDeg, chIdx), ...]}} in LayoutID order."""
    with open(SPEAKER_LAYOUT_CPP, encoding="utf-8") as f:
        src = f.read()
    body = src[src.index("layoutDefs[NUM_LAYOUT_DEFS] = {"):src.index("static SpeakerLayout makeLayoutFromDef")]
    body = re.sub(r"//[^\n]*", "", body)
    out = []
    # each layout: { n, lfe, total, { {az,el,ch}, ... } }
    for m in re.finditer(r"\{\s*(\d+),\s*(-?\d+),\s*(\d+),\s*\{(.*?\}),?\s*\}\s*\}", body, flags=re.S):
        n, lfe, tot, spk = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        s = [tuple(int(x) for x in t) for t in re.findall(r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*\}", spk)]
        assert len(s) == n, (n, len(s))
        out.append(dict(n=n, lfe=lfe, total=tot, spk=s))
    assert len(out) == 15, len(out)
    return dict(zip(NAMES, out))


def vec(az, el):
    """SpeakerLayout.cpp convention: +az = left, x = cos(el) sin(az), y = cos(el) cos(az), z = sin(el)."""
    az, el = np.radians(az), np.radians(el)
    return np.array([np.cos(el) * np.sin(az), np.cos(el) * np.cos(az), np.sin(el)])


if __name__ == "__main__":
    for k, v in load().items():
        print(k, v["n"], v["lfe"], v["total"], [(a, e) for a, e, _c in v["spk"]])
