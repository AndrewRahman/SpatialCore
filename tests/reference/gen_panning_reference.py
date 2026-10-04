#!/usr/bin/env python3
"""Textbook reference values for the panning-law tests (D-13, D-14, D-16).  Offline tool, NOT run in CI.

  VBAP  Pulkki 1997 / Pernaux-Boussard-Jot DAFx-98 s2.2.1 :  g = L^-1 p, then g / ||g||_2
  VBIP  Pernaux-Boussard-Jot DAFx-98 s2.2.2 (single band)  :  g_i = sqrt(G_i / sum G_i), G = L^-1 p
                                                             == sqrt(VBAP gains) renormalised to sum g^2 = 1
  DBAP  Lossius et al. ICMC 2009 eq (2)-(5)                :  v_i = k / d_i^a, a = R/(20 log10 2), k = 1/sqrt(sum d_i^-2a)
        SpatialCore effective R = 12.04 dB (a = 2), spatial blur r_s = 0, d^2 clamped >= 0.001, speaker radius 1.
  MDAP  Pulkki, WASPAA 1999: NOT a closed-form law.  The value is a PORT of the code's construction
        (main + 8-point ring at alpha = clamp(0.9*180/N, 5, 30) deg, amplitude-summed VBAP, L2-normalised) -- it is a
        cross-check of the ring geometry, not an independent oracle.  Depends on ring start phase (~1% of gain).
Conventions match SpeakerLayout.cpp: +azimuth = left, x = cos(el) sin(az), y = cos(el) cos(az), z = sin(el).
Speaker positions are parsed from layoutDefs in src/IO/SpeakerLayout.cpp (layouts_from_cpp.load()).

Prints a complete C++ header on stdout (redirect into tests/reference/PanningReference.h); diagnostics go to
stderr.  Exits non-zero, printing nothing on stdout, if a self-check fails.
"""
import itertools
import sys

import numpy as np

from layouts_from_cpp import load


def check(cond, msg):
    if not cond:
        sys.exit(f"gen_panning_reference.py: SELF-CHECK FAILED: {msg}")


def vec(az, el=0.0):
    az, el = np.radians(az), np.radians(el)
    return np.array([np.cos(el) * np.sin(az), np.cos(el) * np.cos(az), np.sin(el)])


def vbap_2d(spk, az):
    """Adjacent-pair VBAP; returns gains in `spk` order."""
    order = np.argsort(spk)
    for a in range(len(spk)):
        i, j = order[a], order[(a + 1) % len(spk)]
        L = np.array([vec(spk[i])[:2], vec(spk[j])[:2]])
        g = np.linalg.solve(L.T, vec(az)[:2])
        if (g >= -1e-12).all():
            out = np.zeros(len(spk)); out[i], out[j] = g
            return out / np.linalg.norm(out)
    raise RuntimeError(f"no enclosing pair for az {az} on {spk}")


def vbip_2d(spk, az):
    h = np.sqrt(vbap_2d(spk, az)); return h / np.linalg.norm(h)


def vbap_3d_triplet(spk_az_el, tri, az, el):
    """Textbook VBAP for a GIVEN triplet of (az, el) speakers; returns full speaker-order gains."""
    L = np.array([vec(*spk_az_el[k]) for k in tri]).T
    g = np.linalg.solve(L, vec(az, el))
    check((g >= 0).all(), f"source ({az},{el}) is not inside triplet {tri}")
    out = np.zeros(len(spk_az_el)); out[list(tri)] = g / np.linalg.norm(g)
    return out


def min_sum_triplet_margin(spk_az_el, tri, az, el):
    """Mirror of computeVBAPGains3D's selection over EVERY speaker triple with |det| >= 0.01: among the
    enclosing triples (all raw gains >= -1e-6) the minimum raw-gain-sum one wins.  Returns the margin by
    which `tri` beats the runner-up; fails if `tri` is not the winner."""
    p = vec(az, el)
    cands = []
    for t in itertools.combinations(range(len(spk_az_el)), 3):
        L = np.array([vec(*spk_az_el[k]) for k in t]).T
        if abs(np.linalg.det(L)) < 0.01:
            continue
        g = np.linalg.solve(L, p)
        if (g >= -1e-6).all():
            cands.append((float(g.sum()), t))
    cands.sort()
    check(len(cands) >= 1 and cands[0][1] == tuple(sorted(tri)),
          f"7.1.4 ({az},{el}): named triplet {tri} is not the minimum-sum enclosing triple (winner {cands[:1]})")
    margin = cands[1][0] - cands[0][0] if len(cands) > 1 else float("inf")
    check(margin > 1e-4, f"7.1.4 ({az},{el}): triplet {tri} wins by only {margin:.3e} (coplanar tie, RESEARCH F5)")
    return margin


