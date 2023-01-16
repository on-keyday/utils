/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../middle/middle.h"
namespace minlc {
    namespace llvm {
        struct L {
            middle::M& m;  // ref to m
            std::string output;
            size_t seqnum = 0;

            void write(auto&&... args) {
                utils::helper::appends(output, args...);
            }

            auto back() {
                return output.back();
            }

            void writeln(auto&&... args) {
                write(args..., "\n");
            }

            void ln() {
                write("\n");
            }

            std::string yield_seq() {
                auto res = utils::number::to_string<std::string>(seqnum);
                seqnum++;
                return res;
            }
        };

        bool write_program(L& l, const std::string& input, sptr<middle::Program>& prog);
    }  // namespace llvm
}  // namespace minlc
