/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../unicode/utf/convert.h"
#include "nullctx.h"
#include "comb.h"

namespace utils::minilang::comb::known {
    constexpr auto make_unicode_range(auto&& judge, std::uint64_t min_ = 0, std::uint64_t max_ = -1) {
        return [=](auto&& seq, auto&& ctx, auto&&) {
            const size_t begin = seq.rptr;
            size_t count = 0;
            while (count < max_) {
                if (seq.eos()) {
                    break;
                }
                struct {
                    std::uint32_t v = -1;
                    constexpr void push_back(std::uint32_t c) {
                        v = c;
                    }
                } out;
                auto prev = seq.rptr;
                auto err = utf::convert_one<0, 4>(seq, out, true, false);
                CombStatus res = judge(seq, err, out.v, count);
                if (res == CombStatus::fatal) {
                    report_error(ctx, "invalid character in unicode range");
                    return res;
                }
                if (seq.rptr < begin) {
                    report_error(ctx, "offset is not valid expect larger than ", begin, " but pos are ", seq.rptr);
                    return CombStatus::fatal;
                }
                if (seq.rptr < prev) {
                    report_error(ctx, "offset is not valid expect larger than ", prev, " but pos are ", seq.rptr);
                    return CombStatus::fatal;
                }
                if (res == CombStatus::not_match) {
                    seq.rptr = prev;
                    break;
                }
                if (seq.rptr <= prev) {
                    report_error(ctx, "offset is not valid expect larger than ", prev, " but pos are ", seq.rptr);
                    return CombStatus::fatal;
                }
                count++;
            }
            if (count < min_) {
                seq.rptr = begin;
                return CombStatus::not_match;
            }
            return CombStatus::match;
        };
    }

    template <class F>
    concept has_count_arg = requires(F f) {
        { f(size_t{}, size_t{}) };
    };

    constexpr auto make_no_error_unicode_range(auto&& judge, size_t min_ = 0, size_t max_ = -1) {
        return make_unicode_range(
            [=](auto&& seq, utf::UTFError err, std::uint32_t code, size_t count) {
                if (err != utf::UTFError::none) {
                    return CombStatus::fatal;
                }
                if constexpr (has_count_arg<decltype(judge)>) {
                    return judge(code, count) ? CombStatus::match : CombStatus::not_match;
                }
                else {
                    return judge(code) ? CombStatus::match : CombStatus::not_match;
                }
            },
            min_, max_);
    }

    namespace test {
        constexpr bool check_unicode() {
            auto parse = make_no_error_unicode_range([](std::uint32_t code) {
                return code == U'𠮷' ||
                       code == U'野' ||
                       code == U'家';
            });
            auto seq = make_ref_seq(u8"𠮷野家");
            return parse(seq, 0, 0) == CombStatus::match;
        }

        static_assert(check_unicode());
    }  // namespace test
}  // namespace utils::minilang::comb::known
