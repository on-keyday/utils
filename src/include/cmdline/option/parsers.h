/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parsers - default parsers
#pragma once
#include "flag.h"
#include "optparser.h"
#include "../../number/parse.h"
#include "../../utf/convert.h"

namespace utils {
    namespace cmdline {
        namespace option {
            struct BoolParser {
                bool to_set = true;
                bool rough = false;

                static bool parse(bool& val, CmdParseState& state, bool to_set, bool rough) {
                    auto set = [&] {
                        if (state.val) {
                            if (helper::equal(state.val, "true")) {
                                return 1;
                            }
                            else if (helper::equal(state.val, "false")) {
                                return 0;
                            }
                            if (rough) {
                                if (helper::equal(state.val, "true", helper::ignore_case()) ||
                                    helper::equal(state.val, "T", helper::ignore_case()) ||
                                    helper::equal(state.val, "on", helper::ignore_case())) {
                                    return 1;
                                }
                                if (helper::equal(state.val, "false", helper::ignore_case()) ||
                                    helper::equal(state.val, "F", helper::ignore_case()) ||
                                    helper::equal(state.val, "off", helper::ignore_case())) {
                                    return 0;
                                }
                            }
                            return -1;
                        }
                        else {
                            return to_set ? 1 : 0;
                        }
                    };
                    auto i = set();
                    if (i == -1) {
                        return false;
                    }
                    val = i == 1;
                    return true;
                }

                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    bool flag = false;
                    if (!parse(flag, state, to_set, rough)) {
                        state.state = FlagType::not_accepted;
                        return false;
                    }
                    if (!val.set_value(flag)) {
                        state.state = FlagType::type_not_match;
                        return false;
                    }
                    return true;
                }
            };
#define GET_VALUE(TYPE)                             \
    else if (val.get_ptr<TYPE>()) {                 \
        TYPE t;                                     \
        if (!number::parse_integer(v, t, radix)) {  \
            state.state = FlagType::not_accepted;   \
            return false;                           \
        }                                           \
        if (!val.set_value(t)) {                    \
            state.state = FlagType::type_not_match; \
            return false;                           \
        }                                           \
        return true;                                \
    }

            struct IntParser {
                int radix = 10;
                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    const char* v = nullptr;
                    if (state.val) {
                        v = state.val;
                    }
                    else {
                        v = state.argv[state.arg_track_index];
                        state.arg_track_index++;
                    }
                    if (!v) {
                        state.state = FlagType::require_more_argument;
                        return false;
                    }
                    if (!reserved) {
                        std::int64_t i = 0;
                        if (!number::parse_integer(v, i, radix)) {
                            state.state = FlagType::not_accepted;
                            return false;
                        }
                        if (!val.set_value(i)) {
                            state.state = FlagType::type_not_match;
                            return false;
                        }
                    }
                    GET_VALUE(signed char)
                    GET_VALUE(unsigned char)
                    GET_VALUE(signed short)
                    GET_VALUE(unsigned short)
                    GET_VALUE(signed int)
                    GET_VALUE(unsigned int)
                    GET_VALUE(signed long)
                    GET_VALUE(unsigned long)
                    GET_VALUE(signed long long)
                    GET_VALUE(unsigned long long)
                    GET_VALUE(char32_t)
                    GET_VALUE(char16_t)
#ifdef __cpp_char8_t
                    GET_VALUE(char8_t)
#endif

                    state.state = FlagType::type_not_match;
                    return false;
                }
            };
#undef GET_VALUE

            template <class String>
            struct StringParser {
                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    const char* v = nullptr;
                    if (state.val) {
                        v = state.val;
                    }
                    else {
                        v = state.argv[state.arg_track_index];
                        state.arg_track_index++;
                    }
                    if (!v) {
                        state.state = FlagType::require_more_argument;
                        return false;
                    }
                    if (!val.set_value(utf::convert<String>(v))) {
                        state.state = FlagType::type_not_match;
                        return false;
                    }
                    return true;
                }
            };

            struct OnceParser {
                OptParser parser;

                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    if (count) {
                        state.state = FlagType::require_once;
                        return false;
                    }
                    return parser.parse(val, state, reserved, count);
                }
            };

            template <class T, template <class...> class Vec>
            struct VectorParser {
                OptParser parser;
                size_t len = 0;

                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    Vec<T>* ptr = nullptr;
                    if (!reserved) {
                        ptr = val.get_ptr<Vec<T>>();
                        if (!ptr) {
                            val.set_value(Vec<T>{});
                        }
                    }
                    ptr = val.get_ptr<Vec<T>>();
                    if (!ptr) {
                        state.state = FlagType::type_not_match;
                        return false;
                    }
                    T tmp;
                    SafeVal<Value> v(&tmp, true);
                    if (ptr->size() < len) {
                        ptr->resize(len);
                    }
                    for (size_t i = 0; i < len; i++) {
                        if (state.val) {
                            if (!parser.parse(v, state, true, len)) {
                                return false;
                            }
                            (*ptr)[i] = std::move(tmp);
                        }
                        else {
                            state.val = state.argv[state.arg_track_index];
                            if (!state.val) {
                                state.state = FlagType::require_more_argument;
                                return false;
                            }
                            state.arg_track_index++;
                            if (!parser.parse(v, state, true, len)) {
                                return false;
                            }
                            (*ptr)[i] = std::move(tmp);
                        }
                        state.val = nullptr;
                    }
                    return true;
                }
            };

            template <class Flag>
            struct FlagMaskParser {
                Flag mask;
                bool rough = false;
                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
                    bool flag = false;
                    if (!BoolParser::parse(flag, state, true, rough)) {
                        return false;
                    }
                    auto ptr = val.get_ptr<Flag>();
                    if (!ptr) {
                        state.state = FlagType::type_not_match;
                        return false;
                    }
                    if (flag) {
                        *ptr |= mask;
                    }
                    else {
                        *ptr &= ~mask;
                    }
                    return true;
                }
            };
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
