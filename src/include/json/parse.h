/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse json
#pragma once
#include "jsonbase.h"
#include "../core/sequencer.h"
#include "../helper/space.h"
#include "../helper/strutil.h"
#include "../escape/escape.h"

namespace utils {

    namespace json {

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr parse(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json) {
            using self_t = JSONBase<String, Vec, Object>;
            using object_t = typename JSONBase<String, Vec, Object>::object_t;
            using array_t = typename JSONBase<String, Vec, Object>::array_t;

            auto consume_space = [&] {
                while (helper::space::match_space<true>(seq, true)) {
                }
            };
#define DETECT_EOF() \
    if (seq.eos())   \
    return JSONError::unexpected_eof
#define CONSUME_EOF() \
    consume_space();  \
    DETECT_EOF()

            auto read_strs = [&](size_t& beg, size_t& en) -> JSONErr {
                beg = seq.rptr;
                while (true) {
                    DETECT_EOF();
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
            auto unescape = [&](auto& str, size_t be, size_t en) -> JSONErr {
                auto sl = helper::make_ref_slice(seq.buf.buffer, be, en);
                if (!escape::unescape_str(sl, str)) {
                    return JSONError::invalid_escape;
                }
                return true;
            };
            CONSUME_EOF();
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
                size_t be, en;
                auto e = read_strs(be, en);
                if (!e) {
                    return e;
                }
                auto& s = json.get_holder();
                s = new String{};
                auto ptr = const_cast<String*>(s.as_str());
                assert(ptr);
                return unescape(*ptr, be, en);
            }
            else if (seq.consume_if('[')) {
                auto& s = json.get_holder();
                s = new array_t{};
                auto ptr = const_cast<array_t*>(s.as_arr());
                assert(ptr);
                bool first = true;
                while (true) {
                    CONSUME_EOF();
                    if (!first) {
                        if (!seq.consume_if(',') && seq.current() != ']') {
                            return JSONError::need_comma_on_array;
                        }
                        CONSUME_EOF();
                    }
                    if (seq.consume_if(']')) {
                        break;
                    }
                    DETECT_EOF();
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
                    CONSUME_EOF();
                    if (!first) {
                        if (!seq.consume_if(',') && seq.current() != '}') {
                            return JSONError::need_comma_on_object;
                        }
                        CONSUME_EOF();
                    }
                    if (seq.consume_if('}')) {
                        break;
                    }
                    if (!seq.consume_if('\"')) {
                        return JSONError::need_key_name;
                    }
                    size_t be, en;
                    auto e = read_strs(be, en);
                    if (!e) {
                        return e;
                    }
                    String key;
                    if (auto e = unescape(key, be, en); !e) {
                        return e;
                    }
                    CONSUME_EOF();
                    if (!seq.consume_if(':')) {
                        return JSONError::need_colon;
                    }
                    CONSUME_EOF();
                    self_t value;
                    auto err = parse(seq, value);
                    if (!err) {
                        return false;
                    }
                    auto res = ptr->emplace(std::move(key), std::move(value));
                    if (!std::get<1>(res)) {
                        return JSONError::emplace_error;
                    }
                    first = false;
                }
                return true;
            }
            else if (number::is_digit(seq.current()) || seq.current() == '-') {
                auto inipos = seq.rptr;
                bool sign = seq.current() == '-';
                std::int64_t v;
                auto e = number::parse_integer(seq, v);
                if (seq.current() == '.' || seq.current() == 'e' || seq.current() == 'E') {
                    double d;
                    seq.rptr = inipos;
                    auto e = number::parse_float(seq, d);
                    if (!e) {
                        return JSONError::invalid_number;
                    }
                    json = d;
                }
                else if (!sign && e == number::NumError::overflow) {
                    std::uint64_t u;
                    seq.rptr = inipos;
                    auto e = number::parse_integer(seq, u);
                    if (!e) {
                        return JSONError::invalid_number;
                    }
                    json = u;
                }
                else if (!e) {
                    return JSONError::invalid_number;
                }
                else {
                    json = v;
                }
                return true;
            }
            return JSONError::not_json;
        }
#undef DETECT_EOF
#undef CONSUME_EOF

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr parse(T&& in, JSONBase<String, Vec, Object>& json) {
            auto seq = make_ref_seq(in);
            return parse(seq, json);
        }

        template <class JSON, class T>
        JSON parse(T&& in) {
            JSON json;
            if (!json::parse(in, json)) {
                return {};
            }
            return json;
        }

    }  // namespace json

}  // namespace utils
