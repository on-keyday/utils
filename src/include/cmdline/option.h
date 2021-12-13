/*license*/

// option - command line option
#pragma once

#include <cstdint>

namespace utils {
    namespace cmdline {
        enum class OptionType {
            boolean,
            integer,
            string,
            somevalue_int,
            somevalue_string,
        };

        enum class OptionAttribute {
            none = 0,
            required = 0x1,     // option must set
            must_assign = 0x2,  // value must set with `=`
        };

        template <class String, template <class...> class Vec>
        struct Option {
           protected:
            OptionType type_;
            Option(OptionType t)
                : type_(t) {}

           public:
            String name;
            OptionAttribute attr = OptionAttribute::none;

            OptionType type() {
                return type_;
            }
        };

        template <class String, template <class...> class Vec>
        struct BoolOption : Option<String, Vec> {
            BoolOption()
                : Option<String, Vec>(OptionType::boolean) {}
            bool flag = false;
        };

        template <class String, template <class...> class Vec>
        struct IntOption : Option<String, Vec> {
            IntOption()
                : Option<String, Vec>(OptionType::integer) {}
            std::int64_t value = 0;
        };

        template <class String, template <class...> class Vec>
        struct StringOption : Option<String, Vec> {
            StringOption()
                : Option<String, Vec>(OptionType::string) {}
            String value;
        };

        template <class String, template <class...> class Vec>
        struct SomeValueIntOption : Option<String, Vec> {
            SomeValueIntOption()
                : Option<String, Vec>(OptionType::somevalue_int) {}
            Vec<std::int64_t> values;
            size_t minimum = 0;
            size_t maximum = 0;
        };

        template <class String, template <class...> class Vec>
        struct SomeValueStringOption : Option<String, Vec> {
            SomeValueStringOption()
                : Option<String, Vec>(OptionType::somevalue_string) {}
            Vec<String> values;
            size_t minimum = 0;
            size_t maximum = 0;
        };

    }  // namespace cmdline
}  // namespace utils
