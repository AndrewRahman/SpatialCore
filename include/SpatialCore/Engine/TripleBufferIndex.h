#pragma once

#include <atomic>

namespace spatialcore
{

//==============================================================================
// TripleBufferIndex -- wait-free single-writer / single-reader handoff over
// three slots (SC-16).
//
// The payload lives elsewhere (RenderEngine::layoutBuffers); this class only
// decides which slot index each side may touch:
//
//   - the WRITER (message thread) fills writeSlot(), then calls publish();
//   - the READER (audio thread) calls acquireLatest() once per block and reads
//     the slot it returns until its next acquireLatest().
//
// Exactly one thread may call the writer methods and exactly one thread may
// call the reader methods. Writer methods and reader methods may run
// concurrently with each other.
//
// Guarantees:
//   - the slot handed to the writer is never the slot the reader holds, so a
//     fill can never race a read, however many publishes occur between two
//     acquires;
//   - publishes are observed in order, and a publish superseded before the
//     reader acquired is skipped whole: the reader only ever holds a slot the
//     writer has finished and published;
//   - wait-free: publish() is one atomic exchange, acquireLatest() is one
//     relaxed load plus at most one atomic exchange. No allocation, no lock,
//     no loop, so it is safe to call from the audio thread.
//
// Why it is safe: only the writer sets the fresh bit and only the reader
// clears it. Each exchange hands the other side a slot the caller has
// finished with, so writer, reader and pending slots always form a permutation
// of {0, 1, 2}. The release/acquire pair on middle_ orders the fill before the
// read and the read before the refill.
//
// Initial state: the reader holds slot 0, the writer fills slot 2, slot 1 is
// pending and stale.
//==============================================================================
class TripleBufferIndex
{
public:
    static constexpr int kNumSlots = 3;

    static_assert (std::atomic<int>::is_always_lock_free,
                   "TripleBufferIndex requires a lock-free std::atomic<int> to stay wait-free");

    //--------------------------------------------------------------------------
    // Writer thread (message thread)
    //--------------------------------------------------------------------------

    // The slot the writer may fill. Never equals readSlot().
    int writeSlot() const noexcept { return writer_; }

    // Publishes the slot returned by writeSlot() (it must be fully written)
    // and hands the writer a new slot to fill.
    void publish() noexcept
    {
        lastPublished_ = writer_;
        const int previous = middle_.exchange (writer_ | kFreshBit, std::memory_order_acq_rel);
        writer_ = previous & kIndexMask;
    }

    // The slot most recently passed to publish() (slot 0 before any publish,
    // matching the reader's initial slot). Writer-thread view only.
    int lastPublishedSlot() const noexcept { return lastPublished_; }

    //--------------------------------------------------------------------------
    // Reader thread (audio thread)
    //--------------------------------------------------------------------------

    // Moves the reader to the latest published slot, if any was published since
    // the previous call, and returns the slot the reader now holds.
    int acquireLatest() noexcept
    {
        if (hasFresh())
        {
            const int previous = middle_.exchange (reader_, std::memory_order_acq_rel);
            reader_ = previous & kIndexMask;
        }
        return reader_;
    }

    // The slot the reader currently holds.
    int readSlot() const noexcept { return reader_; }

    // SC-20: true when a publish() has happened since the reader's last
    // acquireLatest(). One relaxed load, wait-free. It is a HINT the reader may
    // act on before acquiring (the engine's switch fade renders the layout it
    // already holds for one more block when this is true): it never moves the
    // reader and never clears the bit, and it makes no promise about the
    // published payload, so the reader must not read a fresh slot until
    // acquireLatest() has returned it.
    bool hasFresh() const noexcept
    {
        return (middle_.load (std::memory_order_relaxed) & kFreshBit) != 0;
    }

    //--------------------------------------------------------------------------
    // Diagnostic (invariant tests)
    //--------------------------------------------------------------------------

    // The slot currently parked between the two sides (relaxed read).
    int pendingSlot() const noexcept
    {
        return middle_.load (std::memory_order_relaxed) & kIndexMask;
    }

private:
    static constexpr int kIndexMask = 3;
    static constexpr int kFreshBit = 4;

    // Bits 0-1: the pending slot. Bit 2: set by publish(), cleared by acquire.
    std::atomic<int> middle_ { 1 };

    // Writer-thread state.
    int writer_ = 2;
    int lastPublished_ = 0;

    // Reader-thread state.
    int reader_ = 0;
};

} // namespace spatialcore
