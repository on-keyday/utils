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
