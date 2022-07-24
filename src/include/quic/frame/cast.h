/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// cast - type cast helper
#pragma once
#include "types.h"
#include <type_traits>

namespace utils {
    namespace quic {
        namespace frame {
            template <class T>
            T* cast(Frame* f) {
                if (!f) {
                    return nullptr;
                }
#define CAST_CORE(TYPE)                      \
    if constexpr (std::is_same_v<T, TYPE>) { \
        return static_cast<T*>(f);           \
    }                                        \
    else {                                   \
        return nullptr;                      \
    }
#define CAST(TYPE, CODE) \
    case CODE {          \
        CAST_CORE(TYPE)  \
    }
                constexpr auto s0 = byte(types::STREAM);
                constexpr auto s7 = s0 + 0x7;
                auto ty = byte(f->type);
                if (s0 <= ty && ty <= s7) {
                    CAST_CORE(Stream)
                }
                switch (f->type) {
                    CAST(Padding, types::PADDING)
                    CAST(Ping, types::PING)
                    CAST(Ack, types::ACK)
                    CAST(ResetStream, types::RESET_STREAM)
                    CAST(StopSending, types::STOP_SENDING)
                    CAST(Crypto, types::CRYPTO)
                    CAST(NewToken, types::NEW_TOKEN)
                    CAST(MaxData, types::MAX_DATA)
                    CAST(MaxStreamData, types::MAX_STREAM_DATA)
                    CAST(MaxStreams, types::MAX_STREAMS_BIDI)
                    CAST(MaxStreams, types::MAX_STREAMS_UNI)
                    CAST(DataBlocked, types::DATA_BLOCKED)
                    CAST(StreamDataBlocked, types::STREAM_DATA_BLOCKED)
                    CAST(StreamsBlocked, types::STREAMS_BLOCKED_BIDI)
                    CAST(StreamsBlocked, types::STREAMS_BLOCKED_UNI)
                    CAST(NewConnectionID, types::NEW_CONNECTION_ID)
                    CAST(RetireConnectionID, types::RETIRE_CONNECTION_ID)
                    CAST(PathChallange, types::PATH_CHALLAGE)
                    CAST(PathResponse, types::PATH_RESPONSE)
                    CAST(ConnectionClose, types::CONNECTION_CLOSE)
                    CAST(ConnectionClose, types::CONNECTION_CLOSE_APP)
                    CAST(HandshakeDone, types::HANDSHAKE_DONE)
                    CAST(Datagram, types::DATAGRAM)
                    CAST(Datagram, types::DATAGRAM_LEN)
                    default:
                        return nullptr;
                }
#undef CAST
#undef CAST_CORE
            }

            template <types t>
            struct type_select {
                using type = Frame*;
                static constexpr types code = t;
            };

            template <class T>
            struct code_select {
                using type = T*;
                static constexpr types code[] = {};
            };

#define CAST(TYPE, CODE)                    \
    template <>                             \
    struct type_select<CODE> {              \
        using type = TYPE*;                 \
        static constexpr types code = CODE; \
    };

            template <byte off>
            constexpr auto strm = types(byte(types::STREAM) + off);
            CAST(Padding, types::PADDING)
            CAST(Ping, types::PING)
            CAST(Ack, types::ACK)
            CAST(Ack, types::ACK_ECN)
            CAST(ResetStream, types::RESET_STREAM)
            CAST(StopSending, types::STOP_SENDING)
            CAST(Crypto, types::CRYPTO)
            CAST(NewToken, types::NEW_TOKEN)
            CAST(Stream, strm<0>)
            CAST(Stream, strm<1>)
            CAST(Stream, strm<2>)
            CAST(Stream, strm<3>)
            CAST(Stream, strm<4>)
            CAST(Stream, strm<5>)
            CAST(Stream, strm<6>)
            CAST(Stream, strm<7>)
            CAST(MaxData, types::MAX_DATA)
            CAST(MaxStreamData, types::MAX_STREAM_DATA)
            CAST(MaxStreams, types::MAX_STREAMS_BIDI)
            CAST(MaxStreams, types::MAX_STREAMS_UNI)
            CAST(DataBlocked, types::DATA_BLOCKED)
            CAST(StreamDataBlocked, types::STREAM_DATA_BLOCKED)
            CAST(StreamsBlocked, types::STREAMS_BLOCKED_BIDI)
            CAST(StreamsBlocked, types::STREAMS_BLOCKED_UNI)
            CAST(NewConnectionID, types::NEW_CONNECTION_ID)
            CAST(RetireConnectionID, types::RETIRE_CONNECTION_ID)
            CAST(PathChallange, types::PATH_CHALLAGE)
            CAST(PathResponse, types::PATH_RESPONSE)
            CAST(ConnectionClose, types::CONNECTION_CLOSE)
            CAST(ConnectionClose, types::CONNECTION_CLOSE_APP)
            CAST(HandshakeDone, types::HANDSHAKE_DONE)
            CAST(Datagram, types::DATAGRAM)
            CAST(Datagram, types::DATAGRAM_LEN)
#undef CAST
#define CAST(TYPE, ...)                                \
    template <>                                        \
    struct code_select<TYPE> {                         \
        using type = TYPE*;                            \
        static constexpr types code[] = {__VA_ARGS__}; \
    };

