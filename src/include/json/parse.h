/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
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

            template <class T, class String, template <class...> class Vec, template <class...> class Object, class CustomCallback>
            JSONErr parse_impl(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json, CustomCallback&& custom_cb) {
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
#define unescape(str, be, en)                                   \
    {                                                           \
        auto sl = view::make_ref_slice(seq.buf.buffer, be, en); \
        if (!escape::unescape_str(sl, str)) {                   \
            return JSONError::invalid_escape;                   \
        }                                                       \
    }
                CONSUME_EOF();
                auto c = seq.current();
                switch (c) {
                    case 't': {
                        if (!seq.seek_if("true")) {
                            return JSONError::not_json;
                        }
                        if (!custom_cb.on_bool(std::as_const(seq), json, true)) {
                            return JSONError::invalid_value;
                        }
                        json = true;
                        break;
                    }
                    case 'f': {
                        if (!seq.seek_if("false")) {
                            return JSONError::not_json;
                        }
                        if (!custom_cb.on_bool(std::as_const(seq), json, false)) {
                            return JSONError::invalid_value;
                        }
                        json = false;
                        break;
                    }
                    case 'n': {
                        if (!seq.seek_if("null")) {
                            return JSONError::not_json;
                        }
                        if (!custom_cb.on_null(std::as_const(seq), json)) {
                            return JSONError::invalid_value;
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
                        unescape(s, be, en);
                        if (!custom_cb.on_string(std::as_const(seq), json, s)) {
                            return JSONError::invalid_value;
                        }
                        break;
                    }
                    case '[': {
                        seq.rptr += 1;
                        auto& s = json.get_holder().init_as_array();
                        if (!custom_cb.on_array_begin(std::as_const(seq), json, s)) {
                            return JSONError::invalid_value;
                        }
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
                                if (!custom_cb.on_array_element_before(std::as_const(seq), json, s, s.back())) {
                                    return JSONError::invalid_value;
                                }
                                auto e = parse_impl(seq, s.back(), custom_cb);
                                if (!e) {
                                    return e;
                                }
                                if (!custom_cb.on_array_element_after(std::as_const(seq), json, s, s.back())) {
                                    return JSONError::invalid_value;
                                }
                            }
                            else {
                                self_t tmp;
                                if (!custom_cb.on_array_element_before(std::as_const(seq), json, s, tmp)) {
                                    return JSONError::invalid_value;
                                }
                                auto e = parse_impl(seq, tmp, custom_cb);
                                if (!e) {
                                    return e;
                                }
                                if (!custom_cb.on_array_element_after(std::as_const(seq), json, s, tmp)) {
                                    return JSONError::invalid_value;
                                }
                                s.push_back(std::move(tmp));
                            }
                            first = false;
                        }
                        if (!custom_cb.on_array_end(std::as_const(seq), json, s)) {
                            return JSONError::invalid_value;
                        }
                        break;
                    }
                    case '{': {
                        seq.rptr += 1;
                        auto& s = json.get_holder().init_as_object();
                        custom_cb.on_object_begin(std::as_const(seq), json, s);
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
                            unescape(key, be, en);
                            if (!custom_cb.on_object_key(std::as_const(seq), json, s, key)) {
                                return JSONError::invalid_key_name;
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
                            if (!custom_cb.on_object_value_before(std::as_const(seq), json, s, val)) {
                                return JSONError::invalid_value;
                            }
                            auto err = parse_impl(seq, val, custom_cb);
                            if (!err) {
                                return err;
                            }
                            if (!custom_cb.on_object_value_after(std::as_const(seq), json, s, val)) {
                                return JSONError::invalid_value;
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
                            if (!custom_cb.on_float(std::as_const(seq), json, d)) {
                                return JSONError::invalid_value;
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
                            if (!custom_cb.on_uint(std::as_const(seq), json, u)) {
                                return JSONError::invalid_value;
                            }
                            json = u;
                        }
                        else if (!e) {
                            return JSONError::invalid_number;
                        }
                        else {
                            if (!custom_cb.on_int(std::as_const(seq), json, v)) {
                                return JSONError::invalid_value;
                            }
                            json = v;
                        }
                        break;
                    }
                }
                return JSONError::none;
            }
#undef unescape
#undef DETECT_EOF
#undef CONSUME_EOF
        }  // namespace internal

        struct EmptyCustomCallback {
            bool on_bool(const auto& seq, auto& json, bool value) {
                return true;
            }
            bool on_null(const auto& seq, auto& json) {
                return true;
            }
            bool on_string(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_int(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_uint(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_float(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_array_begin(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_array_element_before(const auto& seq, auto& json, auto& value, auto& element) {
                return true;
            }
            bool on_array_element_after(const auto& seq, auto& json, auto& value, auto& element) {
                return true;
            }
            bool on_array_end(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_object_begin(const auto& seq, auto& json, auto& value) {
                return true;
            }
            bool on_object_key(const auto& seq, auto& json, auto& value, auto& key) {
                return true;
            }
            bool on_object_value_before(const auto& seq, auto& json, auto& value, auto& obj) {
                return true;
            }
            bool on_object_value_after(const auto& seq, auto& json, auto& value, auto& obj) {
                return true;
            }
            bool on_object_end(const auto& seq, auto& json, auto& value) {
                return true;
            }
        };

        template <class T, class String, template <class...> class Vec, template <class...> class Object, class CustomCallback = EmptyCustomCallback>
        JSONErr parse(Sequencer<T>& seq, JSONBase<String, Vec, Object>& json, bool eof = false, CustomCallback&& custom_cb = EmptyCustomCallback()) {
            auto res = internal::parse_impl(seq, json, custom_cb);
            if (res && eof) {
                while (strutil::parse_space<true>(seq, true)) {
                }
                if (!seq.eos()) {
                    return JSONError::not_eof;
                }
            }
            return res;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object, class CustomCallback = EmptyCustomCallback>
        JSONErr parse(T&& in, JSONBase<String, Vec, Object>& json, bool eof = false, CustomCallback&& custom_cb = EmptyCustomCallback()) {
            auto seq = make_ref_seq(in);
            return parse(seq, json, eof, custom_cb);
        }

        template <class JSON, class T, class CustomCallback = EmptyCustomCallback>
        JSON parse(T&& in, bool eof = false, CustomCallback&& custom_cb = EmptyCustomCallback()) {
            JSON json;
            if (!parse(in, json, eof, custom_cb)) {
                return {};
            }
            return json;
        }

    }  // namespace json

}  // namespace futils
