#include "remmy/serialize.h"
#include "remmy/unique_id.h"
#include "check.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <list>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

using remmy::StreamBuffer;
using remmy::SimpleLogger;

struct ConstMembers {
    void Serialize(StreamBuffer&) const;
    void Deserialize(StreamBuffer&);
};
struct NonConstSerialize {
    void Serialize(StreamBuffer&);
    void Deserialize(StreamBuffer&);
};
struct ConstDeserialize {
    void Serialize(StreamBuffer&) const;
    void Deserialize(StreamBuffer&) const;
};
struct OverloadedMembers {
    void Serialize(StreamBuffer&);
    void Serialize(StreamBuffer&) const;
    void Serialize(int) const;
    void Deserialize(StreamBuffer&);
    void Deserialize(int);
};
struct WrongReturn {
    int Serialize(StreamBuffer&) const;
    void Deserialize(StreamBuffer&);
};
struct WrongDeserializeReturn {
    void Serialize(StreamBuffer&) const;
    int Deserialize(StreamBuffer&);
};
struct WrongArgument {
    void Serialize(const StreamBuffer&) const;
    void Deserialize(StreamBuffer&);
};
struct StaticMembers {
    static void Serialize(StreamBuffer&);
    static void Deserialize(StreamBuffer&);
};
struct MissingDeserialize {
    void Serialize(StreamBuffer&) const;
};
struct InheritedMembers : ConstMembers {};
struct NonTrivial {
    ~NonTrivial() {}
};

static_assert(remmy::_has_serialize<ConstMembers>::value);
static_assert(remmy::_has_serialize<OverloadedMembers>::value);
static_assert(remmy::_has_serialize<InheritedMembers>::value);
static_assert(!remmy::_has_serialize<NonConstSerialize>::value);
static_assert(!remmy::_has_serialize<ConstDeserialize>::value);
static_assert(!remmy::_has_serialize<WrongReturn>::value);
static_assert(!remmy::_has_serialize<WrongDeserializeReturn>::value);
static_assert(!remmy::_has_serialize<WrongArgument>::value);
static_assert(!remmy::_has_serialize<StaticMembers>::value);
static_assert(!remmy::_has_serialize<MissingDeserialize>::value);
static_assert(!remmy::_has_serialize<int>::value);
static_assert(!remmy::_has_serialize<int*>::value);
static_assert(remmy::_has_serialize_<ConstMembers, void (ConstMembers::*)(StreamBuffer&) const>::value);
static_assert(remmy::_has_deserialize_<ConstMembers, void (ConstMembers::*)(StreamBuffer&)>::value);
static_assert(remmy::TriviallyCopyable<int>::value);
static_assert(remmy::_should_do_memcpy<int>::value);
static_assert(!remmy::_should_do_memcpy<int*>::value);
static_assert(!remmy::_should_do_memcpy<NonTrivial>::value);

// Trivially copyable objects must still prefer custom serialization.
struct CustomTrivial {
    int value = 0;
    void Serialize(StreamBuffer& buf) const { remmy::Serialize(buf, value + 7); }
    void Deserialize(StreamBuffer& buf) { remmy::Deserialize(buf, value); value -= 7; }
    bool operator==(const CustomTrivial&) const = default;
};
static_assert(std::is_trivially_copyable_v<CustomTrivial>);
static_assert(!remmy::_should_do_memcpy<CustomTrivial>::value);

struct Custom {
    int number;
    double real;
    std::string text;
    void Serialize(StreamBuffer& buf) const { remmy::SerializeVariadic(buf, number, real, text); }
    void Deserialize(StreamBuffer& buf) {
        auto fields = std::tie(number, real, text);
        remmy::DeserializeVariadic(buf, fields);
    }
    bool operator==(const Custom&) const = default;
};

template<typename T>
void RoundTrip(const T& value) {
    StreamBuffer buf;
    remmy::Serialize(buf, value);
    T decoded{};
    remmy::Deserialize(buf, decoded);
    CHECK(decoded == value);
    CHECK(buf.GetSize() == 0);
}

// Independent reference for the existing native-byte, length-prefixed format.
template<typename T>
void AppendRaw(std::vector<char>& bytes, const T& value) {
    const char* first = reinterpret_cast<const char*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}
