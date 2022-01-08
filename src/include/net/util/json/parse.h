/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse json
#pragma once
#include "jsonbase.h"
#include "../../../core/sequencer.h"
#include "../../../wrap/lite/enum.h"
#include "../../../helper/space.h"
#include "../../../helper/strutil.h"
#include "../../../escape/escape.h"

namespace utils {
    namespace net {
        namespace json {

            template <class T, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr parse(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json) {
                while (helper::space::match_space<true>(seq, true)) {
                }
                if (seq.seek_if("true")) {
                    json = true;
                    return true;
                }
                else if (seq.seek_if("false")) {
                    json = false;
                    return true;
                }
                else if (seq.seek_if("null")) {
                    json = nullptr;
                    return true;
                }
                else if (seq.seek_if("\"")) {
                    seq.consume();
                    auto beg = seq.rptr;
                    while (true) {
                        if (!seq.eos()) {
                            return JSONError::unexpected_eof;
                        }
                        if (seq.current() == '\"') {
                            if (seq.current(-1) != '\\') {
                                break;
                            }
                        }
                        seq.consume();
                    }
                    auto sl = helper::make_ref_slice(seq.buf, beg, seq.rptr);
                    seq.consume();
                    escape::unescape_str(sl, out);
                }
            }
        }  // namespace json

    }  // namespace net
}  // namespace utils
