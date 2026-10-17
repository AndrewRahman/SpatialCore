#pragma once

// RealtimeGuard -- the test-side face of the SpatialCoreRTGuard trap (Phase 5, RTSF-05).
//
// The trap (tests/Support/rtguard/RTGuard.cpp) counts every allocation, free and pthread lock made
// on the ONE thread it is armed on. It exists only on macOS and only in a tree whose
// CMAKE_CXX_FLAGS carry no -fsanitize (a sanitizer's allocator blinds libmalloc's logger, RESEARCH
// Pitfall 2). tests/CMakeLists.txt defines SPATIALCORE_RT_GUARD=1 exactly then. In every other
// tree each function here compiles to a no-op returning zero and no scrt_* symbol is referenced,
// so the test target links without the library; every trap test then SKIPs, never passes green
// (D-01, D-13).
//
// Discipline for a test that uses it (DR-1, Pitfalls 1 and 3):
//   - run requireRealtimeTrapCanary() first: a trap that cannot see reports a false zero;
//   - fill every input buffer and make every message-side call while DISARMED;
//   - arm only around the call under test (ScopedRealtimeTrap), from the first block after prepare().

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include <cstdlib>
#include <mutex>

#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
 extern "C"
 {
     void scrt_reset();
     void scrt_arm();
     void scrt_disarm();
     int scrt_allocs();
     int scrt_frees();
     int scrt_locks();
     void scrt_print_first_offender();
 }
#endif

namespace spatialcore::test
{

struct RealtimeCounts
{
    int allocs = 0;
    int frees = 0;
    int locks = 0;
};

constexpr bool realtimeTrapAvailable() noexcept
{
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
    return true;
#else
    return false;
#endif
}

// Skips the running Catch2 case when no trap is linked. Never lets an unavailable trap read as a pass.
inline void skipUnlessRealtimeTrap()
{
    if constexpr (! realtimeTrapAvailable())
    {
        SKIP ("allocation/lock trap available on macOS only, and not under a sanitizer");
    }
}

inline void resetRealtimeCounts()
{
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
    scrt_reset();
#endif
}

inline RealtimeCounts realtimeCounts()
{
    RealtimeCounts c;
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
    c.allocs = scrt_allocs();
    c.frees = scrt_frees();
    c.locks = scrt_locks();
#endif
    return c;
}

inline void printFirstOffender()
{
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
    scrt_print_first_offender();
#endif
}

// Arms the trap on the constructing thread; disarms on destruction. Does not reset the counters,
// so several scopes in a row accumulate.
class ScopedRealtimeTrap
{
public:
    ScopedRealtimeTrap()
    {
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
        scrt_arm();
#endif
    }

    ~ScopedRealtimeTrap()
    {
#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
        scrt_disarm();
#endif
    }

    ScopedRealtimeTrap (const ScopedRealtimeTrap&) = delete;
    ScopedRealtimeTrap& operator= (const ScopedRealtimeTrap&) = delete;
};

// Proves the trap sees a deliberate malloc, a deliberate new, a juce::CriticalSection lock and a
// std::mutex lock in THIS build. Every trap case calls it first (RESEARCH Pitfall 2, T-05-02).
inline void requireRealtimeTrapCanary()
{
    skipUnlessRealtimeTrap();

#if defined (SPATIALCORE_RT_GUARD) && SPATIALCORE_RT_GUARD == 1
    resetRealtimeCounts();

    juce::CriticalSection criticalSection;
    std::mutex plainMutex;

    {
        const ScopedRealtimeTrap trap;

        void* p = std::malloc (64);
        asm volatile ("" :: "r" (p) : "memory");   // keep the optimiser from eliding the pair
        std::free (p);

        volatile int* q = new int (7);
        asm volatile ("" :: "r" (q) : "memory");
        delete q;

        {
            const juce::ScopedLock lock (criticalSection);
        }

        {
            const std::lock_guard<std::mutex> lock (plainMutex);
        }
    }

    const RealtimeCounts seen = realtimeCounts();
    CAPTURE (seen.allocs, seen.frees, seen.locks);
    REQUIRE (seen.allocs >= 2);
    REQUIRE (seen.frees >= 2);
    REQUIRE (seen.locks >= 2);
#endif
}

} // namespace spatialcore::test
