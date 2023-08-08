/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../basic/literal.h"
#include "../basic/peek.h"
#include "../basic/proxy.h"
#include "../basic/logic.h"
#include "range.h"

namespace utils::comb2::composite {
    template <class Begin, class Inner, class End, class MustMatchError = types::MustMatchErrorFn>
    constexpr auto comment(Begin&& b, Inner&& in, End&& e, bool nest = false, MustMatchError&& err = MustMatchError{}) {
        return ops::proxy([=](auto&& seq, auto&& ctx, auto&& rec) {
            if (Status m = b(seq, ctx, rec); m != Status::match) {
                return m;
            }
            size_t nest_p = 1;
            while (nest_p) {
                const size_t ptr = seq.rptr;
                if (Status m = e(seq, ctx, rec); m == Status::match) {
                    nest_p--;
                    continue;
                }
                else if (m == Status::fatal) {
                    return m;
                }
                seq.rptr = ptr;
                if (nest) {
                    if (Status m = b(seq, ctx, rec); m == Status::match) {
                        nest_p++;
                        continue;
                    }
                    else if (m == Status::fatal) {
                        return m;
                    }
                }
                seq.rptr = ptr;
                if (Status m = in(seq, ctx, rec); m != Status::match) {
                    return m;
                }
            }
            return Status::match;
        },
                          std::forward<decltype(err)>(err));
    }
    using namespace ops;
    constexpr auto c_comment = comment(ops::lit("/*"), ops::any, +ops::not_(ops::eos) & ops::lit("*/"), false, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected EOF while parsing comment. expect */");
    });

    constexpr auto nested_c_comment = comment(ops::lit("/*"), ops::any, +ops::not_(ops::eos) & ops::lit("*/"), true, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected EOF while parsing comment. expect */");
    });

    constexpr auto shell_comment = comment(ops::lit("#"), ops::any, eol | eos, false, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected error");
    });

    constexpr auto cpp_comment = comment(ops::lit("//"), ops::any, eol | eos, false, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected error");
    });

    constexpr auto asm_comment = comment(ops::lit(";"), ops::any, eol | eos, false, [](auto&& seq, auto&& ctx, auto&& rec) {
        ctxs::context_error(seq, ctx, "unexpected error");
    });

    namespace test {
        constexpr auto check_comment() {
            auto test_it = [](auto&& fn, auto&& seq) {
                return fn(seq, 0, 0);
            };
            return test_it(shell_comment, make_ref_seq("# abstract\n")) == Status::match &&
                   test_it(nested_c_comment, make_ref_seq("/*un/**/*expect*/")) == Status::match &&
                   test_it(c_comment, make_ref_seq("/*")) == Status::fatal &&
                   test_it(c_comment, make_ref_seq("/*/**/")) == Status::match &&
                   test_it(cpp_comment, make_ref_seq("// comment")) == Status::match;
        }
        static_assert(check_comment());
    }  // namespace test
}  // namespace utils::comb2::composite
