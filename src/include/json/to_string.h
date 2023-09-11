/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string -json to string
#pragma once
#include "error.h"
#include "jsonbase.h"
#include "../number/to_string.h"
#include <strutil/append.h>
#include "../escape/escape.h"
#include "../code/code_writer.h"
#include "stringer.h"

namespace utils {

    namespace json {
        namespace internal {
            template <class S, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string_detail(S& out, const JSONBase<String, Vec, Object>& json, FmtFlag flags, IntTraits traits) {
                static_assert(helper::is_template_instance_of<S, Stringer>);
                using tio = typename helper::template_instance_of_t<S, Stringer>;
                using Str = Stringer<typename tio::template param_at<0>, typename tio::template param_at<1>, typename tio::template param_at<2>>;
                using Holder = const internal::JSONHolder<String, Vec, Object>;
                Str& w = out;
                Holder& holder = json.get_holder();
                w.set_utf_escape(any(flags & FmtFlag::escape));
                w.set_html_escape(any(flags & FmtFlag::html));
                w.set_no_colon_space(any(flags & FmtFlag::no_space_key_value));
                w.set_int_traits(traits);
                auto f = [&](auto& f, Holder& h) -> JSONErr {
                    if (h.is_undef()) {
                        if (!any(flags & FmtFlag::undef_as_null)) {
                            return JSONError::invalid_value;
                        }
                        w.null();
                    }
                    else if (h.is_null()) {
                        w.null();
                    }
                    else if (auto i = h.as_numi()) {
                        w.number(*i);
                    }
                    else if (auto u = h.as_numu()) {
                        w.number(*u);
                    }
                    else if (auto fl = h.as_numf()) {
                        w.number(*fl);
                    }
                    else if (auto str = h.as_str()) {
                        w.string(*str);
                    }
                    else if (auto b = h.as_bool()) {
                        w.boolean(*b);
                    }
                    else if (auto obj = h.as_obj()) {
                        auto field = w.object();
                        for (auto& kv : *obj) {
                            JSONError err;
                            field(get<0>(kv), [&] {
                                Holder& h = get<1>(kv).get_holder();
                                err = f(f, h);
                            });
                            if (err != JSONError::none) {
                                return err;
                            }
                        }
                    }
                    else if (auto arr = h.as_arr()) {
                        auto field = w.array();
                        for (auto& v : *arr) {
                            JSONError err;
                            field([&] {
                                Holder& h = v.get_holder();
                                err = f(f, h);
                            });
                            if (err != JSONError::none) {
                                return err;
                            }
                        }
                    }
                    else {
                        return JSONError::not_json;
                    }
                    return JSONError::none;
                };
                return f(f, holder);
            }

