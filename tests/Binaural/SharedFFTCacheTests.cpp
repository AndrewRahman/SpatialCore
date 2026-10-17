#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Binaural/SharedFFTCache.h>
#include "../Support/Watchdog.h"
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// SharedFFTCache (Phase 5, VERIFY-02, D-08). The cache is process-global: one juce::dsp::FFT per
// order, created on first request and kept for the life of the process. It came from OpenSpatialDelay
// Spatial-Media-Lab/OpenSpatialDelay#142 (PR) for Spatial-Media-Lab/OpenSpatialDelay#131 and had no unit test of its own until now.
//
// Because the cache is process-global, a case that runs after another in the same executable sees
// the orders the earlier one created. The concurrent case therefore uses orders 14 and 15, which no
// other test asks for, so its first use is concurrent even when the whole executable runs in one
// process (ctest runs every case in its own process anyway). The other cases only assert things
// that hold whether or not an order already exists.
// ============================================================================

TEST_CASE ("SharedFFTCache: two requests for one order return the same FFT object",
           "[fftcache][tsan]")
{
    auto first = getSharedFFTCache().getOrCreate (10);
    auto second = getSharedFFTCache().getOrCreate (10);

    REQUIRE (first != nullptr);
    CHECK (first.get() == second.get());
    CHECK (first->getSize() == 1024);
}

TEST_CASE ("SharedFFTCache: the cache keeps an FFT alive after every user has dropped it",
           "[fftcache][tsan]")
{
    auto user = getSharedFFTCache().getOrCreate (12);
    std::weak_ptr<juce::dsp::FFT> watcher = user;
    juce::dsp::FFT* const address = user.get();

    user.reset();   // every user is gone; only the cache can still hold the FFT

    CHECK_FALSE (watcher.expired());
    auto again = getSharedFFTCache().getOrCreate (12);
    CHECK (again.get() == address);
    CHECK (again == watcher.lock());
}

TEST_CASE ("SharedFFTCache: orders 9 to 13 give distinct objects of the right size",
           "[fftcache][tsan]")
{
    std::array<std::shared_ptr<juce::dsp::FFT>, 5> ffts;
    for (int order = 9; order <= 13; ++order)
    {
        auto& fft = ffts[static_cast<size_t> (order - 9)];
        fft = getSharedFFTCache().getOrCreate (order);
        REQUIRE (fft != nullptr);
        CHECK (fft->getSize() == (1 << order));
    }

    for (size_t a = 0; a < ffts.size(); ++a)
        for (size_t b = a + 1; b < ffts.size(); ++b)
            CHECK (ffts[a].get() != ffts[b].get());
}

namespace
{
    constexpr int kThreads = 16;

    struct ConcurrentState
    {
        std::atomic<bool> go { false };
        std::atomic<int> ready { 0 };
        std::array<std::shared_ptr<juce::dsp::FFT>, kThreads> order14;
        std::array<std::shared_ptr<juce::dsp::FFT>, kThreads> order15;
    };
}

TEST_CASE ("SharedFFTCache: sixteen threads released together asking for new orders get one object per order",
           "[fftcache][tsan]")
{
    auto state = std::make_shared<ConcurrentState>();

    const bool finished = runWithWatchdog ([state]
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < kThreads; ++i)
        {
            threads.emplace_back ([state, i]
            {
                state->ready.fetch_add (1);
                while (! state->go.load (std::memory_order_acquire))
                    std::this_thread::yield();

                state->order14[static_cast<size_t> (i)] = getSharedFFTCache().getOrCreate (14);
                state->order15[static_cast<size_t> (i)] = getSharedFFTCache().getOrCreate (15);
            });
        }

        while (state->ready.load() < kThreads)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        state->go.store (true, std::memory_order_release);

        for (auto& t : threads)
            t.join();
    }, std::chrono::milliseconds (10000LL * timeoutScaleFromEnv()));

    REQUIRE (finished);

    REQUIRE (state->order14[0] != nullptr);
    REQUIRE (state->order15[0] != nullptr);
    CHECK (state->order14[0]->getSize() == (1 << 14));
    CHECK (state->order15[0]->getSize() == (1 << 15));
    CHECK (state->order14[0].get() != state->order15[0].get());

    int differing14 = 0, differing15 = 0;
    for (size_t i = 0; i < static_cast<size_t> (kThreads); ++i)
    {
        differing14 += state->order14[i].get() != state->order14[0].get() ? 1 : 0;
        differing15 += state->order15[i].get() != state->order15[0].get() ? 1 : 0;
    }
    CHECK (differing14 == 0);
    CHECK (differing15 == 0);
}
