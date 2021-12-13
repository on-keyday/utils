/*license*/

// somevalue_option - `somevalue` type option
#pragma once

#include "option.h"

namespace utils {
    namespace cmdline {

        template <class String, template <class...> class Vec>
        struct SomeValueOption : Option<String, Vec> {
            SomeValueOption(OptionType t)
                : has_type(t), Option<String, Vec>(OptionType::somevalue) {}
            OptionType has_type;
        };

        template <class T, class String, template <class...> class Vec>
        struct SomeValueOptionBase : protected SomeValueOption<String, Vec> {
           protected:
            SomeValueOptionBase(OptionType h)
                : SomeValueOption<String, Vec>(h) {}

           public:
            Vec<T> values;
            size_t minimum = 0;
            size_t maximum = 0;
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
