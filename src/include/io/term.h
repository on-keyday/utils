/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "reader.h"
#include "writer.h"

namespace utils {
    namespace io {

        // read_terminated reads null or single char terminated string
        // returning rvec will not include null byte
        template <class C, class U, class V = C>
        constexpr std::pair<view::rvec, bool> read_terminated(basic_reader<C, U>& r, V term = C(0), bool consume_term = true) {
            const auto cur = r.offset();
            while (!r.empty()) {
                if (r.top() == term) {
                    break;
                }
                r.offset(1);
            }
            if (r.empty()) {
                r.reset(cur);
                return {{}, false};
            }
            auto ret = r.read().substr(cur);
            if (consume_term) {
                r.offset(1);
            }
            return {ret, true};
        }

        struct SkipSpace {
            constexpr bool operator()(auto c) const {
                return c == ' ';
            }
        };

        template <class C, class U, class Skip = SkipSpace>
        constexpr view::rvec skip(basic_reader<C, U>& r, Skip&& is_skip = Skip{}) {
            const auto cur = r.offset();
            while (!r.empty() && is_skip(r.top())) {
                r.offset(1);
            }
            return r.read().substr(cur);
        }

        // read_terminated reads null or single char terminated string
        // returning rvec will not include null byte
        template <class C, class U, class V = C>
        constexpr bool read_terminated(basic_reader<C, U>& r, view::basic_rvec<C, U>& res, V term = C(0), bool consume_term = true) {
            bool ok = false;
            std::tie(res, ok) = read_terminated(r, consume_term);
            return ok;
        }

        template <class C, class U>
        constexpr bool has_term(view::basic_rvec<C, U> r, C term) {
            for (auto c : r) {
                if (c == term) {
                    return true;
                }
            }
            return false;
        }

        template <class C, class U>
        constexpr bool write_terminated(basic_writer<C, U>& w, view::basic_rvec<C, U> data, C term = 0) {
            if (has_null(data)) {
                return false;
            }
            return w.write(data) &&
                   w.write(term, 1);
        }
    }  // namespace io
}  // namespace utils
