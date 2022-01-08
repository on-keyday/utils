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
#include "../../../helper/space.h"
#include "../../../helper/strutil.h"
#include "../../../escape/escape.h"

namespace utils {
    namespace net {
        namespace json {

            template <class T, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr parse(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json) {
                using self_t = JSONBase<String, Vec, Object>;
                using object_t = JSONBase<String, Vec, Object>::object_t;
                using array_t = JSONBase<String, Vec, Object>::array_t;
                auto consume_space = [&] {
                    while (helper::space::match_space<true>(seq, true)) {
                    }
                };
                auto read_strs = [&](size_t& beg, size_t& en) -> JSONErr {
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
                    en = seq.rptr;
                    seq.consume();
                    return true;
                };
                consume_space();
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
                else if (seq.consume_if('\"')) {
                    size_t beg, en;
                    auto e = read_strs(beg, en);
                    if (!e) {
                        return e;
                    }
                    auto sl = helper::make_ref_slice(buf.in, beg, en);
                    auto& s = json.get_holder();
                    s = new String{};
                    auto ptr = const_cast<String*>(s.as_str());
                    assert(ptr);
                    if (!escape::unescape_str(sl, *ptr)) {
                        return JSONError::invalid_escape;
                    }
                    return true;
                }
                else if (seq.consume_if('[')) {
                    auto& s = json.get_holder();
                    s = new array_t{};
                    auto ptr = const_cast<array_t*>(s.as_arr());
                    assert(ptr);
                    bool first = true;
                    while (true) {
                        consume_space();
                        if (seq.eos()) {
                            return JSONError::unexpected_eof;
                        }
                        if (!first) {
                            if (!seq.consume_if(',')) {
                                return JSONError::need_comma_on_array;
                            }
                            consume_space();
                        }
                        if (seq.consume_if(']')) {
                            break;
                        }
                        if (seq.eos()) {
                            return JSONError::unexpected_eof;
                        }
                        self_t tmp;
                        auto e = parse(seq, tmp);
                        if (!e) {
                            return e;
                        }
                        ptr->push_back(std::move(tmp));
                        first = false;
                    }
                    return true;
                }
                else if (seq.consume_if('{')) {
                    auto& s = json.get_holder();
                    s = new object_t{};
                    auto ptr = const_cast<object_t*>(s.as_obj());
                    assert(ptr);
                    bool first = true;
                    while (true) {
                        consume_space();
                        if (!first) {
                            if (!seq.consume_if(',')) {
                                return JSONError::need_comma_on_array;
                            }
                            consume_space();
                        }
                        if (seq.consume_if('}')) {
                            break;
                        }
                        first = false;
                    }
                }
            }
        }  // namespace json

    }  // namespace net
}  // namespace utils
