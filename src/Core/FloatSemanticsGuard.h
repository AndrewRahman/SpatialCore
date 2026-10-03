#pragma once

//==============================================================================
// Private to SpatialCore's own sources (src/), never installed or included by
// a public header: consumers such as OpenSpatialDelay compile their own
// targets with -ffast-math and include SpatialCore's public headers, which is
// fine and must stay fine.
//
// WR-04: every non-finite guard in SpatialCore (D-06, D-19: the hold-last-good
// sanitiser in RenderEngine, the direction guards in SpatialMath's VBAP,
// ConstantPower, KNN, DirectBinaural and Ambisonics, and DBAP's non-finite
// direction/distance rule) is a std::isfinite / std::isnan test. Under
// -ffast-math or -ffinite-math-only (MSVC: /fp:fast) the compiler may assume no
// value is ever NaN or infinite and fold those tests to "finite", so a
// non-finite position would reach the audio path again with no diagnostic.
//
// ADMOSCReceiver.cpp includes it for a different reason (IN-16): it has no
// isfinite / isnan test of its own. It forwards a single-axis /azim, /elev or
// /dist message with the other two axes set to the NAN macro, a sentinel the
// consumer's Listener detects with std::isnan to update only the received
// axis. Under finite-math-only that NaN is undefined behaviour, so the
// sentinel contract the consumer relies on needs the same IEEE semantics.
//
// CMakeLists.txt keeps fast-math off the SpatialCore target; this turns that
// comment into a build failure in every including translation unit, whichever
// way the flag arrives (target options, CMAKE_CXX_FLAGS, add_compile_options
// before add_subdirectory(SpatialCore)). Include it from every src/ file that
// relies on a non-finite test or produces a NaN/Inf value on purpose.
//==============================================================================

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__) \
    || defined(_M_FP_FAST)
 #error "SpatialCore's non-finite guards (D-06/D-19) need IEEE NaN/Inf semantics: do not compile SpatialCore's sources with -ffast-math, -ffinite-math-only or /fp:fast (consumer targets may use them)"
#endif
