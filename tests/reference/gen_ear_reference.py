#!/usr/bin/env python3
"""Oracle gains for the D-04 lower-hemisphere construction, from the EBU ADM Renderer (PyPI `ear` 2.1.0,
the ITU-R BS.2127 reference implementation).

Two sets of directions are emitted as exact oracle values:
  * nadir-cap cases, which ear handles with its VirtualNgon region;
  * band cases (-30..0 degrees), which ear handles with a bilinear QuadRegion. With the -30 degree copies
    downmixed onto their ear-level speakers that region is exactly the pair's horizon pan, and since G-02-2
    (Plan 02-08) SpatialCore's pair-pan wedges match it, so these are pinned too.
5.1.4's rear gap (|azimuth| above 110 degrees) is NOT pinned: ear feeds U+135/U-135 there even at and above
the horizon, and SpatialCore deliberately keeps the horizon pan (RESEARCH F4). A self-check keeps that
documented departure honest. Also asserts the 'meridian' case (directly under an ear-level speaker) where
both agree exactly: gain 1 on that speaker.

Offline tool, NOT run in CI.  Layouts are parsed from src/IO/SpeakerLayout.cpp so the oracle follows the real
table.  Prints a complete C++ header on stdout (redirect into tests/reference/EarReference.h); diagnostics go
to stderr.  Exits non-zero, printing nothing on stdout, if a self-check fails.
"""
import sys
import warnings

warnings.filterwarnings("ignore")

from importlib.metadata import version as pkg_version

import numpy as np
from ear.core import point_source
from ear.core.geom import PolarPosition
from ear.core.layout import Channel, Layout
from ear.core.point_source import QuadRegion, VirtualNgon

from layouts_from_cpp import load


def check(cond, msg):
    if not cond:
        sys.exit(f"gen_ear_reference.py: SELF-CHECK FAILED: {msg}")


def ear_panner(spk):
    ch = [Channel(name=("T+000" if el == 90 else f"S{i:02d}"), polar_position=PolarPosition(az, el, 1.0))
          for i, (az, el, _c) in enumerate(spk)]
    return point_source.configure(Layout(name="custom", channels=ch))