def dbap(spk, az, el=0.0, dist=1.0, eps=1e-3):
    s = dist * vec(az, el)
    w = np.array([1.0 / max(eps, np.sum((s - vec(a)) ** 2)) for a in spk])   # d^-2 used as AMPLITUDE (a = 2)
    return w / np.linalg.norm(w)


def mdap_2d(spk, az):
    N = len(spk); alpha = np.radians(np.clip(0.9 * 180 / N, 5, 30))
    p = vec(az); A = np.array([p[1], -p[0], 0.0]); A /= np.linalg.norm(A)
    tot = vbap_2d(spk, az).copy()
    for i in range(8):
        phi = 2 * np.pi * i / 8
        rot = A * np.cos(phi) + np.cross(p, A) * np.sin(phi) + p * (p @ A) * (1 - np.cos(phi))
        q = p * np.cos(alpha) + np.cross(rot, p) * np.sin(alpha) + rot * (rot @ p) * (1 - np.cos(alpha))
        tot += vbap_2d(spk, np.degrees(np.arctan2(q[0], q[1])))
    return tot / np.linalg.norm(tot)


def fmt(v, decimals=7):
    v = round(float(v), decimals) + 0.0      # + 0.0 turns -0.0 into 0.0
    return f"{v:.{decimals}f}f"


def arr(name, values, comment=None):
    out = [f"// {comment}"] if comment else []
    out.append(f"inline constexpr float {name}[{len(values)}] = {{ " + ", ".join(fmt(v) for v in values) + " };")
    return out


def check_power(name, g):
    check(abs(float(np.dot(g, g)) - 1.0) < 1e-12, f"{name}: power sum g^2 = {float(np.dot(g, g))!r}, not 1")


def check_vbip_energy_vector(name, spk, az, g):
    rE = sum(gi * gi * vec(a) for gi, a in zip(g, spk))
    err = abs(np.angle(np.exp(1j * (np.arctan2(rE[0], rE[1]) - np.radians(az)))))
    check(err < 1e-9, f"{name}: VBIP energy vector misses the source azimuth by {err:.3e} rad")


