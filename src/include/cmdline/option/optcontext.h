/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optcontext - option context
#pragma once
#include "parse.h"
#include "option_set.h"
#include "parsers.h"
#include "help.h"
#include <concepts>
namespace futils {
    namespace cmdline {
        namespace option {
            enum class CustomFlag {
                none = 0,
                appear_once = 0x1,
                bind_once = 0x2,
                required = 0x4,
                hidden = 0x8,
            };

            DEFINE_ENUM_FLAGOP(CustomFlag)

            inline OptParser bind_custom(OptParser&& ps, CustomFlag flag) {
                if (any(CustomFlag::appear_once & flag)) {
                    ps = OnceParser{std::move(ps)};
                }
                return std::move(ps);
            }

            inline OptMode convert_flag(CustomFlag flag) {
                OptMode mode = OptMode::none;
                if (any(flag & CustomFlag::bind_once)) {
                    mode |= OptMode::bindonce;
                }
                if (any(flag & CustomFlag::required)) {
                    mode |= OptMode::required;
                }
                if (any(flag & CustomFlag::hidden)) {
                    mode |= OptMode::hidden;
                }
                return mode;
            }

            constexpr bool perfect_parsed(FlagType ty) {
                return ty == FlagType::end_of_arg || ty == FlagType::required;
            }

            struct Context {
               private:
                Description desc;
                Results result;

                template <class Ctx, class Arg>
                friend FlagType parse(int argc, char** argv, Ctx& ctx, Arg& arg, ParseFlag flag, int start_index);

               public:
                FlagType check_required() {
                    for (auto& opt : desc.list) {
                        if (any(opt->mode & OptMode::required)) {
                            auto found = result.find(opt->mainname);
                            if (found.empty()) {
                                auto reserved = result.reserved.find(opt->mainname);
                                if (reserved != result.reserved.end()) {
                                    if (get<1>(*reserved).set_count) {
                                        continue;
                                    }
                                }
                                result.erropt = opt->mainname;
                                return FlagType::required;
                            }
                        }
                    }
                    return FlagType::end_of_arg;
                }

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
                    strutil::appends(str, "Usage:\n",
                                     indent, cmdname, " ", usage, "\n");
                    if (desc.list.size()) {
                        strutil::append(str, "Option:\n");
                        help(str, flag, indent);
                    }
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

                bool custom_option(auto&& option, OptParser parser, auto&& help, auto&& argdesc, OptMode mode) {
                    return desc.set(option, std::move(parser), help, argdesc, mode) != nullptr;
                }

                template <class T>
                std::remove_pointer_t<T>* custom_option_reserved(T val, auto&& option, OptParser parser, auto&& help, auto&& argdesc, OptMode mode) {
                    auto opt = desc.set(option, std::move(parser), help, argdesc, mode);
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
                std::remove_pointer_t<T>* Option(auto&& option, T defaultv, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return custom_option_reserved(
                        std::move(defaultv), option,
                        bind_custom(std::move(ps), flag), help, argdesc, convert_flag(flag));
                }

                bool UnboundOption(auto&& option, OptParser ps, auto&& help, auto&& argdesc, CustomFlag flag) {
                    return custom_option(option, bind_custom(std::move(ps), flag), help, argdesc, convert_flag(flag));
                }

                bool UnboundBool(auto&& option, auto&& help, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    return UnboundOption(option, BoolParser{.to_set = true, .rough = rough}, help, "", flag);
                }

                bool UnboundInt(auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return UnboundOption(option, IntParser{.radix = radix}, help, argdesc, flag);
                }

                bool UnboundFloat(auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return UnboundOption(option, FloatParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str = wrap::string, bool view_like = false>
                bool UnboundString(auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return UnboundOption(option, StringParser<Str, view_like>{}, help, argdesc, flag);
                }

                template <class T = std::uint64_t, template <class...> class Vec = wrap::vector>
                    requires std::is_integral_v<T>
                bool UnboundVecInt(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return UnboundOption(
                        option, VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                        help, argdesc, flag);
                }

                template <class T = double, template <class...> class Vec = wrap::vector>
                    requires std::is_floating_point_v<T>
                bool UnboundVecFloat(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return UnboundOption(
                        option, VectorParser<T, Vec>{.parser = FloatParser{.radix = radix}, .len = len},
                        help, argdesc, flag);
                }

                template <class Str = wrap::string, template <class...> class Vec = wrap::vector>
                bool UnboundVecString(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return UnboundOption(
                        option, VectorParser<Str, Vec>{.parser = StringParser<Str>{}, .len = len},
                        help, argdesc, flag);
                }

                bool VarBool(bool* ptr, auto&& option, auto&& help, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        option, ptr, BoolParser{.to_set = !*ptr, .rough = rough},
                        help, "", flag);
                }

                template <class T>
                    requires std::is_integral_v<T>
                bool VarInt(T* ptr, auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        option, ptr, IntParser{.radix = radix},
                        help, argdesc, flag);
                }

                template <class T>
                    requires std::is_floating_point_v<T>
                bool VarFloat(T* ptr, auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        option, ptr, FloatParser{.radix = radix},
                        help, argdesc, flag);
                }

                template <bool view_like = false, class Str>
                bool VarString(Str* ptr, auto&& option, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    if (!ptr) {
                        return false;
                    }
                    return (bool)Option(
                        option, ptr,
                        StringParser<Str, view_like>{},
                        help, argdesc, flag);
                }

