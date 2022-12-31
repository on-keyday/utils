/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// cast - frame cast
#pragma once
#include "wire.h"

namespace utils {
    namespace dnet::quic::frame {
        namespace internal {
            template <class F>
            struct is_ACKFrame_t : std::false_type {};

            template <template <class...> class T>
            struct is_ACKFrame_t<ACKFrame<T>> : std::true_type {};

            template <class F>
            constexpr bool is_ACKFrame_v = is_ACKFrame_t<F>::value;

            template <template <class...> class T, class F>
            struct ReplaceACKFrameVec {
                using type = F;  // non ACK
            };

            template <template <class...> class T, template <class...> class V>
            struct ReplaceACKFrameVec<T, ACKFrame<V>> {
                using type = ACKFrame<T>;
            };

        }  // namespace internal

        template <class F>
        constexpr FrameType frame_type_of = [] {
            if constexpr (internal::is_ACKFrame_v<F>) {
                ACKFrame<quic::test::FixedTestVec> f;
                return f.get_type(true);
            }
            else {
                F f;
                return f.get_type(true);
            }
        }();

        constexpr FrameType known_frame_types[] = {
#define MAP(F) frame_type_of<F>
            MAP(PaddingFrame),
            MAP(PingFrame),
            MAP(ACKFrame<quic::test::FixedTestVec>),
            MAP(ResetStreamFrame),
            MAP(StopSendingFrame),
            MAP(CryptoFrame),
            MAP(NewTokenFrame),
            MAP(StreamFrame),
            MAP(MaxDataFrame),
            MAP(MaxStreamDataFrame),
            MAP(MaxStreamsFrame),
            MAP(DataBlockedFrame),
            MAP(StreamDataBlockedFrame),
            MAP(StreamsBlockedFrame),
            MAP(NewConnectionIDFrame),
            MAP(RetireConnectionIDFrame),
            MAP(PathChallengeFrame),
            MAP(PathResponseFrame),
            MAP(ConnectionCloseFrame),
            MAP(HandshakeDoneFrame),
            MAP(DatagramFrame),
#undef MAP
        };

        template <template <class...> class Vec>
        constexpr auto select_Frame(FrameType typ, auto&& cb) {
#define MAP(F)             \
    case frame_type_of<F>: \
        return cb(F{});
            switch (typ) {
                MAP(PaddingFrame)
                MAP(PingFrame)
                MAP(ACKFrame<Vec>)
                MAP(ResetStreamFrame)
                MAP(StopSendingFrame)
                MAP(CryptoFrame)
                MAP(NewTokenFrame)
                MAP(StreamFrame)
                MAP(MaxDataFrame)
                MAP(MaxStreamDataFrame)
                MAP(MaxStreamsFrame)
                MAP(DataBlockedFrame)
                MAP(StreamDataBlockedFrame)
                MAP(StreamsBlockedFrame)
                MAP(NewConnectionIDFrame)
                MAP(RetireConnectionIDFrame)
                MAP(PathChallengeFrame)
                MAP(PathResponseFrame)
                MAP(ConnectionCloseFrame)
                MAP(HandshakeDoneFrame)
                MAP(DatagramFrame)
                default:
                    return cb(Frame{});
            }
#undef MAP
        }

        // cast() casts Frame* to F* type
        // this uses f->type to judge type
        // this is implicitly unsafe
        // because f->type is mutable so user can change value of f->type freely
        // should apply cast() for parsed frame that f->parse() was called
        template <class F>
        constexpr F* cast(Frame* f) {
            if (!f) {
                return nullptr;
            }
            using Replace = internal::ReplaceACKFrameVec<quic::test::FixedTestVec, F>;
            return select_Frame<quic::test::FixedTestVec>(f->type.type(), [&](auto type) -> F* {
                if constexpr (std::is_same_v<decltype(type), typename Replace::type>) {
                    return static_cast<F*>(f);
                }
                return nullptr;
            });
        }

        template <class F>
        constexpr const F* cast(const Frame* f) {
            return cast<F>(const_cast<Frame*>(f));
        }

        namespace test {
            constexpr bool check_cast() {
                for (auto ty : known_frame_types) {
                    bool ok = select_Frame<quic::test::FixedTestVec>(ty, [&](auto fr) {
                        fr.type = ty;
                        return cast<decltype(fr)>(&fr) != nullptr;
                    });
                    if (!ok) {
                        return false;
                    }
                }
                return true;
            }

            static_assert(check_cast());
        }  // namespace test

    }  // namespace dnet::quic::frame
}  // namespace utils