            CAST(Padding, types::PADDING)
            CAST(Ping, types::PING)
            CAST(Ack, types::ACK, types::ACK_ECN)
            CAST(ResetStream, types::RESET_STREAM)
            CAST(StopSending, types::STOP_SENDING)
            CAST(Crypto, types::CRYPTO)
            CAST(NewToken, types::NEW_TOKEN)
            CAST(Stream, strm<0>, strm<1>, strm<2>, strm<3>, strm<4>, strm<5>, strm<6>, strm<7>)
            CAST(MaxData, types::MAX_DATA)
            CAST(MaxStreamData, types::MAX_STREAM_DATA)
            CAST(MaxStreams, types::MAX_STREAMS_BIDI, types::MAX_STREAMS_UNI)
            CAST(DataBlocked, types::DATA_BLOCKED)
            CAST(StreamDataBlocked, types::STREAM_DATA_BLOCKED)
            CAST(StreamsBlocked, types::STREAMS_BLOCKED_BIDI, types::STREAMS_BLOCKED_UNI)
            CAST(NewConnectionID, types::NEW_CONNECTION_ID)
            CAST(RetireConnectionID, types::RETIRE_CONNECTION_ID)
            CAST(PathChallange, types::PATH_CHALLAGE)
            CAST(PathResponse, types::PATH_RESPONSE)
            CAST(ConnectionClose, types::CONNECTION_CLOSE, types::CONNECTION_CLOSE_APP)
            CAST(HandshakeDone, types::HANDSHAKE_DONE)
            CAST(Datagram, types::DATAGRAM, types::DATAGRAM_LEN)
#undef CAST
            template <class T>
            bool include(types t) {
                for (auto cmp : code_select<T>::code) {
                    if (cmp == t) {
                        return true;
                    }
                }
                return false;
            }

            template <class T>
            auto if_(frame::Frame* f, auto&& then) {
                if (!f || !include<T>(f->type)) {
                    using ret = decltype(then((T*)nullptr));
                    if constexpr (std::is_same_v<ret, void>) {
                        return (void)0;
                    }
                    else {
                        return ret{};
                    }
                }
                return then(static_cast<T*>(f));
            }

            namespace internal_template {

                template <class T, class R, class F>
                F get_(R (T::*)(F*));
                template <class T, class R, class F>
                F get_(R (T::*)(F*) const);

                template <class T>
                struct FuncTraits {
                    using F = decltype(get_(&T::operator()));
                };

                template <class Ret, class Frame>
                struct FuncTraits<Ret (*)(Frame*)> {
                    using F = Frame;
                };

            }  // namespace internal_template

            auto if_(frame::Frame* f, auto&& then) {
                using then_t = std::remove_cvref_t<decltype(then)>;
                using F = typename internal_template::FuncTraits<then_t>::F;
                return if_<F>(f, then);
            }

            template <types type>
            auto frame() {
                using T = std::remove_pointer_t<typename type_select<type>::type>;
                return T{type};
            }
        }  // namespace frame
    }      // namespace quic
}  // namespace utils
