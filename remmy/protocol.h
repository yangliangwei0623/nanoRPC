#pragma once
#include <cstdint>
#include <functional>
#include <tuple>
#include <iostream>
#include "serialize.h"
#include "streambuffer.h"

namespace remmy {
    class ProtocolBase {
    public:
        virtual ~ProtocolBase() {}

        virtual uint64_t UniqueId() = 0;
        
        virtual void MarshallRequest(StreamBuffer &) = 0;

        virtual void MarshallResponse(StreamBuffer &) = 0;

        virtual void UnmarshallRequest(StreamBuffer &) = 0;

        virtual void UnmarshallResponse(StreamBuffer &) = 0;

        virtual void HandleRequest(void *server) = 0;
    };

    template<uint64_t UID>
    class ProtocolWithUID : public ProtocolBase {
    public:
        virtual uint64_t UniqueId() override {
            return UID;
        }

        virtual void MarshallRequest(StreamBuffer &) = 0;

        virtual void MarshallResponse(StreamBuffer &) = 0;

        virtual void UnmarshallRequest(StreamBuffer &) = 0;

        virtual void UnmarshallResponse(StreamBuffer &) = 0;

        virtual void HandleRequest(void *server) = 0;
    };

    template<class RequestT, class ResponseT>
    class ProtocolTemplate : public ProtocolBase {
    public:
        RequestT request;
        ResponseT response;

        virtual void MarshallRequest(StreamBuffer & buf) override {
            Serialize(buf, request);
        }

        void MarshallResponse(StreamBuffer & buf) override {
            Serialize(buf, response);
        }

        virtual void UnmarshallRequest(StreamBuffer & buf) override {
            Deserialize(buf, request);
        }

        virtual void UnmarshallResponse(StreamBuffer & buf) override {
            Deserialize(buf, response);
        }
    };

    template<class RequestT>
    class ProtocolTemplate<RequestT, void> : public ProtocolBase {
    public:
        RequestT request;

        virtual void MarshallRequest(StreamBuffer & buf) override {
            Serialize(buf, request);
        }

        void MarshallResponse(StreamBuffer & buf) override {
        }

        virtual void UnmarshallRequest(StreamBuffer & buf) override {
            Deserialize(buf, request);
        }

        virtual void UnmarshallResponse(StreamBuffer & buf) override {
        }
    };

	template<uint64_t UID, class RequestT, class ResponseT = void>
	class FunctorProtocol : public ProtocolBase {
	public:
		RequestT request;
		ResponseT response;

		virtual uint64_t UniqueId() {
			return UID;
		}

		virtual void MarshallRequest(StreamBuffer& buf) {
			Serialize(buf, request);
		}

		virtual void MarshallResponse(StreamBuffer& buf) {
			Serialize(buf, response);
		}

		virtual void UnmarshallRequest(StreamBuffer& buf) {
			Deserialize(buf, request);
		}

		virtual void UnmarshallResponse(StreamBuffer& buf) {
			Deserialize(buf, response);
		}

		virtual void HandleRequest(void* functor) {
			std::function<ResponseT(const RequestT& req)>* f 
				= static_cast<std::function<ResponseT(const RequestT& req)>*>(functor);
			response = (*f)(request);
		}
	};

	template<uint64_t UID, class RequestT>
	class FunctorProtocol<UID, RequestT, void> : public ProtocolBase {
	public:
		RequestT request;

		virtual uint64_t UniqueId() {
			return UID;
		}

		virtual void MarshallRequest(StreamBuffer& buf) {
			Serialize(buf, request);
		}

		virtual void MarshallResponse(StreamBuffer& buf) {}

		virtual void UnmarshallRequest(StreamBuffer& buf) {
			Deserialize(buf, request);
		}

		virtual void UnmarshallResponse(StreamBuffer &) {}

		virtual void HandleRequest(void* functor) {
			auto* f = static_cast<std::function<void(const RequestT&)>*>(functor);
			(*f)(request);
		}
	};

    template<uint64_t UID, typename ResponseT, typename... RequestTs>
    class SyncProtocol : public ProtocolBase {
        using FuncT = std::function<ResponseT(RequestTs&...)>;
    public:
        ResponseT response;
        std::tuple<RequestTs...> request;

        virtual uint64_t UniqueId() {
            return UID;
        }

        virtual void MarshallRequest(StreamBuffer& buf) {
            SerializeVariadic(buf, request);
        }

        virtual void MarshallResponse(StreamBuffer& buf) {
            Serialize(buf, response);
        }

        virtual void UnmarshallRequest(StreamBuffer& buf) {
            DeserializeVariadic(buf, request);
        }

        virtual void UnmarshallResponse(StreamBuffer& buf) {
            Deserialize(buf, response);
        }

        virtual void HandleRequest(void* functor) {
            FuncT* f = (FuncT*)(functor);
            response = std::apply(*f, request);
        }
    };

    template<uint64_t UID, typename... RequestTs>
    class AsyncProtocol : public ProtocolBase {
        using FuncT = std::function<void(RequestTs&...)>;
    public:
        std::tuple<RequestTs...> request;

        virtual uint64_t UniqueId() {
            return UID;
        }

        // marshall request is done directly in RPCCall
        virtual void MarshallRequest(StreamBuffer& buf) {}

        virtual void MarshallResponse(StreamBuffer& buf) {}

        virtual void UnmarshallRequest(StreamBuffer& buf) {
            DeserializeVariadic(buf, request);
        }

        virtual void UnmarshallResponse(StreamBuffer& buf) {}

        virtual void HandleRequest(void* functor) {
            FuncT* f = (FuncT*)(functor);
            std::apply(*f, request);
        }
    };

    template<typename... RequestTs>
    class AsyncProtocolReplaceable : public ProtocolBase {
        using FuncT = std::function<void(RequestTs&...)>;
        size_t UID;
        FuncT func;
    public:

        AsyncProtocolReplaceable(size_t uid, FuncT func) : UID(uid), func(func) {}

        std::tuple<RequestTs...> request;

        virtual uint64_t UniqueId() {
            return UID;
        }

        // marshall request is done directly in RPCCall
        virtual void MarshallRequest(StreamBuffer& buf) {}

        virtual void MarshallResponse(StreamBuffer& buf) {}

        virtual void UnmarshallRequest(StreamBuffer& buf) {
            DeserializeVariadic(buf, request);
        }

        virtual void UnmarshallResponse(StreamBuffer& buf) {}

        virtual void HandleRequest(void* functor) {
            std::apply(func, request);
        }
    };
};