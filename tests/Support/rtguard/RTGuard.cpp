// RTGuard.cpp -- SpatialCoreRTGuard, the realtime-safety trap (Phase 5, RTSF-05).
//
// TEST-ONLY and macOS-ONLY. This file is compiled into the shared library SpatialCoreRTGuard
// (tests/CMakeLists.txt) and linked into SpatialCoreTests alone. It is never linked into
// SpatialCore, SpatialCoreUI or any target a consumer links: an allocation hook and a pthread
// interposer inside a plugin would alter the host process (threat T-05-01).
//
// What it counts, on the ONE thread it is armed on (other threads are never counted):
//   - allocations and frees: libmalloc's malloc_logger SPI sees malloc, calloc, realloc,
//     posix_memalign, operator new and free. Type bit 2 is an allocation, bit 4 a free;
//     realloc (0xe) counts both, calloc (0x4a) counts one allocation.
//   - locks: a __DATA,__interpose table redirects pthread_mutex_lock, pthread_mutex_trylock,
//     pthread_rwlock_rdlock, pthread_rwlock_wrlock and pthread_cond_wait to counting wrappers
//     that then call the real function. std::mutex and juce::CriticalSection both reach those.
//
// Why the interpose tuples must live in a linked dylib: dyld applies a __DATA,__interpose table
// to every image loaded AFTER the one that holds it, and a table in the main executable is
// ignored (measured: 0 hits). The test executable links this dylib, so the tuples reach every
// image, including libc++'s std::mutex.
//
// What it does NOT see: juce::SpinLock and any other spin on an atomic (no system call), and
// os_unfair_lock_lock (an interposer on it aborted the process, RESEARCH Pitfall 10). The spin
// locks are covered by the Plan 05-03 source-scan rule.
//
// Hook discipline (RESEARCH Pitfall 1): no per-thread storage (it allocates through malloc on a
// new thread and re-enters the hook before it exists), no logging, no allocation inside the
// hook. The armed thread is an std::atomic<pthread_t> compared with pthread_self().

#include <execinfo.h>
#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <cstdio>

extern "C"
{
// libmalloc SPI, not declared in any public header. Type bits: alloc = 2, dealloc = 4.
typedef void (malloc_logger_t) (uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
                                uintptr_t result, uint32_t skip);
extern malloc_logger_t* malloc_logger;
}

namespace
{
    constexpr int kMaxFrames = 64;

    std::atomic<pthread_t> gArmed { nullptr };
    std::atomic<int> gAllocs { 0 };
    std::atomic<int> gFrees { 0 };
    std::atomic<int> gLocks { 0 };

    // The stack of the first counted allocation since the last reset. Written only by the armed
    // thread, from inside the hook, while the armed id is cleared so the capture cannot recurse.
    void* gFrames[kMaxFrames];
    std::atomic<int> gFrameCount { 0 };
    std::atomic<bool> gHaveOffender { false };

    inline bool armedOnThisThread() noexcept
    {
        return gArmed.load (std::memory_order_relaxed) == pthread_self();
    }

    void mallocHook (uint32_t type, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t)
    {
        const pthread_t self = pthread_self();
        if (gArmed.load (std::memory_order_relaxed) != self)
            return;   // other threads are never counted

        if ((type & 2u) != 0)
        {
            const int previous = gAllocs.fetch_add (1, std::memory_order_relaxed);

            if (previous == 0 && ! gHaveOffender.load (std::memory_order_relaxed))
            {
                // First counted allocation since the reset: capture the stack. backtrace() can
                // itself allocate on its first call, so disarm while it runs, then re-arm.
                gArmed.store (nullptr, std::memory_order_relaxed);
                const int n = backtrace (gFrames, kMaxFrames);
                gFrameCount.store (n, std::memory_order_relaxed);
                gHaveOffender.store (true, std::memory_order_relaxed);
                gArmed.store (self, std::memory_order_relaxed);
            }
        }

        if ((type & 4u) != 0)
            gFrees.fetch_add (1, std::memory_order_relaxed);
    }

    inline void noteLock() noexcept
    {
        if (armedOnThisThread())
            gLocks.fetch_add (1, std::memory_order_relaxed);
    }
}

extern "C"
{
void scrt_reset()
{
    gAllocs.store (0);
    gFrees.store (0);
    gLocks.store (0);
    gFrameCount.store (0);
    gHaveOffender.store (false);
}

// Installs the hook once, then arms the calling thread. Does NOT reset the counters, so several
// armed blocks accumulate into one total.
void scrt_arm()
{
    if (malloc_logger != mallocHook)
        malloc_logger = mallocHook;

    gArmed.store (pthread_self());
}

void scrt_disarm()
{
    gArmed.store (nullptr);
}

int scrt_allocs() { return gAllocs.load(); }
int scrt_frees()  { return gFrees.load(); }
int scrt_locks()  { return gLocks.load(); }

void scrt_print_first_offender()
{
    if (! gHaveOffender.load())
    {
        std::fputs ("RTGuard: no allocation captured\n", stderr);
        return;
    }

    std::fputs ("RTGuard: stack of the first allocation on the armed thread:\n", stderr);
    backtrace_symbols_fd (gFrames, gFrameCount.load(), 2);
}

// Counting wrappers. Each counts when armed on this thread, then forwards to the real function.
int rt_mutex_lock (pthread_mutex_t* m)
{
    noteLock();
    return pthread_mutex_lock (m);
}

int rt_mutex_trylock (pthread_mutex_t* m)
{
    noteLock();
    return pthread_mutex_trylock (m);
}

int rt_rwlock_rdlock (pthread_rwlock_t* l)
{
    noteLock();
    return pthread_rwlock_rdlock (l);
}

int rt_rwlock_wrlock (pthread_rwlock_t* l)
{
    noteLock();
    return pthread_rwlock_wrlock (l);
}

int rt_cond_wait (pthread_cond_t* c, pthread_mutex_t* m)
{
    noteLock();
    return pthread_cond_wait (c, m);
}
}

struct ScrtInterpose
{
    const void* replacement;
    const void* original;
};

#define SCRT_INTERPOSE(r, o) { (const void*) (r), (const void*) (o) }

__attribute__ ((used)) static const ScrtInterpose kScrtTuples[]
    __attribute__ ((section ("__DATA,__interpose"))) =
{
    SCRT_INTERPOSE (rt_mutex_lock,      pthread_mutex_lock),
    SCRT_INTERPOSE (rt_mutex_trylock,   pthread_mutex_trylock),
    SCRT_INTERPOSE (rt_rwlock_rdlock,   pthread_rwlock_rdlock),
    SCRT_INTERPOSE (rt_rwlock_wrlock,   pthread_rwlock_wrlock),
    SCRT_INTERPOSE (rt_cond_wait,       pthread_cond_wait),
};
