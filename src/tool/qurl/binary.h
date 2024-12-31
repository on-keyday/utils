/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>
#include <binary/flags.h>

namespace qurl {
    namespace raw {
        namespace binary = futils::binary;
        using byte = futils::byte;
        enum class Instruction : futils::byte {
            // undefined operation
            UNDEF,
            // stack control
            PUSH,
            POP,
            RESOLVE,
            // function call/return
            CALL,
            RET,
            RET_NONE,
            // operation
            ADD,
            SUB,
            MUL,
            DIV,
            MOD,
            AND,
            OR,
            EQ,
            NEQ,
            // variable scope control
            ENTER,
            LEAVE,
            ALLOC,
            ASSIGN,
            // flow control
            JMPIF,
            LABEL,
            // reserved for two byte instruction
            RESERVED,
        };

        enum class RepFlag {
            imm,
            addr,
            reg,
        };

        struct Arg {
            futils::byte arg[16];
            bool number(byte n_byte, std::uint64_t value, RepFlag rep) {
                binary::flags_t<byte, 2, 2, 1, 3> flag;
                if (n_byte != 1 && n_byte != 2 && n_byte != 4 && n_byte != 8) {
                    return false;
                }
                switch (n_byte) {
                    case 1:
                        n_byte = 0;
                    case 2:
                        n_byte = 1;
                    case 4:
                        n_byte = 2;
                    case 8:
                        n_byte = 3;
                }
                if (!flag.set<0>(n_byte) ||
                    !flag.set<1>(rep)) {
                    return false;
                }
                if (value < 8) {
                    flag.set<2>(true);
                    flag.set<3>(value);
                }
                else {
                    flag.set<2>(false);
                    flag.set<3>(1);
                }
            }
        };
    }  // namespace raw
}  // namespace qurl
