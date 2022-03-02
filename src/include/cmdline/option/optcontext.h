/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optcontext - option context
#pragma once
#include "parse.h"
#include "option_set.h"
#include "parsers.h"
#include "help.h"
namespace utils {
    namespace cmdline {
        namespace option {
            enum class CustomFlag {
                none = 0,
                appear_once = 0x1,
                bind_once = 0x2,
            };

            DEFINE_ENUM_FLAGOP(CustomFlag)

            inline OptParser bind_custom(OptParser&& ps, CustomFlag flag) {
                if (any(CustomFlag::appear_once & flag)) {
                    ps = OnceParser{std::move(ps)};
                }
                return std::move(ps);
            }

            struct Context {
               private:
                Description desc;
                Results result;

                template <class Ctx, class Arg>
                friend FlagType parse(int argc, char** argv, Ctx& ctx, Arg& arg, ParseFlag flag, int start_index);

               public:
                template <class Str>
                void help(Str& str, ParseFlag flag, const char* indent = "    ") {
                    option::desc(str, flag, desc.list, indent);
                }

                template <class Str>
                Str help(ParseFlag flag, const char* indent = "    ") {
                    return option::desc<Str>(flag, desc.list, indent);
                }

                template <class Str>
                void Usage(Str& str, ParseFlag flag, const char* cmdname, const char* usage = "[option]", const char* indent = "    ") {
                    helper::appends(str, "Usage:\n",
                                    indent, cmdname, " ", usage, "\nOption:\n");
                    help(str, flag, indent);
                }

                template <class Str>
                Str Usage(ParseFlag flag, const char* cmdname, const char* usage = "[option]", const char* indent = "    ") {
                    Str str;
                    Usage(str, flag, cmdname, usage, indent);
                    return str;
                }

                auto& erropt() {
                    return result.erropt;
                }

                int errindex() {
                    return result.index;
                }

                bool custom_option(auto&& option, OptParser parser, auto&& help, auto&& argdesc, bool bindonce) {
                    return desc.set(option, std::move(parser), help, argdesc, bindonce) != nullptr;
                }

                template <class T>
                std::remove_pointer_t<T>* custom_option_reserved(T val, auto&& option, OptParser parser, auto&& help, auto&& argdesc, bool bindonce) {
                    auto opt = desc.set(option, std::move(parser), help, argdesc, bindonce);
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
                std::remove_pointer_t<T>* Option(auto&& option, T defaultv, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag) {
                    return custom_option_reserved(
                        std::move(defaultv), option,
                        bind_custom(std::move(ps), flag), help, argdesc, any(flag & CustomFlag::bind_once));
                }

                bool UnboundOption(auto&& option, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag) {
                    return custom_option(option, bind_custom(std::move(ps), flag), help, argdesc, any(flag & CustomFlag::bind_once));
                }

                bool UnboundBool(auto&& option, auto&& help, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(option, BoolParser{.to_set = true, .rough = rough}, help, "", flag);
                }

                bool UnboundInt(auto&& option, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(option, IntParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str>
                bool UnboundString(auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(option, StringParser<Str>{}, help, argdesc, flag);
                }

                template <class T = std::uint64_t, template <class...> class Vec = wrap::vector>
                bool UnboundVecInt(auto&& option, size_t len, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(
                        option, VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                        help, argdesc, flag);
                }

                template <class Str, template <class...> class Vec = wrap::vector>
                bool UnboundVecString(auto&& option, size_t len, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(
                        option, VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                        help, argdesc, flag);
                }

                bool VarBool(bool* ptr, auto& option, auto&& help, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        option, ptr, BoolParser{.to_set = !*ptr, .rough = rough},
                        help, "", flag);
                }

                template <std::integral T>
                bool VarInt(T* ptr, auto& option, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        ptr, option, IntParser{.radix = radix},
                        help, argdesc, flag);
                }

                template <class Str>
                bool VarString(Str* ptr, auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
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
                    return (bool)Option(option, ptr, VectorParser{.parser = IntParser{.radix = radix}, .len = len},
                                        help, argdesc, flag);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                bool VarVecString(Vec<Str>* ptr, auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    return (bool)Option(option, ptr,
                                        VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                                        help, argdesc, flag);
                }

                bool* Bool(auto&& option, bool defaultv, auto&& help, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return Option(
                        option, defaultv,
                        BoolParser{.to_set = !defaultv, .rough = rough},
                        help, "", flag);
                }

                template <std::integral T = std::int64_t>
                T* Int(auto&& option, T defaultv, auto&& help, auto&& argdesc, int radix = 10, CustomFlag flag = CustomFlag::none) {
                    return Option(
                        option, defaultv,
                        IntParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str = wrap::string>
                Str* String(auto&& option, Str defaultv, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return Option(option, std::move(defaultv),
                                  StringParser<Str>{}, help, argdesc, flag);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                Vec<Str>* VecString(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, Vec<Str>&& defaultv = Vec<Str>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return Option(option, std::move(defaultv),
                                  VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                                  help, argdesc, flag);
                }

                template <std::integral T = std::int64_t, template <class...> class Vec = wrap::vector>
                Vec<T>* VecInt(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10, Vec<T>&& defaultv = Vec<T>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return Option(option, std::move(defaultv),
                                  VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                                  help, argdesc, flag);
                }

                template <class Flag>
                Flag* FlagSet(auto&& option, Flag mask, auto&& help, bool rough = true, CustomFlag flag = CustomFlag::none, Flag defaultv = Flag{}) {
                    return Option(
                        option, std::move(defaultv),
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, "", flag);
                }

                template <class Flag>
                bool VarFlagSet(Flag* ptr, auto&& option, Flag mask, auto&& help, bool rough = true, CustomFlag flag = CustomFlag::none) {
                    return (bool)Option(
                        option, ptr,
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, "", flag);
                }

                auto find(auto&& optname) {
                    return result.find(optname);
                }

                template <class T>
                T value_or_not(auto&& name, T or_not = {}, size_t index = 0) {
                    return result.value_or_not<T>(name, std::forward<T>(or_not), index);
                }

                template <class T>
                T value(auto&& name, size_t index = 0) {
                    return value_or_not<T>(name, {}, index);
                }

                template <class T>
                T* value_ptr(auto&& name, size_t index = 0) {
                    return result.value_ptr<T>(name, index);
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