def ear_gains_region(panner, az, el):
    """Returns (gains in layoutDefs speaker order, the ear region object that handled the direction).
    ear: +az = left, x = right (mirror of SpatialCore's x axis) -- gains are unaffected by the mirror."""
    a, e = np.radians(az), np.radians(el)
    pos = np.array([np.sin(-a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
    for r in panner.psp.regions:
        pv = r.handle_remap(pos, panner.psp.num_channels)
        if pv is not None:
            g = np.dot(panner.downmix, pv)
            return g / np.linalg.norm(g), r
    raise RuntimeError("ear found no region")


def ear_gains(panner, az, el):
    """Returns (gains in layoutDefs speaker order, handled_by_VirtualNgon)."""
    g, r = ear_gains_region(panner, az, el)
    return g, isinstance(r, VirtualNgon)


CASES = {
    "S5_1_2": [(30, -60), (180, -60), (-100, -75), (30, -90)],
    "S7_1_4": [(30, -60), (0, -45), (180, -70), (-100, -75), (30, -90)],
    "S9_1_6": [(45, -75), (-90, -60), (120, -50), (30, -90)],
}


# (azimuth, elevation) pairs in ear's below-horizon band (-30..0 degrees), one QuadRegion each (checked below).
# 5.1.4 has none with |azimuth| above 110: that rear gap is the documented departure from ear.
BAND_CASES = {
    "S5_1_2": [(60, -15), (-70, -25), (170, -40)],
    "S5_1_4": [(60, -15), (-70, -25), (-100, -20)],
    "S7_1_2": [(60, -10), (-110, -20), (160, -28)],
    "S7_1_4": [(60, -5), (60, -15), (60, -25), (-165, -35), (110, -20)],
    "S7_1_6": [(45, -12), (-60, -22), (170, -30)],
    "S9_1_4": [(45, -12), (-75, -18), (-160, -26)],
    "S9_1_6": [(45, -12), (-100, -24), (150, -8)],
    "SML13_1": [(20, -10), (-110, -25), (160, -20)],
}

ELEVATED_DEG = 10.0     # speakers above this elevation must get no gain in the band


def fmt(v, decimals):
    v = round(float(v), decimals) + 0.0      # + 0.0 turns -0.0 into 0.0
    return f"{v:.{decimals}f}f"


def main():
    L = load()
    body = []
    for name, pts in CASES.items():
        spk = L[name]["spk"]
        check(len(spk) <= 16, f"{name} has {len(spk)} speakers; EarCase holds 16")
        p = ear_panner(spk)
        order = ", ".join(f"{az}/{el}" for az, el, _c in spk)
        body.append(f"// {name}: {len(spk)} speakers, gains in layoutDefs speaker order (az/el deg): {order}")
        body.append(f"inline constexpr int kEarNumSpeakers_{name} = {len(spk)};")
        body.append(f"inline constexpr EarCase kEarCases_{name}[] = {{")
        for az, el in pts:
            g, ngon = ear_gains(p, az, el)
            check(ngon, f"{name} ({az},{el}) is not in ear's VirtualNgon (nadir-cap) region")
            check(abs(float(np.dot(g, g)) - 1.0) < 1e-12, f"{name} ({az},{el}) power is not 1")
            body.append(f"    {{ {float(az):.1f}f, {float(el):.1f}f, {{ " + ", ".join(fmt(v, 7) for v in g) + " } },")
        body.append("};")
        body.append("")
        g, _ = ear_gains(p, spk[0][0], -15.0)
        check(abs(g[0] - 1.0) < 1e-9, f"{name}: meridian case (az {spk[0][0]}, el -15) must be exactly 1 "
                                       f"on the speaker above, got {g[0]!r}")

    band = []
    band_total = 0
    band.append("// Below-horizon band (-30..0 deg) cases: ear's QuadRegion, equal to the pair's horizon pan, matched exactly")
    band.append("// since G-02-2. 5.1.4's rear gap (|azimuth| above 110 deg) is deliberately not pinned.")
    band.append("")
    for name, pts in BAND_CASES.items():
        spk = L[name]["spk"]
        check(len(spk) <= 16, f"{name} has {len(spk)} speakers; EarCase holds 16")
        p = ear_panner(spk)
        order = ", ".join(f"{az}/{el}" for az, el, _c in spk)
        band.append(f"// {name}: {len(spk)} speakers, gains in layoutDefs speaker order (az/el deg): {order}")
        band.append(f"inline constexpr int kEarBandNumSpeakers_{name} = {len(spk)};")
        band.append(f"inline constexpr EarCase kEarBandCases_{name}[] = {{")
        for az, el in pts:
            g, region = ear_gains_region(p, az, el)
            g0, _ = ear_gains_region(p, az, 0.0)
            check(isinstance(region, QuadRegion), f"{name} band ({az},{el}) is not in ear's QuadRegion")
            check(float(np.max(np.abs(g - g0))) < 1e-12,
                  f"{name} band ({az},{el}) differs from ear's horizon pan at the same azimuth")
            for (_saz, sel, _c), gv in zip(spk, g):
                if sel > ELEVATED_DEG:
                    check(abs(float(gv)) < 1e-12, f"{name} band ({az},{el}) puts gain on an elevated speaker")
            check(abs(float(np.dot(g, g)) - 1.0) < 1e-12, f"{name} band ({az},{el}) power is not 1")
            if name == "S5_1_4":
                check(abs(az) <= 110, f"S5_1_4 band ({az},{el}) is in the rear gap (|az| > 110), "
                                      f"where SpatialCore deliberately departs from ear")
            band.append(f"    {{ {float(az):.1f}f, {float(el):.1f}f, {{ " + ", ".join(fmt(v, 7) for v in g) + " } },")
            band_total += 1
        band.append("};")
        band.append("")

    # Keeps the documented 5.1.4 rear-gap departure honest: ear really does feed the height speakers there.
    spk514 = L["S5_1_4"]["spk"]
    g, _ = ear_gains_region(ear_panner(spk514), 180.0, -10.0)
    check(max(float(gv) for (_a, sel, _c), gv in zip(spk514, g) if sel > ELEVATED_DEG) > 0.1,
          "ear no longer feeds the 5.1.4 height speakers in the rear gap at (180, -10): "
          "the documented 5.1.4 departure from ear is out of date")

    lines = [
        "#pragma once",
        "",
        "// Oracle gains for the D-04 below-horizon (lower-hemisphere) construction on height layouts.",
        "// Generated by tests/reference/gen_ear_reference.py",
        f"//   numpy {np.__version__}, ear {pkg_version('ear')}",
        "// Generated — do not edit; regenerate per tests/reference/README.md",
        "//",
        "// Oracle: the EBU ADM Renderer (PyPI `ear` 2.1.0, github.com/ebu/ebu_adm_renderer), the reference",
        "// implementation of ITU-R BS.2127 -- independent of the code under test. Speaker positions are",
        "// parsed from layoutDefs in src/IO/SpeakerLayout.cpp.",
        "//",
        "// Two sets of cases, both matched exactly by SpatialCore's VBAP:",
        "//   kEarCases_*     -- nadir cap (below -30 deg), ear's VirtualNgon region;",
        "//   kEarBandCases_* -- the -30..0 deg band, ear's bilinear QuadRegion, which with the -30 deg copies",
        "//                      downmixed onto their ear-level speakers equals the pair's horizon pan. SpatialCore",
        "//                      pans it with pair-pan wedges since G-02-2 (Plan 02-08).",
        "// 5.1.4 behind the listener (|azimuth| above 110 deg) is the one deliberate difference: ear also feeds",
        "// U+135/U-135 there at and above the horizon, SpatialCore keeps the horizon pan, so those directions",
        "// are not pinned. Gains are L2-normalised; unused trailing slots are zero.",
        "",
        "namespace spatialcore_ref",
        "{",
        "",
        "struct EarCase { float azimuthDeg; float elevationDeg; float gains[16]; };",
        "",
    ] + body + band + ["} // namespace spatialcore_ref"]

    print("\n".join(lines))
    print(f"gen_ear_reference.py: {sum(len(v) for v in CASES.values())} VirtualNgon (nadir-cap) cases, "
          f"{band_total} QuadRegion band cases (equal to the horizon pan), {len(CASES)} meridian checks, "
          f"5.1.4 rear-gap carve-out check passed", file=sys.stderr)

if __name__ == "__main__":
    main()