                template <class T, template <class...> class Vec>
                    requires std::is_integral_v<T>
                bool VarVecInt(Vec<T>* ptr, auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    if (!ptr) {
                        return false;
                    }
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    return (bool)Option(option, ptr, VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                                        help, argdesc, flag);
                }

                template <class T, template <class...> class Vec>
                    requires std::is_floating_point_v<T>
                bool VarVecFloat(Vec<T>* ptr, auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    if (!ptr) {
                        return false;
                    }
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    return (bool)Option(option, ptr, VectorParser<T, Vec>{.parser = FloatParser{.radix = radix}, .len = len},
                                        help, argdesc, flag);
                }

                template <class Str, template <class...> class Vec>
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

                bool* Bool(auto&& option, bool defaultv, auto&& help, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    return Option(
                        option, defaultv,
                        BoolParser{.to_set = !defaultv, .rough = rough},
                        help, "", flag);
                }

                template <class T = std::int64_t>
                    requires std::is_integral_v<T>
                T* Int(auto&& option, T defaultv, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return Option(
                        option, defaultv,
                        IntParser{.radix = radix}, help, argdesc, flag);
                }

                template <class T = double>
                    requires std::is_floating_point_v<T>
                T* Float(auto&& option, T defaultv, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10) {
                    return Option(
                        option, defaultv,
                        FloatParser{.radix = radix}, help, argdesc, flag);
                }

                template <class Str = wrap::string, bool view_like = false>
                Str* String(auto&& option, Str defaultv, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none) {
                    return Option(option, std::move(defaultv),
                                  StringParser<Str, view_like>{}, help, argdesc, flag);
                }

                template <class T = std::int64_t, template <class...> class Vec = wrap::vector>
                    requires std::is_integral_v<T>
                Vec<T>* VecInt(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10, Vec<T>&& defaultv = Vec<T>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return Option(option, std::move(defaultv),
                                  VectorParser<T, Vec>{.parser = IntParser{.radix = radix}, .len = len},
                                  help, argdesc, flag);
                }

                template <class T = double, template <class...> class Vec = wrap::vector>
                    requires std::is_floating_point_v<T>
                Vec<T>* VecFloat(auto&& option, size_t len, auto&& help, auto&& argdesc, CustomFlag flag = CustomFlag::none, int radix = 10, Vec<T>&& defaultv = Vec<T>{}) {
                    if (defaultv.size() < len) {
                        defaultv.resize(len);
                    }
                    return Option(option, std::move(defaultv),
                                  VectorParser<T, Vec>{.parser = FloatParser{.radix = radix}, .len = len},
                                  help, argdesc, flag);
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

                template <class Flag>
                Flag* FlagSet(auto&& option, Flag mask, auto&& help, CustomFlag flag = CustomFlag::none, bool rough = true, Flag defaultv = Flag{}) {
                    return Option(
                        option, std::move(defaultv),
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, "", flag);
                }

                template <class Flag>
                bool VarFlagSet(Flag* ptr, auto&& option, Flag mask, auto&& help, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    return (bool)Option(
                        option, ptr,
                        FlagMaskParser<Flag>{.mask = mask, .rough = rough},
                        help, "", flag);
                }

                template <class State, class F>
                State* Func(auto&& option, auto&& help, auto&& argdesc, State state, F&& f, CustomFlag flag = CustomFlag::none) {
                    return Option(option, std::move(state),
                                  FuncParser<std::decay_t<F>, State>{std::forward<F>(f)},
                                  help, argdesc, flag);
                }

                template <class State = void, class F>
                bool VarFunc(State* ptr, auto&& option, auto&& help, auto&& argdesc, F&& f, CustomFlag flag = CustomFlag::none) {
                    return (bool)Option(option, ptr,
                                        FuncParser<std::decay_t<F>, State>{std::forward<F>(f)},
                                        help, argdesc, flag);
                }

                template <class State, class F>
                State* BoolFunc(auto&& option, auto&& help, State state, F&& f, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    return Option(option, std::move(state),
                                  BoolFuncParser<std::decay_t<F>, State>{std::forward<F>(f), rough},
                                  help, "", flag);
                }

                template <class State = void, class F>
                bool VarBoolFunc(State* ptr, auto&& option, auto&& help, F&& f, CustomFlag flag = CustomFlag::none, bool rough = true) {
                    return (bool)Option(option, ptr,
                                        BoolFuncParser<std::decay_t<F>, State>{std::forward<F>(f), rough},
                                        help, "", flag);
                }

                template <class Key, class Value, template <class...> class M>
                Value* Map(auto&& option, auto&& help, auto&& argdesc, M<Key, Value>&& mapping, CustomFlag flag = CustomFlag::none, Value&& defaultv = Value()) {
                    return Option(option, std::move(defaultv),
                                  MappingParser<Key, Value, M>{std::move(mapping)},
                                  help, argdesc, flag);
                }

                template <class Key, class Value, template <class...> class M>
                bool VarMap(Value* ptr, auto&& option, auto&& help, auto&& argdesc, M<Key, Value>&& mapping, CustomFlag flag = CustomFlag::none) {
                    return (bool)Option(option, ptr,
                                        MappingParser<Key, Value, M>{std::move(mapping)},
                                        help, argdesc, flag);
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

                bool is_set(auto&& name, size_t index = 0) {
                    if (Result* v = result.get_result(name, index)) {
                        return v->set_count != 0;
                    }
                    return false;
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace futils
