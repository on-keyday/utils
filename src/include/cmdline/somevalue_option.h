/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// somevalue_option - `somevalue` type option
#pragma once

#include "option.h"

namespace utils {
    namespace cmdline {

        template <class String, template <class...> class Vec>
        struct SomeValueOption : Option<String, Vec> {
           protected:
            SomeValueOption(OptionType t)
                : has_type_(t), Option<String, Vec>(OptionType::somevalue) {}
            OptionType has_type_;

           public:
            OptionType has_type() {
                return has_type;
            }

            size_t minimum = 0;
            size_t maximum = 0;
        };

        template <class T, class String, template <class...> class Vec>
        struct SomeValueOptionBase : public SomeValueOption<String, Vec> {
           protected:
            SomeValueOptionBase(OptionType h)
                : SomeValueOption<String, Vec>(h) {}

           public:
            Vec<T> values;
        };

        template <class String, template <class...> class Vec>
        struct BoolSomeValueOption : SomeValueOptionBase<std::int64_t, String, Vec> {
            BoolSomeValueOption()
                : SomeValueOptionBase<bool, String, Vec>(OptionType::boolean) {}
        };

        template <class String, template <class...> class Vec>
        struct IntSomeValueOption : SomeValueOptionBase<std::int64_t, String, Vec> {
            IntSomeValueOption()
                : SomeValueOptionBase<std::int64_t, String, Vec>(OptionType::integer) {}
        };

        template <class String, template <class...> class Vec>
        struct StringSomeValueOption : SomeValueOptionBase<std::int64_t, String, Vec> {
            StringSomeValueOption()
                : SomeValueOptionBase<String, String, Vec>(OptionType::string) {}
        };
    }  // namespace cmdline
}  // namespace utils
