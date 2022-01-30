/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// line_pos - show line and pos
#pragma once
#include "../core/sequencer.h"
#include "appender.h"
#include "readutil.h"
#include "../number/to_string.h"

namespace utils {
    namespace helper {
        template <class Out, class T>
        void write_pos(Out& w, Sequencer<T>& seq, char pos = '^') {
            size_t line, pos;
            get_linepos(seq, line, pos);

            number::to_string(w, line + 1);
        }
    }  // namespace helper
}  // namespace utils
