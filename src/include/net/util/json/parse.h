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

namespace utils {
    namespace net {
        namespace json {

            enum class JSONError {
                none,
                unknown,
            };

            using JSONErr = wrap::EnumWrap<JSONError, JSONError::none, JSONError::unknown>;

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
                }
            }
        }  // namespace json

    }  // namespace net
}  // namespace utils
