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
                bool parse(SafeVal<Value>& val, CmdParseState& state, bool reserved, size_t count) {
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
                    auto t = set();
                    if (t == -1) {
                        state.state = FlagType::not_accepted;
                        return false;
                    }
                    if (!val.set_value(t == 1)) {
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
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
