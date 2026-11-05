#pragma once

#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "logging.h"
#include "streambuffer.h"

namespace remmy {

    template<typename T>
    struct TriviallyCopyable : std::is_trivially_copyable<T> {};

    template<typename T, typename Sign>
    struct _has_serialize_ : std::bool_constant<requires {
        static_cast<Sign>(&T::Serialize);
    }> {};

    template<typename T, typename Sign>
    struct _has_deserialize_ : std::bool_constant<requires {
        static_cast<Sign>(&T::Deserialize);
    }> {};

    // Serialization reads a const object; deserialization modifies it.
    template<typename T>
    concept HasSerializationMembers = requires {
        static_cast<void (T::*)(StreamBuffer&) const>(&T::Serialize);
        static_cast<void (T::*)(StreamBuffer&)>(&T::Deserialize);
    };

    template<typename T, bool isClass = std::is_class_v<T>>
    struct _has_serialize : std::bool_constant<isClass && HasSerializationMembers<T>> {};

    #define ENABLE_IF_HAS_SERIALIZE(T, RT) typename std::enable_if<_has_serialize<T>::value, RT>::type

    template<typename T>
    struct _should_do_memcpy : std::bool_constant<
        std::is_trivially_copyable_v<T> && !std::is_pointer_v<T> && !_has_serialize<T>::value> {};

    namespace _detail {
        template<typename>
        inline constexpr bool unsupported_serialization = false;
    }

    template<typename T>
    class Serializer {
    public:
        template<typename T2 = T>
        static void Serialize(StreamBuffer& buf, const T2& val) {
            if constexpr (_has_serialize<T2>::value) {
                val.Serialize(buf);
            }
            else if constexpr (_should_do_memcpy<T2>::value) {
                buf.Write(&val, sizeof(T));
            }
            else {
                static_assert(_detail::unsupported_serialization<T2>,
                    "Serialize not defined for this type. Define void Serialize(StreamBuffer&) const "
                    "and void Deserialize(StreamBuffer&), or specialize Serializer<T>.");
            }
        }

        template<typename T2 = T>
        static void Deserialize(StreamBuffer& buf, T2& val) {
            if constexpr (_has_serialize<T2>::value) {
                val.Deserialize(buf);
            }
            else if constexpr (_should_do_memcpy<T2>::value) {
                buf.Read(&val, sizeof(T));
            }
            else {
                static_assert(_detail::unsupported_serialization<T2>,
                    "Deserialize not defined for this type. Define void Serialize(StreamBuffer&) const "
                    "and void Deserialize(StreamBuffer&), or specialize Serializer<T>.");
            }
        }
    };

    // Make tuple overloads visible to container and variadic helpers at definition time.
    template<typename... Ts>
    inline void Serialize(StreamBuffer& buf, const std::tuple<Ts...>& tup);

    template<typename... Ts>
    inline void Deserialize(StreamBuffer& buf, std::tuple<Ts...>& tup);

    // partial specialization for pair
    template<typename T1, typename T2>
    class Serializer <std::pair<T1, T2>> {
    public:
        static void Serialize(StreamBuffer & buf, const std::pair<T1, T2> & val) {
            Serializer<T1>::Serialize(buf, val.first);
            Serializer<T2>::Serialize(buf, val.second);
        }
        static void Deserialize(StreamBuffer & buf, std::pair<T1, T2> & val) {
            Serializer<T1>::Deserialize(buf, val.first);
            Serializer<T2>::Deserialize(buf, val.second);
        }
    };

    template<typename T>
    inline void Serialize(StreamBuffer & buf, const T & val) {
        Serializer<T>::Serialize(buf, val);
    }

    template<class T>
    inline void Deserialize(StreamBuffer & buf, T & val) {
        Serializer<T>::Deserialize(buf, val);
    }

    // ------------------------------
    // specially for vector
    // If T is not trivially copyable, we must copy them one-by-one
    // If T is trivially copyable, we copy the whole vector at once
    template<typename ContainerT, typename T, 
        bool ONE_BY_ONE = true>
    class ContainerSerializer {
    public:
        static void Serialize(StreamBuffer& buf, const ContainerT& vec) {
            size_t size = vec.size();
            buf.Write(&size, sizeof(size));
            for (auto & iter : vec) {
                remmy::Serialize(buf, iter);
            }
        }
        static void Deserialize(StreamBuffer& buf, ContainerT& vec) {
            size_t size;
            buf.Read(&size, sizeof(size));
            vec.resize(size);
            for (auto & iter : vec) {
                remmy::Deserialize(buf, iter);
            }             
        }
    };

    template<typename ContainerT, typename T>
    class ContainerSerializer <ContainerT, T, false> {
    public:
        static void Serialize(StreamBuffer& buf, const ContainerT& vec) {
            size_t size = vec.size();
            buf.Write(&size, sizeof(size));
            if (!vec.empty()) {
                buf.Write(&vec[0], sizeof(T)*vec.size());
            }
        }
        static void Deserialize(StreamBuffer& buf, ContainerT& vec) {
            size_t size;
            buf.Read(&size, sizeof(size));
            vec.resize(size);
            if (!vec.empty()) {
                buf.Read(&vec[0], sizeof(T)*size);
            }
        }
    };

    template<typename T>
    class Serializer<std::vector<T>>
        : public ContainerSerializer<std::vector<T>, T, !_should_do_memcpy<T>::value> {};

    template<>
    class Serializer<std::string>
        : public ContainerSerializer<std::string, char, false> {};

    template<typename T>
    class Serializer<std::deque<T>>
        : public ContainerSerializer<std::deque<T>, T, true> {};

    template<typename T>
    class Serializer<std::list<T>>
        : public ContainerSerializer<std::list<T>, T, true> {};

    template<typename T>
    class Serializer<std::set<T>> : public ContainerSerializer<std::set<T>, T, true> {};

    template<typename T>
    class Serializer<std::unordered_set<T>> : public ContainerSerializer<std::unordered_set<T>, T, true> {};

    template<typename K, typename V>
    class Serializer<std::map<K, V>> : public ContainerSerializer<std::map<K, V>, std::pair<K, V>, true> {};

    template<typename K, typename V>
    class Serializer<std::unordered_map<K, V>> : public ContainerSerializer<std::unordered_map<K, V>, std::pair<K, V>, true> {};

    
    template<typename... Ts>
    inline void SerializeVariadic(StreamBuffer& buf, const Ts&... values) {
        (Serialize(buf, values), ...);
    }

    template<typename... Ts>
    inline void Serialize(StreamBuffer& buf, const std::tuple<Ts...>& tup) {
        std::apply([&buf](const auto&... values) {
            (Serialize(buf, values), ...);
        }, tup);
    }

    template<typename... Ts>
    inline void Deserialize(StreamBuffer& buf, std::tuple<Ts...>& tup) {
        std::apply([&buf](auto&... values) {
            (Deserialize(buf, values), ...);
        }, tup);
    }

    template<typename... Ts>
    inline void DeserializeVariadic(StreamBuffer& buf, std::tuple<Ts...>& tup) {
        Deserialize(buf, tup);
    }
}
