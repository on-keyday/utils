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

    }  // namespace qpack::decoder
}  // namespace utils