            /*
            template <class Out, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string_detail(const JSONBase<String, Vec, Object>& json, code::IndentWriter<Out, const char*>& out, FmtFlag flag) {
                const internal::JSONHolder<String, Vec, Object>& holder = json.get_holder();
                auto numtostr = [&](auto& j) -> JSONErr {
                    auto e = number::to_string(out.t, j);
                    if (!e) {
                        return JSONError::invalid_number;
                    }
                    return true;
                };
                auto escflag = any(flag & FmtFlag::escape) ? escape::EscapeFlag::utf16 : escape::EscapeFlag::none;
                auto line = !any(flag & FmtFlag::no_line);
                auto write_comma = [&](bool& first) {
                    if (first) {
                        if (line) {
                            out.write_ln();
                            out.indent(1);
                        }
                        first = false;
                    }
                    else {
                        out.write_raw(",");
                        if (line) {
                            out.write_ln();
                            out.indent(1);
                        }
                    }
                    if (line) {
                        out.write_indent();
                    }
                };
                auto write_tail = [&](bool& first) {
                    if (!first) {
                        if (line) {
                            out.write_ln();
                        }
                    }
                };
                auto escape = [&](auto& str) {
                    if (any(flag & FmtFlag::html)) {
                        return escape::escape_str(str, out.t, escflag, escape::json_set(), escape::html_range());
                    }
                    else {
                        if (any(flag & FmtFlag::unescape_slash)) {
                            return escape::escape_str(str, out.t, escflag, escape::json_set_no_html());
                        }
                        else {
                            return escape::escape_str(str, out.t, escflag, escape::json_set());
                        }
                    }
                };
                if (holder.is_undef()) {
                    if (any(flag & FmtFlag::undef_as_null)) {
                        strutil::append(out.t, "null");
                        return true;
                    }
                    return JSONError::invalid_value;
                }
                else if (holder.is_null()) {
                    strutil::append(out.t, "null");
                    return true;
                }
                else if (auto b = holder.as_bool()) {
                    strutil::append(out.t, *b ? "true" : "false");
                    return true;
                }
                else if (auto i = holder.as_numi()) {
                    return numtostr(*i);
                }
                else if (auto u = holder.as_numu()) {
                    return numtostr(*u);
                }
                else if (auto f = holder.as_numf()) {
                    return numtostr(*f);
                }
                else if (auto s = holder.as_str()) {
                    out.t.push_back('\"');
                    auto e = escape(*s);
                    if (!e) {
                        return JSONError::invalid_escape;
                    }
                    out.t.push_back('\"');
                    return true;
                }
                else if (auto o = holder.as_obj()) {
                    out.write_raw("{");
                    bool first = true;
                    for (auto& kv : *o) {
                        write_comma(first);
                        out.write_raw("\"");
                        auto e1 = escape(get<0>(kv));
                        if (!e1) {
                            return JSONError::invalid_escape;
                        }
                        out.write_raw("\":");
                        if (!any(flag & FmtFlag::no_space_key_value)) {
                            out.t.push_back(' ');
                        }
                        auto e2 = to_string_detail(get<1>(kv), out, flag);
                        if (!e2) {
                            return e2;
                        }
                        if (line) {
                            out.indent(-1);
                        }
                    }
                    write_tail(first);
                    if (!first && line) {
                        out.write_indent();
                    }
                    out.write_raw("}");
                    return true;
                }
                else if (auto a = holder.as_arr()) {
                    out.write_raw("[");
                    bool first = true;
                    for (auto& v : *a) {
                        write_comma(first);
                        auto e2 = to_string_detail(v, out, flag);
                        if (!e2) {
                            return e2;
                        }
                        if (line) {
                            out.indent(-1);
                        }
                    }
                    write_tail(first);
                    if (!first && line) {
                        out.write_indent();
                    }
                    out.write_raw("]");
                    return true;
                }
                return JSONError::not_json;
            }*/
        }  // namespace internal

        template <class S, class String, template <class...> class Vec, template <class...> class Object>
            requires helper::is_template_instance_of<S, Stringer>
        JSONErr to_string(const JSONBase<String, Vec, Object>& json, S& out, FmtFlag flag = FmtFlag::none, IntTraits traits = IntTraits::int_as_int) {
            auto e = internal::to_string_detail(out, json, flag, traits);
            if (e && any(flag & FmtFlag::last_line)) {
                out.out().push_back('\n');
            }
            return e;
        }

        template <class Out, class String, template <class...> class Vec, template <class...> class Object, class Indent = const char*>
            requires(!helper::is_template_instance_of<Out, Stringer>)
        JSONErr to_string(const JSONBase<String, Vec, Object>& json, Out& out, FmtFlag flag = FmtFlag::none, Indent indent = "    ", IntTraits traits = IntTraits::int_as_int) {
            auto w = code::make_indent_writer(out, indent);
            Stringer<decltype(out), std::string_view, std::decay_t<Indent>> s{out};
            s.set_indent(indent);
            return to_string(json, s, flag, traits);
        }

        template <class Out, class String, template <class...> class Vec, template <class...> class Object>
        Out to_string(const JSONBase<String, Vec, Object>& json, FmtFlag flag = FmtFlag::none, const char* indent = "    ", IntTraits traits = IntTraits::int_as_int) {
            Out res;
            if (!to_string(json, res, flag, indent, traits)) {
                return {};
            }
            return res;
        }
    }  // namespace json

}  // namespace utils
