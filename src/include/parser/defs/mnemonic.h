/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// mnemonic - parser's command mnemonic
#pragma once
#include "../../core/sequencer.h"

namespace utils {
    namespace parser {
        namespace mnemonic {
            enum class Command {
                eq,
                if_,
                loop,
                least,
                const_,
                do_,
                else_,
                LastIndex,
                not_found,
            };

            constexpr char* mnemonics[int(Command::LastIndex)] = {
                "eq",
                "if",
                "loop",
                "least",
                "const",
                "do",
                "else",
            };

            template <class T>
            Command consume(Sequencer<T>& seq) {
                helper::space::consume_space(seq, true);
                size_t start = seq.rptr;
                for (int i = 0; i < int(Command::LastIndex); i++) {
                    if (seq.seek_if(mnemonics[i])) {
                        if (!helper::space::consume_space(seq, true)) {
                            seq.rptr = start;
                            return Command::not_found;
                        }
                        return Command(i);
                    }
                }
                return Command::not_found;
            }

        }  // namespace mnemonic
    }      // namespace parser
}  // namespace utils
