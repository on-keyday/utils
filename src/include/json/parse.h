/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse json
#pragma once
#include "jsonbase.h"
#include "../core/sequencer.h"
#include "../strutil/space.h"
// #include "../strutil/strutil.h"
#include "../escape/escape.h"
#include "../view/slice.h"

namespace futils {

    namespace json {

        namespace internal {
            template <class T>
            concept has_resize = requires(T t) {
                { t.resize(size_t{}) };
            };
            template <class T, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr parse_impl(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json) {
                using self_t = JSONBase<String, Vec, Object>;
                using object_t = typename JSONBase<String, Vec, Object>::object_t;
                using array_t = typename JSONBase<String, Vec, Object>::array_t;

#define DETECT_EOF() \
    if (seq.eos())   \
    return JSONError::unexpected_eof
#define CONSUME_EOF()                               \
    while (strutil::parse_space<true>(seq, true)) { \
    }                                               \
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
                        seq.rptr += 1;
                    }
                    en = seq.rptr;
                    seq.consume();
                    return true;
                };
                auto unescape = [&](auto& str, size_t be, size_t en) -> JSONErr {
                    auto sl = view::make_ref_slice(seq.buf.buffer, be, en);
                    if (!escape::unescape_str(sl, str)) {
                        return JSONError::invalid_escape;
                    }
                    return true;
                };
                CONSUME_EOF();
                auto c = seq.current();
                switch (c) {
                    case 't': {
                        if (!seq.seek_if("true")) {
                            return JSONError::not_json;
                        }
                        json = true;
                        break;
                    }
                    case 'f': {
                        if (!seq.seek_if("false")) {
                            return JSONError::not_json;
                        }
                        json = false;
                        break;
                    }
                    case 'n': {
                        if (!seq.seek_if("null")) {
                            return JSONError::not_json;
                        }
                        json = nullptr;
                        break;
                    }
                    case '\"': {
                        seq.rptr += 1;
                        size_t be, en;
                        auto e = read_strs(be, en);
                        if (!e) {
                            return e;
                        }
                        auto& s = json.get_holder().init_as_string();
                        if (auto e = unescape(s, be, en); !e) {
                            return e;
                        }
                        break;
                    }
                    case '[': {
                        seq.rptr += 1;
                        auto& s = json.get_holder().init_as_array();
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
                            if constexpr (has_resize<array_t>) {
                                s.resize(s.size() + 1);
                                auto e = parse_impl(seq, s.back());
                                if (!e) {
                                    return e;
                                }
                            }
                            else {
                                self_t tmp;
                                auto e = parse_impl(seq, tmp);
                                if (!e) {
                                    return e;
                                }
                                s.push_back(std::move(tmp));
                            }
                            first = false;
                        }
                        break;
                    }
                    case '{': {
                        seq.rptr += 1;
                        auto& s = json.get_holder().init_as_object();
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
                            auto res = s.emplace(std::move(key), self_t{});
                            if (!get<1>(res)) {
                                return JSONError::emplace_error;
                            }
                            self_t& val = get<1>(*get<0>(res));
                            auto err = parse_impl(seq, val);
                            if (!err) {
                                return err;
                            }
                            first = false;
                        }
                        break;
                    }
                    case '0':
                        [[fallthrough]];
                    case '1':
                        [[fallthrough]];
                    case '2':
                        [[fallthrough]];
                    case '3':
                        [[fallthrough]];
                    case '4':
                        [[fallthrough]];
                    case '5':
                        [[fallthrough]];
                    case '6':
                        [[fallthrough]];
                    case '7':
                        [[fallthrough]];
                    case '8':
                        [[fallthrough]];
                    case '9':
                        [[fallthrough]];
                    case '-': {
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
                        break;
                    }
                    default:
                        return JSONError::not_json;
                }
                return JSONError::none;
            }
#undef DETECT_EOF
#undef CONSUME_EOF
        }  // namespace internal

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr parse(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json, bool eof = false) {
            auto res = internal::parse_impl(seq, json);
            if (res && eof) {
                while (strutil::parse_space<true>(seq, true)) {
                }
                if (!seq.eos()) {
                    return JSONError::not_eof;
                }
            }
            return res;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr parse(T&& in, JSONBase<String, Vec, Object>& json, bool eof = false) {
            auto seq = make_ref_seq(in);
            return parse(seq, json, eof);
        }

        template <class JSON, class T>
        JSON parse(T&& in, bool eof = false) {
            JSON json;
            if (!parse(in, json, eof)) {
                return {};
            }
            return json;
        }

    }  // namespace json

}  // namespace futils
