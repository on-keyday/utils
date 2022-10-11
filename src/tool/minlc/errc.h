/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/cout.h>
#include <minilang/old/minl.h>
namespace minlc {
    void printe(auto&&... msg) {
        (utils::wrap::cerr_wrap() << ... << msg) << "\n";
    }

    void printo(auto&&... msg) {
        (utils::wrap::cout_wrap() << ... << msg) << "\n";
    }

    void print_node(const std::shared_ptr<utils::minilang::MinNode>&);
    void print_pos(size_t pos);

    struct ErrC {
        void say(auto&&... msg) {
            printe(msg...);
        }
        void trace(auto start, auto& seq) {
            print_pos(seq.rptr);
        }
        void node(const auto& node) {
            print_node(node);
        }
        size_t index = 0;
        size_t current = 0;
        bool out_log = false;
        void log_enter(const char* func, const char* file, int line) {
            if (out_log) {
                printe(index, ",", current, ",enter,", func, ",", file, ":", line);
            }
            current++;
            index++;
        }
        void log_leave(const char* func, const char* file, int line) {
            current--;
            if (out_log) {
                printe(index, ",", current, ",leave,", func, ",", file, ":", line);
            }
            index++;
        }
    };
}  // namespace minlc
