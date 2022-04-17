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
                consume,
                require,
                any,
                bindany,
                bind,
                if_,

                LastIndex,
                not_found,
            };

            struct Mnemonic {
                const char* str;
                size_t exprcount;
            };

            constexpr Mnemonic mnemonics[int(Command::LastIndex)] = {
                {"consume", 1},
                {"require", 1},
                {"any", 1},
                {"bindany", 2},
                {"bind", 2},
                {"if", 2},
            };

            template <class T>
            Command consume(Sequencer<T>& seq, size_t& pos, int i = -1) {
                size_t start = seq.rptr;
                helper::space::consume_space(seq, true);
                pos = seq.rptr;
                if (i < 0) {
                    for (i = 0; i < int(Command::LastIndex); i++) {
                        if (seq.seek_if(mnemonics[i].str)) {
                            if (!helper::space::consume_space(seq, true)) {
                                seq.rptr = start;
                                return Command::not_found;
                            }
                            return Command(i);
                        }
                    }
                }
                else {
                    if (i >= int(Command::LastIndex)) {
                        return Command::not_found;
                    }
                    if (seq.seek_if(mnemonics[i].str)) {
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
