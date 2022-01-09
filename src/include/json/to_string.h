/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_string -json to string
#pragma once
#include "error.h"
#include "jsonbase.h"
#include "../number/to_string.h"
#include "../helper/appender.h"
#include "../escape/escape.h"
#include "../helper/indent.h"

namespace utils {

    namespace json {
        namespace internal {
            template <class Out, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string_detail(const JSONBase<String, Vec, Object>& json, helper::IndentWriter<Out, const char*>& out, FmtFlag flag) {
                const internal::JSONHolder<String, Vec, Object>& holder = json.get_holder();
                auto numtostr = [&](auto& j) -> JSONErr {
                    auto e = number::to_string(out.t, j);
                    if (!e) {
                        return JSONError::invalid_number;
                    }
                    return true;
                };
                auto escflag = any(flag & FmtFlag::escape) ? escape::EscapeFlag::utf : escape::EscapeFlag::none;
                auto line = !any(flag & FmtFlag::no_line);
                auto write_comma = [&](bool& first) {
                    if (first) {
                        if (line) {
                            out.write_line();
                            out.indent(1);
                        }
                        first = false;
                    }
                    else {
                        out.write_raw(",");
                        if (line) {
                            out.write_line();
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
                            out.write_line();
                        }
                    }
                };
                auto escape = [&](auto& str) {
                    if (any(flag & FmtFlag::html)) {
                        return escape::escape_str(str, out.t, escflag, escape::json_set(), escape::html_range());
                    }
                    else {
                        return escape::escape_str(str, out.t, escflag, escape::json_set());
                    }
                };
                if (holder.is_undef()) {
                    if (any(flag & FmtFlag::undef_as_null)) {
                        helper::append(out.t, "null");
                        return true;
                    }
                    return JSONError::invalid_value;
                }
                else if (holder.is_null()) {
                    helper::append(out.t, "null");
                    return true;
                }
                else if (auto b = holder.as_bool()) {
                    helper::append(out.t, *b ? "true" : "false");
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
                        auto e1 = escape(std::get<0>(kv));
                        if (!e1) {
                            return JSONError::invalid_escape;
                        }
                        out.write_raw("\":");
                        if (!any(flag & FmtFlag::no_space_key_value)) {
                            out.t.push_back(' ');
                        }
                        auto e2 = to_string(std::get<1>(kv), out, flag);
                        if (!e2) {
                            return e2;
                        }
                        if (line) {
                            out.indent(-1);
                        }
                    }
                    write_tail(first);
                    if (line) {
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
                        auto e2 = to_string(v, out, flag);
                        if (!e2) {
                            return e2;
                        }
                        if (line) {
                            out.indent(-1);
                        }
                    }
                    write_tail(first);
                    if (line) {
                        out.write_indent();
                    }
                    out.write_raw("]");
                    return true;
                }
                return JSONError::not_json;
            }
        }  // namespace internal
        template <class Out, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr to_string(const JSONBase<String, Vec, Object>& json, helper::IndentWriter<Out, const char*>& out, FmtFlag flag = FmtFlag::none) {
            auto e = internal::to_string_detail(json, out, flag);
            if (e && any(flag & FmtFlag::last_line)) {
                out.write_line();
            }
            return e;
        }

        template <class Out, class String, template <class...> class Vec, template <class...> class Object>
        JSONErr to_string(const JSONBase<String, Vec, Object>& json, Out& out, FmtFlag flag = FmtFlag::none, const char* indent = "    ") {
            auto w = helper::make_indent_writer(out, indent);
            return to_string(json, w, flag);
        }

        template <class Out, class String, template <class...> class Vec, template <class...> class Object>
        Out to_string(const JSONBase<String, Vec, Object>& json, FmtFlag flag = FmtFlag::none, const char* indent = "    ") {
            Out res;
            if (!to_string(json, res, flag, indent)) {
                return {};
            }
            return res;
        }
    }  // namespace json

}  // namespace utils
