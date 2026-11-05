#include "remmy/remmy.h"
#include "check.h"

#include <atomic>
#include <chrono>
#include <future>

using namespace std::chrono_literals;
using remmy::UniqueId;

int main() {
    std::promise<int> async_result;
    std::promise<int> replacement_result;
    std::promise<void> zero_result;
    std::promise<void> zero_replacement_result;
    std::atomic<bool> old_called = false;
    remmy::CommAsio comm("127.0.0.1", 0);
    remmy::RPCStub<remmy::AsioEP> rpc(&comm);
    rpc.RegisterSyncHandler<UniqueId("multiply"), int, int, int>(
        [](int x, int y) { return x * y; });
    rpc.RegisterSyncHandler<UniqueId("zero_sync"), int>([] { return 42; });
    rpc.RegisterAsyncHandler<UniqueId("add"), int, int>(
        [&](int x, int y) { async_result.set_value(x + y); });
    rpc.RegisterAsyncHandler<UniqueId("zero_async")>([&] { zero_result.set_value(); });
    rpc.RegisterAsyncHandlerReplaceable<int>(UniqueId("replace"),
        [&](int) { old_called = true; });
    rpc.RegisterAsyncHandlerReplaceable<int>(UniqueId("replace"),
        [&](int value) { replacement_result.set_value(value + 1); });
    rpc.RegisterAsyncHandlerReplaceable<>(UniqueId("zero_repl"),
        [&] { zero_replacement_result.set_value(); });
    rpc.StartServing();

    int result = 0;
    CHECK(rpc.RpcCall<UniqueId("multiply")>(comm.EP(), 2000, result, 6, 7) == remmy::ErrorCode::SUCCESS);
    CHECK(result == 42);
    CHECK(rpc.RpcCall<UniqueId("zero_sync")>(comm.EP(), 2000, result) == remmy::ErrorCode::SUCCESS);
    CHECK(result == 42);
    auto sum = async_result.get_future();
    CHECK(rpc.RpcCallAsync<UniqueId("add")>(comm.EP(), 6, 7) == remmy::ErrorCode::SUCCESS);
    CHECK(sum.wait_for(2s) == std::future_status::ready);
    CHECK(sum.get() == 13);
    auto replaced = replacement_result.get_future();
    CHECK(rpc.RpcCallAsync<UniqueId("replace")>(comm.EP(), 10) == remmy::ErrorCode::SUCCESS);
    CHECK(replaced.wait_for(2s) == std::future_status::ready);
    CHECK(replaced.get() == 11);
    CHECK(!old_called);
    auto zero = zero_result.get_future();
    CHECK(rpc.RpcCallAsync<UniqueId("zero_async")>(comm.EP()) == remmy::ErrorCode::SUCCESS);
    CHECK(zero.wait_for(2s) == std::future_status::ready);
    auto zero_replaced = zero_replacement_result.get_future();
    CHECK(rpc.RpcCallAsync<UniqueId("zero_repl")>(comm.EP()) == remmy::ErrorCode::SUCCESS);
    CHECK(zero_replaced.wait_for(2s) == std::future_status::ready);
}
