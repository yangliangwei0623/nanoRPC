#include "remmy/comm_asio.h"
#include "check.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    template<typename T>
    void Append(std::vector<char>& bytes, const T& value) {
        const char* first = reinterpret_cast<const char*>(&value);
        bytes.insert(bytes.end(), first, first + sizeof(value));
    }

    void AppendPacket(std::vector<char>& bytes, uint32_t magic, int payload) {
        Append(bytes, magic);
        Append(bytes, uint64_t{sizeof(magic) + sizeof(uint64_t) + sizeof(payload)});
        Append(bytes, payload);
    }

    void CheckPayload(const remmy::CommAsio::MessagePtr& message, int expected) {
        CHECK(message != nullptr);
        CHECK(message->GetStatus() == remmy::ErrorCode::SUCCESS);
        int value = 0;
        remmy::Deserialize(message->GetStreamBuffer(), value);
        CHECK(value == expected);
        CHECK(message->GetStreamBuffer().GetSize() == 0);
    }
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    const bool subsequent = std::string(argv[1]) == "subsequent";
    remmy::CommAsio receiver("127.0.0.1", 0);
    receiver.Start();
    asio::io_service io;
    remmy::AsioSocket peer(io);
    peer.connect(receiver.EP());
    const auto peer_address = peer.local_endpoint();

    // Send both small packets together to exercise parsing a later header in
    // the same receive buffer. Both malformed cases also run as separate tests.
    std::vector<char> bytes;
    if (subsequent) AppendPacket(bytes, remmy::PKG_MAGIC_HEAD, 17);
    AppendPacket(bytes, 0x12345678, 23);
    asio::write(peer, asio::buffer(bytes));
    if (subsequent) CheckPayload(receiver.Recv(), 17);
    const auto failure = receiver.Recv();
    CHECK(failure != nullptr);
    CHECK(failure->GetStatus() == remmy::ErrorCode::SERVER_FAIL);
    CHECK(failure->GetRemoteAddr() == peer_address);

    // The failure path must release both locks, allowing another connection
    // to communicate and both communicator destructors to finish normally.
    remmy::CommAsio healthy("127.0.0.1", 0);
    healthy.Start();
    auto message = std::make_shared<remmy::CommAsio::MessageType>();
    message->SetRemoteAddr(receiver.EP());
    remmy::Serialize(message->GetStreamBuffer(), 42);
    CHECK(healthy.Send(message) == remmy::CommErrors::SUCCESS);
    const auto received = receiver.Recv();
    CheckPayload(received, 42);
    auto reply = std::make_shared<remmy::CommAsio::MessageType>();
    reply->SetRemoteAddr(received->GetRemoteAddr());
    remmy::Serialize(reply->GetStreamBuffer(), 43);
    CHECK(receiver.Send(reply) == remmy::CommErrors::SUCCESS);
    CheckPayload(healthy.Recv(), 43);
}
