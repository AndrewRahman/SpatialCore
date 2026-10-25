# evalSH independent check (AmbiX SN3D, ACN 0..48)

Files (all in /tmp/shref/): ref.py (scipy lpmv reference + sanity), h.cpp (evalSH harness), cmp.py (compare),
out.txt (numeric result), grid.txt/sc.txt (raw values), d.cpp + dec.py + dec_out.txt (16-speaker ring order-6 decode check).
Build: clang++ -std=c++17 -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -I include -I src/Core -I build/_deps/juce-src/modules h.cpp src/Core/SpatialMath.cpp build/libSpatialCore.a <frameworks> -Wl,-dead_strip
Run: python3 ref.py; ./h > sc.txt; python3 cmp.py

Grid: az -180..180 step 5, el {-60,-30,0,30,60,85}, 6 x 73 points, 49 channels. Reference sanity passed first
(W,Y,Z,X, V/T/R/S/U closed forms, addition theorem l<=6 to 1e-12).

## Result: all 49 channels match, overall max abs error 1.1e-6 (float rounding); horizon back-quadrant max 9.7e-7. Zero channels > 1e-4.

## Per-ACN numbers
ACN  0 l=0 m=+0 maxerr 0.000e+00 at az=-180 el=-60 ref=+1.0000000 sc=+1.0000000
ACN  1 l=1 m=-1 maxerr 1.203e-07 at az=-170 el=0 ref=-0.1736482 sc=-0.1736483
ACN  2 l=1 m=+0 maxerr 4.422e-08 at az=-180 el=-60 ref=-0.8660254 sc=-0.8660254
ACN  3 l=1 m=+1 maxerr 6.826e-08 at az=-115 el=0 ref=-0.4226183 sc=-0.4226183
ACN  4 l=2 m=-2 maxerr 1.862e-07 at az=-175 el=0 ref=+0.1503837 sc=+0.1503835
ACN  5 l=2 m=-1 maxerr 1.183e-07 at az=-95 el=85 ref=-0.1498115 sc=-0.1498116
ACN  6 l=2 m=+0 maxerr 1.190e-07 at az=-180 el=-60 ref=+0.6250000 sc=+0.6250001
ACN  7 l=2 m=+1 maxerr 1.183e-07 at az=-175 el=85 ref=-0.1498115 sc=-0.1498116
ACN  8 l=2 m=+2 maxerr 1.252e-07 at az=-160 el=0 ref=+0.6634139 sc=+0.6634138
ACN  9 l=3 m=-3 maxerr 5.715e-07 at az=-170 el=0 ref=-0.3952847 sc=-0.3952853
ACN 10 l=3 m=-2 maxerr 1.582e-07 at az=-170 el=-30 ref=-0.2483697 sc=-0.2483698
ACN 11 l=3 m=-1 maxerr 1.892e-07 at az=-95 el=85 ref=-0.2106553 sc=-0.2106555
ACN 12 l=3 m=+0 maxerr 1.488e-07 at az=-180 el=85 ref=+0.9772767 sc=+0.9772768
ACN 13 l=3 m=+1 maxerr 1.892e-07 at az=-175 el=85 ref=-0.2106553 sc=-0.2106555
ACN 14 l=3 m=+2 maxerr 8.001e-08 at az=-175 el=-30 ref=-0.7151520 sc=-0.7151521
ACN 15 l=3 m=+3 maxerr 3.319e-07 at az=-170 el=0 ref=-0.6846532 sc=-0.6846529
ACN 16 l=4 m=-4 maxerr 3.019e-07 at az=-175 el=0 ref=+0.2529273 sc=+0.2529270
ACN 17 l=4 m=-3 maxerr 4.945e-07 at az=-170 el=-30 ref=+0.3396416 sc=+0.3396421
ACN 18 l=4 m=-2 maxerr 1.325e-07 at az=-170 el=0 ref=-0.1911951 sc=-0.1911952
ACN 19 l=4 m=-1 maxerr 2.259e-07 at az=-100 el=85 ref=-0.2667963 sc=-0.2667965
ACN 20 l=4 m=+0 maxerr 2.980e-07 at az=-180 el=-60 ref=+0.0234375 sc=+0.0234378
ACN 21 l=4 m=+1 maxerr 2.259e-07 at az=-170 el=85 ref=-0.2667963 sc=-0.2667965
ACN 22 l=4 m=+2 maxerr 6.557e-08 at az=-125 el=-60 ref=-0.2031448 sc=-0.2031447
ACN 23 l=4 m=+3 maxerr 3.142e-07 at az=-170 el=-30 ref=+0.5882766 sc=+0.5882763
ACN 24 l=4 m=+4 maxerr 2.412e-07 at az=-160 el=0 ref=+0.1284146 sc=+0.1284143
ACN 25 l=5 m=-5 maxerr 4.739e-07 at az=-180 el=0 ref=-0.0000000 sc=+0.0000005
ACN 26 l=5 m=-4 maxerr 2.594e-07 at az=-170 el=-30 ref=-0.4010747 sc=-0.4010750
ACN 27 l=5 m=-3 maxerr 3.807e-07 at az=-170 el=0 ref=+0.2614563 sc=+0.2614566
ACN 28 l=5 m=-2 maxerr 1.987e-07 at az=-160 el=-60 ref=-0.4456386 sc=-0.4456388
ACN 29 l=5 m=-1 maxerr 3.516e-07 at az=-95 el=-60 ref=-0.5576370 sc=-0.5576374
ACN 30 l=5 m=+0 maxerr 4.024e-07 at az=-180 el=-60 ref=+0.2232722 sc=+0.2232718
ACN 31 l=5 m=+1 maxerr 3.516e-07 at az=-175 el=-60 ref=-0.5576370 sc=-0.5576374
ACN 32 l=5 m=+2 maxerr 1.354e-07 at az=-175 el=-60 ref=-0.6827580 sc=-0.6827581
ACN 33 l=5 m=+3 maxerr 2.343e-07 at az=-170 el=0 ref=+0.4528555 sc=+0.4528553
ACN 34 l=5 m=+4 maxerr 2.003e-07 at az=-160 el=-30 ref=-0.1083498 sc=-0.1083496
ACN 35 l=5 m=+5 maxerr 6.045e-07 at az=-160 el=0 ref=+0.1218247 sc=+0.1218254
ACN 36 l=6 m=-6 maxerr 6.377e-07 at az=-175 el=0 ref=+0.3358466 sc=+0.3358460
ACN 37 l=6 m=-5 maxerr 3.829e-07 at az=-180 el=-30 ref=+0.0000000 sc=-0.0000004
ACN 38 l=6 m=-4 maxerr 2.016e-07 at az=-175 el=-30 ref=+0.1670177 sc=+0.1670175
ACN 39 l=6 m=-3 maxerr 3.617e-07 at az=-170 el=-60 ref=+0.2573710 sc=+0.2573714
ACN 40 l=6 m=-2 maxerr 3.142e-07 at az=-115 el=-60 ref=+0.5257816 sc=+0.5257819
ACN 41 l=6 m=-1 maxerr 4.890e-07 at az=-95 el=-60 ref=+0.2625388 sc=+0.2625393
ACN 42 l=6 m=+0 maxerr 1.109e-06 at az=-180 el=85 ref=+0.9215976 sc=+0.9215987
ACN 43 l=6 m=+1 maxerr 4.890e-07 at az=-175 el=-60 ref=+0.2625388 sc=+0.2625393
ACN 44 l=6 m=+2 maxerr 2.942e-07 at az=-175 el=-60 ref=+0.6759318 sc=+0.6759321
ACN 45 l=6 m=+3 maxerr 2.428e-07 at az=-170 el=-60 ref=+0.4457797 sc=+0.4457794
ACN 46 l=6 m=+4 maxerr 1.634e-07 at az=-165 el=0 ref=-0.2480392 sc=-0.2480390
ACN 47 l=6 m=+5 maxerr 4.847e-07 at az=-160 el=-30 ref=-0.0984136 sc=-0.0984141
ACN 48 l=6 m=+6 maxerr 9.657e-07 at az=-170 el=0 ref=+0.3358466 sc=+0.3358457
channels >1e-4: 0 overall max 1.1094072033257518e-06
horizon back quadrant max err 9.656906983512847e-07

## 16-speaker horizontal ring, order-6 AmbisonicsCodec::getDecodeMatrix vs double numpy
decode max abs err float vs double: 1.0699965627712638e-05 max |D| 0.18558267363326456
SC mirror asymmetry (float): 1.0698999999995962e-05
135 peak spk 6 max 0.8112 min -0.1504 err vs double 1.4800426033273473e-05
-135 peak spk 10 max 0.8112 min -0.1504 err vs double 9.150035294711034e-06
150 peak spk 7 max 0.7174 min -0.1737 err vs double 1.3132329888893013e-05
-150 peak spk 9 max 0.7174 min -0.1737 err vs double 8.518751811827097e-06
170 peak spk 8 max 0.649 min -0.1526 err vs double 1.235441266755899e-05
-170 peak spk 8 max 0.649 min -0.1526 err vs double 9.611899768280363e-06