def main():
    L = load()
    quad = [a for a, _e, _c in L["Quad"]["spk"]]
    l50 = [a for a, _e, _c in L["S5_0"]["spk"]]
    s714 = [(a, e) for a, e, _c in L["S7_1_4"]["spk"]]
    check(quad == [45, -45, 135, -135], f"Quad order changed: {quad}")
    check(l50 == [30, -30, 0, 110, -110], f"5.0 order changed: {l50}")
    check(len(s714) == 11, f"7.1.4 has {len(s714)} speakers, expected 11")

    vb = {}
    for tag, spk, az in (("Quad_az30", quad, 30), ("S5_0_az10", l50, 10), ("S5_0_az50", l50, 50)):
        g_vbap, g_vbip = vbap_2d(spk, az), vbip_2d(spk, az)
        check_power(f"VBAP {tag}", g_vbap)
        check_power(f"VBIP {tag}", g_vbip)
        check_vbip_energy_vector(f"VBIP {tag}", spk, az, g_vbip)
        vb[tag] = (g_vbap, g_vbip)

    dbap1 = dbap(quad, 30, dist=1.0)
    dbap05 = dbap(quad, 30, dist=0.5)
    golden05 = [0.9390509, 0.2691336, 0.1768006, 0.1203831]   # existing code golden (RESEARCH F2)
    dev = max(abs(a - b) for a, b in zip(dbap05, golden05))
    check(dev < 1e-6, f"DBAP Quad az30 dist0.5 misses the existing code golden by {dev:.3e}")

    mdap_quad = mdap_2d(quad, 30)
    mdap_50 = mdap_2d(l50, 10)

    pins = ((0, 30, (2, 7, 8)), (60, 20, (0, 3, 7)), (10, 10, (0, 2, 7)), (90, 30, (3, 7, 9)))
    cases3d, margins = [], []
    for az, el, tri in pins:
        margins.append(min_sum_triplet_margin(s714, tri, az, el))
        g = vbap_3d_triplet(s714, tri, az, el)
        check_power(f"VBAP 7.1.4 ({az},{el})", g)
        cases3d.append(f"    {{ {float(az):.1f}f, {float(el):.1f}f, {{ " + ", ".join(fmt(v) for v in g) + " } },"
                       f"  // triplet {tri[0]}, {tri[1]}, {tri[2]}")

    lines = [
        "#pragma once",
        "",
        "// Textbook panning-law reference values (D-13, D-14, D-16) in layoutDefs speaker order.",
        "// Generated by tests/reference/gen_panning_reference.py",
        f"//   numpy {np.__version__}",
        "// Generated — do not edit; regenerate per tests/reference/README.md",
        "//",
        "// Oracles (published formulas, independent of the code under test):",
        "//   VBAP  Pulkki 1997 / Pernaux-Boussard-Jot DAFx-98 s2.2.1:  g = L^-1 p, then g / ||g||_2",
        "//   VBIP  Pernaux-Boussard-Jot DAFx-98 s2.2.2 (single band): g_i = sqrt(G_i / sum G), G = L^-1 p",
        "//   DBAP  Lossius et al. ICMC 2009 eq (2)-(5): v_i = k / d_i^a, a = 2 (R = 12.04 dB), r_s = 0,",
        "//         d^2 clamped >= 0.001, speaker radius 1, source at distance * unit direction",
        "// Speaker positions are parsed from layoutDefs in src/IO/SpeakerLayout.cpp.",
        "// Self-checks: VBAP/VBIP power == 1 (1e-12); VBIP energy vector points at the source (1e-9 rad);",
        "// DBAP dist 0.5 reproduces the pre-existing code golden (1e-6); each 7.1.4 pin's triplet is the",
        "// unique minimum-sum enclosing triple over all |det| >= 0.01 triples by > 1e-4 (no coplanar tie).",
        "//",
        "// Speaker order -- Quad: 45, -45, 135, -135 | 5.0: 30, -30, 0, 110, -110 |",
        "// 7.1.4 (az/el): " + ", ".join(f"{a}/{e}" for a, e in s714),
        "",
        "namespace spatialcore_ref",
        "{",
        "",
    ]
    lines += arr("kVbap_Quad_az30", vb["Quad_az30"][0], "VBAP, Quad, az 30 el 0")
    lines += arr("kVbip_Quad_az30", vb["Quad_az30"][1], "VBIP, Quad, az 30 el 0")
    lines += arr("kVbap_S5_0_az10", vb["S5_0_az10"][0], "VBAP, 5.0, az 10 el 0")
    lines += arr("kVbip_S5_0_az10", vb["S5_0_az10"][1], "VBIP, 5.0, az 10 el 0")
    lines += arr("kVbap_S5_0_az50", vb["S5_0_az50"][0], "VBAP, 5.0, az 50 el 0")
    lines += arr("kVbip_S5_0_az50", vb["S5_0_az50"][1], "VBIP, 5.0, az 50 el 0")
    lines.append("")
    lines += arr("kDbap_Quad_az30_dist1", dbap1, "DBAP, Quad, az 30 el 0, distance 1.0")
    lines += arr("kDbap_Quad_az30_dist05", dbap05, "DBAP, Quad, az 30 el 0, distance 0.5 (equals the pre-existing code golden, F2)")
    lines += [
        "",
        "// MDAP: a PORT of the code's ring construction (main direction + 8 aux points on a ring at",
        "// alpha = clamp(0.9 * 180 / N, 5, 30) degrees, amplitude-summed 2D VBAP, L2-normalised). This is a",
        "// cross-check of the ring geometry, not an independent oracle (RESEARCH F3); it depends on the",
        "// ring start phase (~1% of gain). Compare at tolerance 5e-4.",
    ]
    lines += arr("kMdapPort_Quad_az30", mdap_quad)
    lines += arr("kMdapPort_S5_0_az10", mdap_50)
    lines += [
        "",
        "// 3D VBAP on 7.1.4: each pin lies inside a single minimum-sum triplet (no coplanar tie, RESEARCH F5).",
        "struct Vbap3DCase { float azimuthDeg; float elevationDeg; float gains[11]; };",
        "",
        f"inline constexpr Vbap3DCase kVbap3D_S7_1_4[{len(cases3d)}] = {{",
    ] + cases3d + ["};", "", "} // namespace spatialcore_ref"]

    print("\n".join(lines))
    print("gen_panning_reference.py: all self-checks passed; 7.1.4 triplet margins "
          + ", ".join(f"{m:.4f}" for m in margins), file=sys.stderr)


if __name__ == "__main__":
    main()
