#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

namespace spatialcore::test
{

/** SPATIALCORE_TEST_TIMEOUT_SCALE: an integer of at least 1 (default 1; anything else counts as 1).
    It stretches the watchdogs and idle waits of the thread tests, so a sanitizer build (which runs
    several times slower) can use the same cases. Same rules as the scale in ProfileSwitchTests.cpp,
    which keeps its own copy. */
inline int timeoutScaleFromEnv()
{
    const char* text = std::getenv ("SPATIALCORE_TEST_TIMEOUT_SCALE");
    if (text == nullptr)
        return 1;

    char* end = nullptr;
    const long value = std::strtol (text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 1000)
        return 1;
    return static_cast<int> (value);
}

/** Run fn on its own thread and wait up to limit for it to finish. Returns true
    if it finished (the thread is joined) and false if it timed out (the thread
    is detached and left running). std::async is deliberately not used: its
    future blocks in the destructor on a hung task, which would turn a
    regression into a hung test binary instead of a failure. fn must only touch
    state it owns through a captured std::shared_ptr, so a detached, stuck thread
    never reads or writes freed memory. */
template <typename Fn>
bool runWithWatchdog (Fn fn, std::chrono::milliseconds limit)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    std::thread worker ([fn = std::move (fn), done]() mutable
    {
        fn();
        done->store (true);
    });

    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (! done->load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));

    if (done->load())
    {
        worker.join();
        return true;
    }

    worker.detach();
    return false;
}

} // namespace spatialcore::test
