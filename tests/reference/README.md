# tests/reference — offline reference oracles

This directory holds the generators for every Phase 2 reference value, plus the C++ headers they
print. Each generator uses an oracle that is **independent of the code under test**:

| Generator | Oracle | Header it prints |
|---|---|---|
| `gen_sh_reference.py` | scipy `lpmv` (Condon-Shortley phase removed), cross-checked against scipy `sph_harm_y` | `ShReference.h` |
| `gen_ear_reference.py` | EBU ADM Renderer, PyPI `ear` 2.1.0 (ITU-R BS.2127 reference implementation) | `EarReference.h` |
| `gen_panning_reference.py` | Published formulas: VBAP (Pulkki 1997), VBIP (Pernaux-Boussard-Jot DAFx-98), DBAP (Lossius et al. ICMC 2009) | `PanningReference.h` |
| `layouts_from_cpp.py` | — (helper) parses the `layoutDefs` table in `src/IO/SpeakerLayout.cpp`, so the oracles use the real speaker positions | — |

**Offline only (D-11c).** Nothing in this directory runs in CI, and no CI workflow refers to it
(P1 D-10, P1 D-13). The C++ tests `#include` the checked-in headers. Python is needed only to
regenerate them.

## Install (dev machine only)

Tested on Python 3.14.7, macOS arm64. The venv goes in `.context/venv`, which is gitignored.

A plain `pip install ear` **fails on Python 3.14**: ear 2.1.0 pins `lxml~=4.4`, which has no wheel
for 3.14 and does not build from source. Install in this order: a binary-only lxml, then ear with
`--no-deps`, then the remaining pins:

```bash
python3 -m venv .context/venv                       # .context/ is gitignored
.context/venv/bin/pip install --only-binary=:all: lxml==6.1.3
.context/venv/bin/pip install --no-deps ear==2.1.0
.context/venv/bin/pip install numpy==2.5.3 scipy==1.18.1 attrs==26.1.0 multipledispatch==1.0.0 \
    six==1.17.0 PyYAML==6.0.3 ruamel.yaml==0.19.1 setuptools==80.10.2   # setuptools<81 keeps pkg_resources
```

Frozen versions (`pip freeze`):

```
attrs==26.1.0
ear==2.1.0
lxml==6.1.3
multipledispatch==1.0.0
numpy==2.5.3
PyYAML==6.0.3
ruamel.yaml==0.19.1
scipy==1.18.1
setuptools==80.10.2
six==1.17.0
```

pip prints "ear 2.1.0 requires numpy~=1.14 / lxml~=4.4 / attrs<22 / multipledispatch~=0.5" conflict
warnings. They are expected: ear 2.1.0 imports and runs correctly on these versions. Every
checked-in value was produced on exactly this set.

Package provenance (checked before first use, 2026-10-01): numpy is from github.com/numpy/numpy,
scipy from github.com/scipy/scipy, and ear from github.com/ebu/ebu_adm_renderer (author EBU,
BSD-3-Clause-Clear, 2.1.0 released 2022-01-26).

## Regenerate

Run these from the repo root:

```bash
.context/venv/bin/python tests/reference/gen_sh_reference.py      > tests/reference/ShReference.h
.context/venv/bin/python tests/reference/gen_ear_reference.py     > tests/reference/EarReference.h
.context/venv/bin/python tests/reference/gen_panning_reference.py > tests/reference/PanningReference.h
```

Each generator writes the whole header to stdout and its diagnostics to stderr. It exits non-zero
and prints nothing to stdout if a self-check fails:

- `gen_sh_reference.py`: the two scipy routes must agree to within 1e-12, and the SN3D addition
  theorem (sum over m of Y_lm² = 1) must hold to within 1e-12 for orders 0-6 on 4000 seeded directions.
- `gen_ear_reference.py`: every nadir-cap case must fall in ear's `VirtualNgon` region, and the
  meridian case (directly under the first ear-level speaker, el -15) must give exactly 1. Every
  band case must fall in a `QuadRegion`, equal ear's own horizon pan at the same azimuth (within
  1e-12), put no gain (below 1e-12) on any speaker above 10 degrees elevation, and have power 1;
  5.1.4 band cases must have |azimuth| at most 110. A carve-out check also requires ear to feed
  5.1.4's height speakers (more than 0.1) at (180, -10), so the documented rear-gap departure below
  cannot go stale.
- `gen_panning_reference.py`: VBAP and VBIP power must be 1, and the VBIP energy vector must point
  at the source. DBAP at distance 0.5 must reproduce the existing code golden to within 1e-6. Each
  7.1.4 3D pin must lie in a unique minimum-sum enclosing triplet, with no coplanar tie.

Before you commit, compile-check the headers: `c++ -std=c++17 -fsyntax-only -x c++ tests/reference/<Header>.h`.

## Oracle discipline

- **Never hand-edit a header.** If a value looks wrong, fix the generator (or the oracle's input)
  and regenerate.
- **Never derive a reference value from the code under test.** The oracle must be a second
  implementation, a published formula, or an external reference renderer.
- **Regenerate and diff** to confirm a header is current. The output is deterministic, so
  `diff <(.context/venv/bin/python tests/reference/gen_sh_reference.py) tests/reference/ShReference.h`
  prints nothing.

## Which header feeds which tests

| Header | Test tags |
|---|---|
| `ShReference.h` | `[sn3d][golden]` |
| `EarReference.h` | `[ear][golden]`, `[ear][golden][band]` |
| `PanningReference.h` | `[vbip]`, `[panning-law][textbook]` |

Include from a test file one level down: `#include "../reference/ShReference.h"`. All data lives in
`namespace spatialcore_ref`.

## Caveats

- **MDAP is a port, not an oracle.** `kMdapPort_*` in `PanningReference.h` port the code's own
  ring construction: 8 aux points at alpha = clamp(0.9 × 180 / N, 5, 30) degrees, amplitude-summed
  VBAP, L2-normalised. They cross-check the ring geometry, but they are **not an independent
  oracle**. MDAP has no closed-form textbook law, and the values depend on the ring's start phase
  by about 1% of gain (RESEARCH F3). Compare at tolerance 5e-4.
- **EAR band.** `EarReference.h` pins the nadir cap and, since Plan 02-08 (G-02-2), the -30..0
  degree band on all 8 height layouts. In the band ear uses a bilinear `QuadRegion`, which with its
  -30 degree copies downmixed onto their ear-level speakers equals the pair's horizon pan; SpatialCore
  pans the band with pair-pan wedges and matches it. The one deliberate difference is 5.1.4 behind
  the listener (|azimuth| above 110 degrees): there ear also feeds U+135/U-135, even at and above the
  horizon, while SpatialCore keeps the horizon pan (RESEARCH F4), so those directions are not pinned.
