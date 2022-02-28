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
                    return (bool)desc.set(option, std::move(parser), help, argdesc) != nullptr;
                }

                template <class T>
                std::remove_pointer_t<T>* custom_option_reserved(T val, auto&& option, OptParser parser, auto&& help, auto&& argdesc) {
                    auto opt = desc.set(option, std::move(parser), help, argdesc);
                    if (!opt) {
                        return nullptr;
                    }
                    Result& area = result.reserved[opt->mainname];
                    area.desc = opt;
                    area.value = SafeVal<option::Value>(std::move(val), true);
                    area.as_name = opt->mainname;
                    area.set_count = 0;
                    using ret_t = std::remove_pointer_t<T>;
                    auto p = area.value.get_ptr<ret_t>();
                    assert(p);
                    return p;
                }

                template <class T>
                std::remove_pointer_t<T>* value(auto&& option, T defaultv, OptParser ps, auto&& help, auto&& argdesc, bool once = false) {
                    if (once) {
                        ps = OnceParser{std::move(ps)};
                    }
                    return custom_option_reserved(
                        std::move(defaultv), option,
                        std::move(ps), help, argdesc);
                }

                bool VarBool(bool* ptr, auto& option, auto&& help, auto&& argdesc, bool rough = true, bool once = false) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        option, ptr, BoolParser{.to_set = !*ptr, .rough = rough},
                        help, argdesc, once);
                }

                template <std::integral T = std::int64_t>
                bool VarInt(T* ptr, auto& option, auto&& help, auto&& argdesc, int radix = 10, bool once = false) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        ptr, option, IntParser{.radix = radix},
                        help, argdesc, once);
                }

                template <class Str>
                bool VarStr(Str* ptr, auto&& option, auto&& help, auto&& argdesc, bool once = false) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        option, ptr,
                        StringParser<Str>{},
                        help, argdesc, once);
                }

                bool* Bool(auto&& option, bool defaultv, auto&& help, auto&& argdesc, bool rough = true, bool once = false) {
                    return value(
                        option, defaultv,
                        BoolParser{.to_set = !defaultv, .rough = rough},
                        help, argdesc, once);
                }

                template <std::integral T = std::int64_t>
                T* Int(auto&& option, T defaultv, auto&& help, auto&& argdesc, int radix = 10, bool once = false) {
                    return value(
                        option, defaultv,
                        IntParser{.radix = radix}, help, argdesc, once);
                }

                template <class Str = wrap::string>
                Str* String(auto&& option, Str defaultv, auto&& help, auto&& argdesc, bool once = false) {
                    return value(option, std::move(defaultv),
                                 StringParser<Str>{}, help, argdesc, once);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                Vec<Str>* VecString(auto&& option, size_t len, auto&& help, auto&& argdesc, bool once = false, Vec<Str>&& defaultv = Vec<Str>{}) {
                    return value(option, std::move(defaultv),
                                 VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                                 help, argdesc, once);
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
