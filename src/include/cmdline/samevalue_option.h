/*license*/

// somevalue_option - `somevalue` type option
#pragma once

#include "option.h"

namespace utils {
    namespace cmdline {

        template <class T, class String, template <class...> class Vec>
        struct SomeValueOption : Option<String> {
           protected:
            OptionType has_type;
            SomeValueOption(OptionType h)
                : has_type(h), Option<String>(OptionType::somevalue) {}

           public:
            Vec<T> values;
            size_t minimum = 0;
            size_t maximum = 0;
        };
        template <class String, template <class...> class Vec>
        struct IntSomeValueOption : SomeValueOption<std::int64_t, String, Vec> {
            IntSomeValueOption()
                : SomeValueOption<std::int64_t, String, Vec>(OptionType::integer) {}
        };
    }  // namespace cmdline
}  // namespace utils