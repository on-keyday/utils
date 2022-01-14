/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "jsonbase.h"
#include "../helper/readutil.h"
#include "../escape/escape.h"
#include "../escape/read_string.h"

namespace utils {
    namespace json {

        enum class PathError {
            none,
            expect_slash,
            expect_dot_or_subscript,
            escape_failed,
            not_number,
            out_of_range,
            key_not_found,
            unknown,
        };

        using PathErr = wrap::EnumWrap<PathError, PathError::none, PathError::unknown>;

        template <class String, class T, class SepCond>
        PathErr read_key(String& key, Sequencer<T>& seq, SepCond&& cond) {
            bool str = false;
            if (seq.current() == '\"') {
                if (!escape::read_string(key, seq, escape::ReadFlag::escape)) {
                    return PathError::escape_failed;
                }
            }
            else {
                helper::read_whilef<true>(key, seq, [&](auto&& c) {
                    return cond(c);
                });
            }
            return true;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path_file_like(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
            ret = &json;
            while (!seq.eos()) {
                if (!seq.seek_if('/')) {
                    return PathError::expect_slash;
                }
                if (seq.eos()) {
                    break;
                }
                if (auto e = read_key(); !e) {
                    return e;
                }
                if (ret->is_array()) {
                    size_t idx = 0;
                    if (!number::parse_integer(key, idx)) {
                        return PathError::not_number;
                    }
                    auto tmp = ret->at(idx);
                    if (!tmp) {
                        return PathError::out_of_range;
                    }
                    ret = tmp;
                }
                else if (ret->is_object()) {
                    auto tmp = ret->at(key);
                    if (!tmp) {
                        if (!append) {
                            return PathError::key_not_found;
                        }
                        tmp = &(*ret)[key];
                    }
                    ret = tmp;
                }
            }
            return true;
        }
        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path_object_like(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
            bool as_path = false;
            if (seq.match("/")) {
                as_path = true;
            }
            while (!seq.eos()) {
                bool as_array = false;
                if (seq.consume_if('.')) {
                }
                else if (seq.consume_if('[')) {
                    as_array = true;
                }
                else {
                    return PathError::expect_dot_or_subscript;
                }
            }
        }
    }  // namespace json
}  // namespace utils
