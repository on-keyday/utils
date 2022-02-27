/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optcontext - option context
#pragma once
#include "option_set.h"
#include "parsers.h"
namespace utils {
    namespace cmdline {
        namespace option {
            struct Context {
                Description desc;
                Results result;

                bool custom_option(auto&& option, OptParser parser, auto&& help, auto&& argdesc) {
                    return desc.set(option, std::move(parser), help, argdesc) != nullptr;
                }

                template <class T>
                std::remove_pointer_t<T>* custom_option_reserved(T val, auto&& option, OptParser parser, auto&& help, auto&& argdesc) {
                    auto opt = desc.set(option, std::move(parser), help, argdesc);
                    if (!opt) {
                        return nullptr;
                    }
                    Result& area = result.reserved[opt->mainname];
                    area.desc = opt;
                    area.value = SafeVal<Value>(std::move(val), true);
                    area.as_name = opt->mainname;
                    area.set_count = 0;
                    using ret_t = std::remove_pointer_t<T>;
                    auto p = area.value.get_ptr<ret_t>();
                    assert(p);
                    return p;
                }

                bool VarBool(bool* ptr, auto& option, auto&& help, auto&& argdesc, bool rough = true) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)custom_option_reserved(
                        ptr, option, BoolParser{.to_set = !*ptr, .rough = rough},
                        help, argdesc);
                }

                template <std::integral T = std::int64_t>
                bool VarInt(T* ptr, auto& option, auto&& help, auto&& argdesc, int radix = 10) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)custom_option_reserved(
                        ptr, option, IntParser{.radix = radix},
                        help, argdesc);
                }

                template <class Str>
                bool VarStr(Str* ptr, auto&& option, auto&& help, auto&& argdesc) {
                    return (bool)custom_option_reserved(
                        ptr, option,
                        StringParser<Str>{},
                        help, argdesc);
                }

                bool* Bool(auto&& option, bool defaultv, auto&& help, auto&& argdesc, bool rough = true) {
                    return custom_option_reserved(
                        defaultv, option,
                        BoolParser{.to_set = !defaultv, .rough = rough},
                        help, argdesc);
                }

                template <std::integral T = std::int64_t>
                T* Int(auto&& option, T defaultv, auto&& help, auto&& argdesc, int radix = 10) {
                    return custom_option_reserved(
                        defaultv, option,
                        IntParser{.radix = radix}, help, argdesc);
                }

                template <class Str = wrap::string>
                Str* String(auto&& option, Str defaultv, auto&& help, auto&& argdesc) {
                    return custom_option_reserved(
                        std::move(defaultv), option,
                        StringParser<Str>{}, help, argdesc);
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
