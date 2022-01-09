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
            template <class Out, class String, template <class...> class Vec, template <class...> class Object>
            JSONErr to_string(const JSONBase<String, Vec, Object>& json, helper::IndentWriter<Out, const char*>& out, bool escape) {
                internal::JSONHolder<String, Vec, Object>& holder = json.get_holder();
                auto numtostr = [&](auto& j) -> JSONErr {
                    auto e = number::to_string(out.t, *j);
                    if (!e) {
                        return JSONError::invalid_number;
                    }
                    return true;
                };
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
                    auto e = escape::escape_str(*s, out.t, escape ? escape::EscapeFlag::utf : escape::EscapeFlag::none);
                    if (!e) {
                        return JSONError::invalid_escape;
                    }
                    out.t.push_back('\"');
                    return true;
                }
                else if (auto o = holder.as_obj()) {
                    for (auto& kv : *o) {
                        std::get<0>(kv);
                    }
                    out.write_raw("{", "\n");
                    out.write_indent();
                }
            }
        }  // namespace json
    }      // namespace net
}  // namespace utils