void AppendString(std::vector<char>& bytes, const std::string& value) {
    AppendRaw(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}
void CheckBytes(StreamBuffer& buf, const std::vector<char>& expected) {
    CHECK(buf.GetSize() == expected.size());
    if (!expected.empty()) CHECK(std::memcmp(buf.GetBuf(), expected.data(), expected.size()) == 0);
}

static_assert(remmy::UniqueId("") == 15);
static_assert(remmy::UniqueId("0") == 961);
static_assert(remmy::UniqueId("A") == 971);
static_assert(remmy::UniqueId("add") == 4090473);
static_assert(remmy::UniqueId("0123456789") == 0xF0420C41461C824AULL);

int main() {
    RoundTrip(42);
    RoundTrip(1.5);
    RoundTrip(std::string("hello\0world", 11));
    RoundTrip(std::string{});
    RoundTrip(CustomTrivial{13});
    RoundTrip(Custom{23, 2.5, "fields"});
    RoundTrip(std::vector<int>{1, 2, 3});
    RoundTrip(std::vector<int>{});
    RoundTrip(std::vector<std::string>{"one", "", "three"});
    RoundTrip(std::vector<CustomTrivial>{{1}, {2}});
    RoundTrip(std::list<std::string>{"a", "b"});
    RoundTrip(std::deque<int>{3, 2, 1});
    RoundTrip(std::pair<int, std::string>{5, "pair"});
    RoundTrip(std::tuple<>{});
    RoundTrip(std::tuple{7, std::tuple{std::string("nested"), 1.5}, std::tuple<>{}});
    RoundTrip(std::vector<std::tuple<int, std::string>>{{1, "tuple"}, {2, "container"}});

    StreamBuffer empty;
    remmy::SerializeVariadic(empty);
    auto no_fields = std::tuple{};
    remmy::DeserializeVariadic(empty, no_fields);
    CHECK(empty.GetSize() == 0);

    const uint32_t number = 0x12345678;
    const std::string text = "wire";
    const std::vector<int> numbers{2, 4, 6};
    const Custom custom{9, 1.25, "custom"};
    std::vector<char> expected;
    AppendRaw(expected, number);
    AppendString(expected, text);
    AppendRaw(expected, numbers.size());
    for (const int item : numbers) AppendRaw(expected, item);
    AppendRaw(expected, custom.number);
    AppendRaw(expected, custom.real);
    AppendString(expected, custom.text);
    AppendRaw(expected, 20); // CustomTrivial{13} encodes 13 + 7.

    StreamBuffer variadic;
    remmy::SerializeVariadic(variadic, number, text, numbers, custom, CustomTrivial{13});
    CheckBytes(variadic, expected);
    StreamBuffer tuple;
    remmy::Serialize(tuple, std::tuple{number, std::tuple{text, numbers}, custom, CustomTrivial{13}});
    CheckBytes(tuple, expected);

    StreamBuffer explicit_template;
    remmy::Serializer<int>::Serialize<int>(explicit_template, 42);
    int restored = 0;
    remmy::Serializer<int>::Deserialize<int>(explicit_template, restored);
    CHECK(restored == 42);

    char runtime_id[] = "RPC_Proto";
    CHECK(remmy::UniqueId(runtime_id) == remmy::UniqueId("RPC_Proto"));
    CHECK(remmy::DecodeUniqueId(remmy::UniqueId(runtime_id)) == runtime_id);
    CHECK(remmy::UniqueId("a!b") == remmy::UniqueId("a b"));
    CHECK(remmy::UniqueId("a\0ignored") == remmy::UniqueId("a"));

    int evaluated = 0;
    REMMY_LOG("disabled %d", ++evaluated);
    CHECK(evaluated == 0);
    REMMY_LOG("no arguments");
    REMMY_WARN("no arguments");
    REMMY_WARN("one argument: %d", 42);
    REMMY_ASSERT(true, "no arguments");
    REMMY_ASSERT(true, "one argument: %d", 42);
    if (false) {
        REMMY_ABORT("no arguments");
        SIMPLE_SUICIDE("no arguments");
    }
}
