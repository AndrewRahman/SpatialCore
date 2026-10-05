#include <catch2/catch_test_macros.hpp>
#include <SpatialCore/Engine/TripleBufferIndex.h>
#include <atomic>
#include <string>

using namespace spatialcore;

// ============================================================================
// TripleBufferIndex -- the wait-free single-writer / single-reader handoff that
// RenderEngine uses to pass layout slots from the message thread to the audio
// thread (SC-16). These cases pin the index logic exhaustively and
// single-threaded: the cross-thread memory ordering is covered by the OSP
// [switchstress] regression run under ThreadSanitizer.
// ============================================================================

namespace
{
    constexpr int kMaxSequenceLength = 12;

    // Runs the operation sequence encoded in the low `length` bits of `bits`
    // (bit set = publish, bit clear = acquire) and returns the sequence as a
    // string of 'P' / 'A' characters.
    std::string sequenceToString (unsigned bits, int length)
    {
        std::string s;
        for (int i = 0; i < length; ++i)
            s.push_back (((bits >> i) & 1u) != 0 ? 'P' : 'A');
        return s;
    }

    bool isPermutationOfSlots (int a, int b, int c)
    {
        const bool inRange = a >= 0 && a < 3 && b >= 0 && b < 3 && c >= 0 && c < 3;
        return inRange && a != b && b != c && a != c;
    }
}

TEST_CASE ("TripleBufferIndex: initial state holds slot 0 for the reader and publishes nothing",
           "[engine][triple-buffer]")
{
    TripleBufferIndex idx;

    CHECK (TripleBufferIndex::kNumSlots == 3);
    CHECK (idx.readSlot() == 0);
    CHECK (idx.writeSlot() == 2);
    CHECK (idx.pendingSlot() == 1);
    CHECK (idx.lastPublishedSlot() == 0);

    // Nothing published: the reader does not move.
    CHECK (idx.acquireLatest() == 0);
    CHECK (idx.readSlot() == 0);
    CHECK (idx.writeSlot() == 2);
    CHECK (idx.pendingSlot() == 1);
}

TEST_CASE ("TripleBufferIndex: writer, reader and pending slots stay a permutation of {0,1,2} over every sequence up to length 12",
           "[engine][triple-buffer]")
{
    int sequencesRun = 0;
    std::string firstFailure;

    for (int length = 1; length <= kMaxSequenceLength; ++length)
    {
        for (unsigned bits = 0; bits < (1u << length); ++bits)
        {
            TripleBufferIndex idx;
            bool ok = true;

            for (int step = 0; step < length && ok; ++step)
            {
                if (((bits >> step) & 1u) != 0)
                    idx.publish();
                else
                    idx.acquireLatest();

                ok = isPermutationOfSlots (idx.writeSlot(), idx.readSlot(), idx.pendingSlot())
                     && idx.lastPublishedSlot() != idx.writeSlot();
            }

            ++sequencesRun;
            if (! ok && firstFailure.empty())
                firstFailure = sequenceToString (bits, length);
        }
    }

    CHECK (sequencesRun == 8190);
    INFO ("first failing sequence: " << firstFailure);
    REQUIRE (firstFailure.empty());
}

TEST_CASE ("TripleBufferIndex: acquireLatest returns the latest publish, skips superseded ones, and never moves without a publish",
           "[engine][triple-buffer]")
{
    int sequencesRun = 0;
    std::string firstFailure;

    for (int length = 1; length <= kMaxSequenceLength; ++length)
    {
        for (unsigned bits = 0; bits < (1u << length); ++bits)
        {
            TripleBufferIndex idx;
            bool ok = true;
            bool publishedSinceAcquire = false;
            int latestPublishedSlot = -1;

            for (int step = 0; step < length && ok; ++step)
            {
                if (((bits >> step) & 1u) != 0)
                {
                    // The slot being published is the one the writer holds now.
                    latestPublishedSlot = idx.writeSlot();
                    idx.publish();
                    publishedSinceAcquire = true;
                    ok = idx.lastPublishedSlot() == latestPublishedSlot;
                }
                else
                {
                    const int before = idx.readSlot();
                    const int got = idx.acquireLatest();
                    const int expected = publishedSinceAcquire ? latestPublishedSlot : before;
                    ok = (got == expected) && (idx.readSlot() == got);
                    publishedSinceAcquire = false;
                }
            }

            ++sequencesRun;
            if (! ok && firstFailure.empty())
                firstFailure = sequenceToString (bits, length);
        }
    }

    CHECK (sequencesRun == 8190);
    INFO ("first failing sequence: " << firstFailure);
    REQUIRE (firstFailure.empty());
}

TEST_CASE ("TripleBufferIndex: std::atomic<int> is lock-free on this platform",
           "[engine][triple-buffer]")
{
    // Alongside the header's static_assert: the handoff is only wait-free if
    // the atomic it exchanges on never falls back to a lock.
    CHECK (std::atomic<int> {}.is_lock_free());
}

// SC-20: the reader-side peek the engine's switch fade acts on before acquiring.
TEST_CASE ("TripleBufferIndex: hasFresh is false initially, true after publish, false after acquireLatest",
           "[engine][triple-buffer][sc20]")
{
    TripleBufferIndex idx;
    CHECK_FALSE (idx.hasFresh());

    // An acquire with nothing published leaves it false.
    idx.acquireLatest();
    CHECK_FALSE (idx.hasFresh());

    idx.publish();
    CHECK (idx.hasFresh());

    // The peek never moves the reader or clears the bit.
    const int held = idx.readSlot();
    CHECK (idx.hasFresh());
    CHECK (idx.readSlot() == held);

    idx.acquireLatest();
    CHECK_FALSE (idx.hasFresh());

    // Several publishes before one acquire: still one fresh layout, cleared by one acquire.
    idx.publish();
    idx.publish();
    idx.publish();
    CHECK (idx.hasFresh());
    idx.acquireLatest();
    CHECK_FALSE (idx.hasFresh());
}
