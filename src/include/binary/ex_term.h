/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "term.h"
#include "expandable_writer.h"

namespace futils {
    namespace binary {

        template <class T, class C>
        constexpr bool write_terminated(basic_expand_writer<T, C>& w, view::basic_rvec<C> data, C term = 0) {
            if (has_term(data, term)) {
                return false;
            }
            return w.write(data) &&
                   w.write(term, 1);
        }
    }  // namespace binary
}  // namespace futils
