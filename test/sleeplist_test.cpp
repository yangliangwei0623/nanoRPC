#include "remmy/sleeplist.h"
#include "check.h"

#include <chrono>
#include <future>

using namespace std::chrono_literals;
using remmy::ErrorCode;

int main() {
    remmy::SleepingList<int> events;
    int response = 42;
    const auto check_removed = [&] {
        CHECK(events.GetResponsePtr(1) == nullptr);
        // A late notification after removal must be harmless.
        events.SignalResponse(1);
        events.SignalServerFail(1);
    };

    for (const auto timeout : {0u, 2000u}) {
        for (const bool fail : {false, true}) {
            events.AddEvent(1, &response);
            CHECK(events.GetResponsePtr(1) == &response);
            if (fail) events.SignalServerFail(1);
            else events.SignalResponse(1);
            const auto start = std::chrono::steady_clock::now();
            CHECK(events.WaitForResponse(1, timeout) ==
                  (fail ? ErrorCode::SERVER_FAIL : ErrorCode::SUCCESS));
            CHECK(std::chrono::steady_clock::now() - start < 1s);
            check_removed();

            events.AddEvent(1, &response);
            auto waiting = std::async(std::launch::async, [&] {
                return events.WaitForResponse(1, timeout);
            });
            CHECK(waiting.wait_for(30ms) == std::future_status::timeout);
            if (fail) events.SignalServerFail(1);
            else events.SignalResponse(1);
            CHECK(waiting.wait_for(1s) == std::future_status::ready);
            CHECK(waiting.get() == (fail ? ErrorCode::SERVER_FAIL : ErrorCode::SUCCESS));
            check_removed();
        }
    }

    events.AddEvent(1, &response);
    const auto start = std::chrono::steady_clock::now();
    CHECK(events.WaitForResponse(1, 40) == ErrorCode::TIMEOUT);
    CHECK(std::chrono::steady_clock::now() - start >= 40ms);
    check_removed();

    // Failure takes priority even when a success notification is also present.
    for (const bool failure_first : {false, true}) {
        events.AddEvent(1, &response);
        if (failure_first) events.SignalServerFail(1);
        events.SignalResponse(1);
        if (!failure_first) events.SignalServerFail(1);
        CHECK(events.WaitForResponse(1) == ErrorCode::SERVER_FAIL);
        check_removed();
    }
    events.AddEvent(1, &response);
    events.RemoveEvent(1);
    check_removed();
}
