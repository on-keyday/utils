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

namespace utils {
    namespace helper {
        template <class Out, class T>
        void write_pos(Out& w, Sequencer<T>& seq, char pos = '^') {
            get_linepos();
        }
    }  // namespace helper
}  // namespace utils
