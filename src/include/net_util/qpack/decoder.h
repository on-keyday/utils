/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "common.h"

namespace utils {
    namespace qpack::decoder {
        enum class Instruction {
            undefined,
            section_ack,
            stream_cancel,
            increment,
        };

        constexpr byte mask(Instruction ist) {
            switch (ist) {
                case Instruction::section_ack:
                    return 0x80;
                case Instruction::stream_cancel:
                case Instruction::increment:
                    return 0xC0;
                default:
                    return 0;
            }
        }

        constexpr byte match(Instruction ist) {
            switch (ist) {
                case Instruction::section_ack:
                    return 0x80;
                case Instruction::stream_cancel:
                    return 0x40;
                case Instruction::increment:
                    return 0x00;
                default:
                    return 0xff;
            }
        }

        constexpr Instruction get_istr(byte m) {
            auto is = [&](Instruction ist) {
                return (mask(ist) & m) == match(ist);
            };
            if (is(Instruction::section_ack)) {
                return Instruction::section_ack;
            }
            else if (is(Instruction::stream_cancel)) {
                return Instruction::stream_cancel;
            }
            else if (is(Instruction::increment)) {
                return Instruction::increment;
            }
            return Instruction::undefined;
        }

        template <class Table, class T>
        constexpr QpackError render_instruction(fields::FieldDecodeContext<Table>& ctx, io::expand_writer<T>& w, Instruction istr, std::uint64_t value) {
            switch (istr) {
                default:
                    return QpackError::undefined_instruction;
                case Instruction::section_ack: {
                    if (auto err = hpack::encode_integer<7>(w, value, match(Instruction::section_ack)); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return QpackError::none;
                }
                case Instruction::stream_cancel: {
                    if (auto err = hpack::encode_integer<6>(w, value, match(Instruction::stream_cancel)); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return QpackError::none;
                }
                case Instruction::increment: {
                    if (value == 0) {
                        return QpackError::invalid_increment;
                    }
                    if (auto err = hpack::encode_integer<6>(w, value, match(Instruction::increment)); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return QpackError::none;
                }
            }
        }

        template <class Table, class C, class U>
        constexpr QpackError parse_instruction(fields::FieldEncodeContext<Table>& ctx, io::basic_reader<C, U>& r) {
            if (r.empty()) {
                return QpackError::input_length;
            }
            switch (get_istr(r.top())) {
                default:
                    return QpackError::undefined_instruction;
                case Instruction::section_ack: {
                    size_t id = 0;
                    if (auto err = hpack::decode_integer<7>(r, id); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return ctx.remove_ref(id, true);
                }
                case Instruction::stream_cancel: {
                    size_t id = 0;
                    if (auto err = hpack::decode_integer<6>(r, id); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return ctx.remove_ref(id, false);
                }
                case Instruction::increment: {
                    size_t incr = 0;
                    if (auto err = hpack::decode_integer<6>(r, incr); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return ctx.increment(incr);
                }
            }
        }

    }  // namespace qpack::decoder
}  // namespace utils
