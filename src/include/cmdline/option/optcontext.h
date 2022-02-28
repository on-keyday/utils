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
            enum class CustomFlag {
                none = 0,
                with_once = 0x1,
            };

            DEFINE_ENUM_FLAGOP(CustomFlag)

            inline OptParser bind_custom(OptParser&& ps, CustomFlag flag) {
                if (any(CustomFlag::with_once & flag)) {
                    ps = OnceParser{std::move(ps)};
                }
                return std::move(ps);
            }

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
                std::remove_pointer_t<T>* value(auto&& option, T defaultv, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag) {
                    return custom_option_reserved(
                        std::move(defaultv), option,
                        bind_custom(std::move(ps), flag), help, argdesc);
                }

                bool unbound_value(auto&& option, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag) {
                    return custom_option(option, bind_custom(std::move(ps), flag), help, argdesc);
                }

                bool UnboundBool(auto&& option, auto&& help, auto&& argdesc, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return unbound_value(option, BoolParser{.to_set = true, .rough = rough}, help, argdesc, flag);
                }

                bool UnboundInt(auto&& option, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return unbound_value(option, IntParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str>
                bool UnboundString(auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return unbound_value(option, StringParser<Str>{}, help, argdesc, flag);
                }

                bool VarBool(bool* ptr, auto& option, auto&& help, auto&& argdesc, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        option, ptr, BoolParser{.to_set = !*ptr, .rough = rough},
                        help, argdesc, flag);
                }

                template <std::integral T = std::int64_t>
                bool VarInt(T* ptr, auto& option, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        ptr, option, IntParser{.radix = radix},
                        help, argdesc, flag);
                }

                template <class Str>
                bool VarStr(Str* ptr, auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)value(
                        option, ptr,
                        StringParser<Str>{},
                        help, argdesc, flag);
                }

                template <std::integral T, template <class...> class Vec>
                bool VarVecInt(Vec<T>* ptr, auto&& option, size_t len, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    return (bool)value(option, ptr, VectorParser{.parser = IntParser{.radix = radix}, .len = len},
                                       help, argdesc, flag);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                bool VecString(Vec<Str>* ptr, auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    return (bool)value(option, ptr,
                                       VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                                       help, argdesc, flag);
                }

                bool* Bool(auto&& option, bool defaultv, auto&& help, auto&& argdesc, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return value(
                        option, defaultv,
                        BoolParser{.to_set = !defaultv, .rough = rough},
                        help, argdesc, flag);
                }

                template <std::integral T = std::int64_t>
                T* Int(auto&& option, T defaultv, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return value(
                        option, defaultv,
                        IntParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str = wrap::string>
                Str* String(auto&& option, Str defaultv, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return value(option, std::move(defaultv),
                                 StringParser<Str>{}, help, argdesc, flag);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                Vec<Str>* VecString(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, Vec<Str>&& defaultv = Vec<Str>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return value(option, std::move(defaultv),
                                 VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                                 help, argdesc, flag);
                }

                template <std::integral T = std::int64_t, template <class...> class Vec = wrap::vector>
                Vec<T>* VecInt(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10, Vec<T>&& defaultv = Vec<T>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return value(option, std::move(defaultv),
                                 VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                                 help, argdesc, flag);
                }

                template <class Flag>
                Flag* FlagSet(auto&& option, Flag mask, auto&& help, auto&& argdesc, bool rough = true, CustomFlag flag = CustomFlag::none, Flag defaultv = Flag{}) {
                    return value(
                        option, std::move(defaultv),
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, argdesc, flag);
                }

                template <class Flag>
                bool VarFlagSet(Flag* ptr, auto&& option, Flag mask, auto&& help, auto&& argdesc, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return (bool)value(
                        option, ptr,
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, argdesc, flag);
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
