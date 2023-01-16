/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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
            expect_end_subscript,
            not_object_or_array,
            unknown,
        };

        using PathErr = wrap::EnumWrap<PathError, PathError::none, PathError::unknown>;
        namespace internal {
            template <class String, class T, class SepCond>
            PathErr read_key(String& key, Sequencer<T>& seq, SepCond&& cond, bool& str) {
                if (seq.current() == '\"') {
                    str = true;
                    if (!escape::read_string(key, seq, escape::ReadFlag::escape, escape::default_prefix(), escape::json_set())) {
                        return PathError::escape_failed;
                    }
                }
                else {
                    str = false;
                    helper::read_whilef<true>(key, seq, [&](auto&& c) {
                        return cond(c);
                    });
                }
                return true;
            }

            template <class String, template <class...> class Vec, template <class...> class Object>
            PathErr update_path(String& key, JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, bool append, bool str) {
                if (!ret) {
                    ret = &json;
                }
                if (append && ret->is_undef()) {
                    if (str) {
                        ret->init_obj();
                    }
                    else {
                        ret->init_array();
                    }
                }
                if (ret->is_array()) {
                    size_t idx = 0;
                    if (!number::parse_integer(key, idx)) {
                        return PathError::not_number;
                    }
                    auto tmp = ret->at(idx);
                    if (!tmp) {
                        if (append && ret->size() == idx) {
                            ret->push_back(JSONBase<String, Vec, Object>{});
                            tmp = ret->at(idx);
                            assert(tmp);
                        }
                        else {
                            return PathError::out_of_range;
                        }
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
                else {
                    return PathError::not_object_or_array;
                }
                return true;
            }
        }  // namespace internal

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path_file_like(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
            ret = &json;
            while (!seq.eos()) {
                if (!seq.consume_if('/')) {
                    return PathError::expect_slash;
                }
                if (seq.eos()) {
                    break;
                }
                String key;
                bool str = false;
                if (auto e = internal::read_key(
                        key, seq, [&](auto& c) { return c != '/'; }, str);
                    !e) {
                    return e;
                }
                if (auto e = internal::update_path(key, ret, json, append, str); !e) {
                    return e;
                }
            }
            return true;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path_object_like(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
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
                String key;
                bool str = false;
                if (auto e = internal::read_key(
                        key, seq, [&](auto& c) {if(as_array){return c!=']';}else{return c!='.'&&c!='[';} }, str);
                    !e) {
                    return e;
                }
                if (auto e = internal::update_path(key, ret, json, append, !as_array ? true : str); !e) {
                    return e;
                }
                if (as_array) {
                    if (!seq.consume_if(']')) {
                        return PathError::expect_end_subscript;
                    }
                }
            }
            return true;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Sequencer<T>& seq, bool append = false) {
            if (seq.current() == '/') {
                return path_file_like(ret, json, seq, append);
            }
            else {
                return path_object_like(ret, json, seq, append);
            }
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path(const JSONBase<String, Vec, Object>*& ret, const JSONBase<String, Vec, Object>& json, Sequencer<T>& seq) {
            using self_t = JSONBase<String, Vec, Object>;
            return path(const_cast<self_t*&>(ret), const_cast<self_t&>(json), seq, false);
        }

        template <class Path, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path(JSONBase<String, Vec, Object>*& ret, JSONBase<String, Vec, Object>& json, Path&& pathstr, bool append = false) {
            auto seq = make_ref_seq(pathstr);
            return path(ret, json, seq, append);
        }

        template <class Path, class String, template <class...> class Vec, template <class...> class Object>
        PathErr path(const JSONBase<String, Vec, Object>*& ret, const JSONBase<String, Vec, Object>& json, Path&& pathstr) {
            auto seq = make_ref_seq(pathstr);
            return path(ret, json, seq);
        }

        template <class Path, class String, template <class...> class Vec, template <class...> class Object>
        JSONBase<String, Vec, Object>* path(JSONBase<String, Vec, Object>& json, Path&& pathstr, bool append = false) {
            JSONBase<String, Vec, Object>* ret = nullptr;
            if (!path(ret, json, pathstr, append)) {
                return nullptr;
            }
            return ret;
        }

        template <class Path, class String, template <class...> class Vec, template <class...> class Object>
        const JSONBase<String, Vec, Object>* path(const JSONBase<String, Vec, Object>& json, Path&& pathstr) {
            return path(const_cast<JSONBase<String, Vec, Object>&>(json), pathstr, false);
        }
    }  // namespace json
}  // namespace utils
