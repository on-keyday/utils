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
#include "../../../number/to_string.h"
#include "../../../helper/appender.h"
#include "../../../escape/escape.h"
#include "../../../helper/indent.h"

namespace utils {
    namespace net {
        namespace json {
            enum class FmtFlag {
                none,
                escape = 0x1,
                space_key_value = 0x2,
                no_line = 0x4,
            };

            DEFINE_ENUM_FLAGOP(FmtFlag)

            template <class Out, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string(const JSONBase<String, Vec, Object>& json, helper::IndentWriter<Out, const char*>& out, FmtFlag flag = FmtFlag::none) {
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
                if (auto b = holder.as_bool()) {
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
                    auto e = escape::escape_str(*s, out.t, escflag);
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
                            }
                        }
                        if (line) {
                            out.write_indent();
                        }
                        out.write_raw("\"");
                        auto e1 = escape::escape_str(std::get<0>(kv), out.t, escflag);
                        if (!e1) {
                            return JSONError::invalid_escape;
                        }
                        out.write_raw("\":");
                        if (any(flag & FmtFlag::space_key_value)) {
                            out.push_back(' ');
                        }
                        if (line) {
                            out.indent(1);
                        }
                        auto e2 = to_string(std::get<1>(kv), out, flag);
                        if (!e2) {
                            return e2;
                        }
                        if (line) {
                            out.indent(-1);
                        }
                    }
                    if (!first) {
                        if (line) {
                            out.write_line();
                            out.indent(-1);
                        }
                    }
                    out.write_raw("}");
                }
                return JSONError::not_json;
            }
        }  // namespace json
    }      // namespace net
}  // namespace utils
