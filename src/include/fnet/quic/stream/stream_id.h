/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <binary/flags.h>

namespace utils {
    namespace fnet::quic::stream {
        // initiated by
        enum class Origin {
            client,
            server,
            unknown,
        };

        enum class StreamType {
            bidi,
            uni,
            unknown,
        };

        constexpr Origin inverse(Origin dir) {
            switch (dir) {
                case Origin::client:
                    return Origin::server;
                case Origin::server:
                    return Origin::client;
                default:
                    return Origin::unknown;
            }
        }

        struct StreamID {
            // std::uint64_t id = ~0;

           private:
            binary::flags_t<std::uint64_t, 2, 60, 1, 1> value;
            bits_flag_alias_method(value, 0, invalid_bit);
            bits_flag_alias_method(value, 1, seq_count_raw);
            bits_flag_alias_method(value, 2, type_raw);
            bits_flag_alias_method(value, 3, origin_raw);

           public:
            constexpr StreamID() = default;

            constexpr StreamID(std::uint64_t seq, Origin orig, StreamType typ) {
                if (!set_seq_count_raw(seq)) {
                    set_invalid_bit(1);
                }
                if (orig != Origin::server && orig != Origin::client) {
                    set_invalid_bit(1);
                }
                else {
                    set_origin_raw(orig == Origin::server);
                }
                if (typ != StreamType::bidi && typ != StreamType::uni) {
                    set_invalid_bit(1);
                }
                else {
                    set_type_raw(typ == StreamType::uni);
                }
            }

            constexpr StreamID(std::uint64_t id)
                : value(id) {}

            constexpr bool valid() const noexcept {
                return invalid_bit() != 0;
            }

            constexpr operator std::uint64_t() const {
                return value.as_value();
            }

            constexpr std::uint64_t seq_count() const {
                if (!valid()) {
                    return ~0;
                }
                return seq_count_raw();
            }

            constexpr StreamType type() const {
                if (!valid()) {
                    return StreamType::unknown;
                }
                return type_raw() ? StreamType::uni : StreamType::bidi;
            }

            constexpr Origin origin() const {
                if (!valid()) {
                    return Origin::unknown;
                }
                return origin_raw() ? Origin::server : Origin::client;
            }

            constexpr std::uint64_t to_int() const noexcept {
                return value.as_value();
            }
        };

        constexpr StreamID make_id(std::uint64_t seq, Origin dir, StreamType type) noexcept {
            return StreamID{seq, dir, type};
        }

        constexpr StreamID invalid_id = ~0;
    }  // namespace fnet::quic::stream
}  // namespace utils
