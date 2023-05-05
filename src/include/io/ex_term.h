/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "term.h"
#include "expandable_writer.h"

namespace utils {
    namespace io {

        template <class T, class C, class U>
        constexpr bool write_terminated(basic_expand_writer<T, C, U>& w, view::basic_rvec<C, U> data, C term = 0) {
            if (has_term(data, term)) {
                return false;
            }
            return w.write(data) &&
                   w.write(term, 1);
        }
    }  // namespace io
}  // namespace utils
