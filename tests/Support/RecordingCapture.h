#pragma once

#include <juce_osc/juce_osc.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace spatialcore::test
{

// Thread-safe capture of every message that reaches a loopback UDP port. The network thread
// appends, the test thread reads, so a lock guards the list. Shared by the OSC sender and
// receiver tests (IN-04: it used to be copied between the two files).
//
// WR-06: settle() replaces any "quiet for N ms" wait. It sends a marker datagram to the same
// port from a second socket and waits until the receiver has seen it. Loopback delivers a
// datagram into the receiving socket's queue inside sendto(), and the receive thread reads
// that queue in order, so by the time the marker arrives every datagram sent before settle()
// was called has already been recorded. The wait ends on that event, not on a timer, so a
// slow machine makes it longer instead of making the count wrong. Markers are not recorded
// as entries.
struct RecordingCapture : public juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    struct Entry
    {
        juce::String address;
        float args[3] = {};
        int argCount = 0;
    };

    // port is the port this capture's OSCReceiver listens on.
    explicit RecordingCapture (int port)
    {
        markerSender.connect ("127.0.0.1", port);
    }

    void oscMessageReceived (const juce::OSCMessage& message) override
    {
        const auto address = message.getAddressPattern().toString();
        if (address == kMarkerAddress)
        {
            ++markersSeen;
            return;
        }

        Entry e;
        e.address  = address;
        e.argCount = message.size();
        // IN-04: getFloat32() on another type asserts in Debug, and stray traffic on a
        // shared port can be anything, so a non-float argument is recorded as 0.
        for (int i = 0; i < message.size() && i < 3; ++i)
            e.args[i] = message[i].isFloat32() ? message[i].getFloat32() : 0.0f;

        const juce::ScopedLock sl (lock);
        entries.push_back (e);
    }

    int count() const
    {
        const juce::ScopedLock sl (lock);
        return (int) entries.size();
    }

    int countFor (const juce::String& address) const
    {
        const juce::ScopedLock sl (lock);
        int n = 0;
        for (const auto& e : entries)
            if (e.address == address)
                ++n;
        return n;
    }

    Entry at (int i) const
    {
        const juce::ScopedLock sl (lock);
        return i >= 0 && i < (int) entries.size() ? entries[(size_t) i] : Entry {};
    }

    Entry last() const
    {
        const juce::ScopedLock sl (lock);
        return entries.empty() ? Entry {} : entries.back();
    }

    // Waits for every datagram sent so far to be recorded, then returns the count.
    int settle (int timeoutMs = 5000)
    {
        const int token = ++markersSent;
        markerSender.send (juce::OSCMessage (juce::OSCAddressPattern (kMarkerAddress)));

        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds (timeoutMs);
        while (markersSeen.load() < token && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        return count();
    }

    static constexpr const char* kMarkerAddress = "/spatialcore-test/settle";

    mutable juce::CriticalSection lock;
    std::vector<Entry> entries;
    juce::OSCSender markerSender;
    std::atomic<int> markersSent { 0 };
    std::atomic<int> markersSeen { 0 };
};

} // namespace spatialcore::test
