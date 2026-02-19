/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <type_traits>
#include "../internal/test.h"
#include "comb2/seqrange.h"
#include "core/sequencer.h"
#include "range.h"
#include "string.h"
#include "../basic/group.h"

namespace futils::comb2::cmdline {
    enum class ArgType {
        str_arg,
        arg,
    };

    namespace internal {
        template <class Callback>
        struct ArgCallback {
            Callback&& cb;
            size_t counter = 0;
            constexpr void end_string(Status& res, auto&& tag, auto&& seq, Pos pos) {
                if (res == Status::match) {
                    auto n = counter;
                    if constexpr (std::is_invocable_r_v<bool, decltype(cb), decltype(tag), decltype(seq), decltype(pos), decltype(n)>) {
                        if (!cb(tag, seq, pos, n)) {
                            res = Status::fatal;
                        }
                    }
                    else {
                        cb(tag, seq, pos, n);
                    }
                    counter++;
                }
            }
        };
    }  // namespace internal

    template <class T, class Callback>
    constexpr size_t command_line_callback(Sequencer<T>& input, Callback&& cb) {
        using namespace ops;
        constexpr auto space = composite::space | composite::tab | composite::eol;
        constexpr auto spaces = *space;
        constexpr auto str_arg = str(ArgType::str_arg, (composite::c_str_weak | composite::char_str_weak) & peek(space | eos));
        constexpr auto arg = str(ArgType::arg, ~(not_(space | eos) & uany));
        constexpr auto parser = spaces & *((str_arg | arg) & spaces);
        internal::ArgCallback<Callback> l{std::forward<Callback>(cb)};
        parser(input, l, cb);
        return l.counter;
    }

    template <class T, class Callback>
    constexpr auto command_line_callback(T&& input, Callback&& cb) {
        auto seq = make_ref_seq(input);
        return command_line_callback(seq, cb);
    }

    namespace test {
        constexpr bool callback_test() {
            auto assert_ = [](auto&& a, auto&& b, auto&&... arg) {
                if (!(... && arg)) {
                    comb2::test::error_if_constexpr("condition not met", a, b, arg...);
                }
            };
            if (!(command_line_callback("hello world", [&](ArgType t, auto&& seq, Pos pos, size_t i) {
                      if (i == 0) {
                          assert_(pos, t, pos.begin == 0 && pos.end == 5, t == ArgType::arg);
                      }
                      else if (i == 1) {
                          assert_(pos, t, pos.begin == 6 && pos.end == 11, t == ArgType::arg);
                      }
                      else {
                          assert_(pos, t, false);
                      }
                  }) == 2)) {
                return false;
            }
            if (!(command_line_callback("echo \"hello\\\" world\"", [&](ArgType t, auto&& seq, Pos pos, size_t i) {
                      if (i == 0) {
                          assert_(pos, t, pos.begin == 0 && pos.end == 4, t == ArgType::arg);
                      }
                      else if (i == 1) {
                          assert_(pos, t, pos.begin == 5 && pos.end == 20, t == ArgType::str_arg);
                      }
                      else {
                          assert_(pos, t, false);
                      }
                  }) == 2)) {
                return false;
            }
            return true;
        }

        constexpr auto cb_test = callback_test();
        static_assert(cb_test, "test failed");
    }  // namespace test

    template <class Buffer, class Output, class T, class Callback>
    constexpr auto command_line(Output&& output, T&& input, Callback&& escape_handler) {
        return command_line_callback(input, [&](ArgType t, auto&& seq, Pos pos, size_t i) {
            Buffer buf;
            seq_range_to_string(buf, seq, pos);
            if (t == ArgType::str_arg) {
                escape_handler(buf);
            }
            output.push_back(std::move(buf));
        });
    }

    template <class Output, class Buffer = typename Output::value_type, class T, class Callback>
    constexpr Output command_line(T&& input, Callback&& escape_handler) {
        Output output;
        command_line<Buffer>(output, input, escape_handler);
        return output;
    }

}  // namespace futils::comb2::cmdline
