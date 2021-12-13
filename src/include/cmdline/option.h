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
            required = 0x1,        // option must set
            must_assign = 0x2,     // value must set with `=`
            no_option_like = 0x4,  // some value disallow option like string
        };

        template <class String>
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

        template <class String>
        struct BoolOption : Option<String> {
            BoolOption()
                : Option<String>(OptionType::boolean) {}
            bool flag = false;
        };

        template <class String>
        struct IntOption : Option<String> {
            IntOption()
                : Option<String>(OptionType::integer) {}
            std::int64_t value = 0;
        };

        template <class String>
        struct StringOption : Option<String> {
            StringOption()
                : Option<String, Vec>(OptionType::string) {}
            String value;
        };

        template <class String>
        struct SomeValueIntOption : Option<String> {
            SomeValueIntOption()
                : Option<String>(OptionType::somevalue_int) {}
            size_t minimum = 0;
            size_t maximum = 0;
        };

        template <class String>
        struct SomeValueStringOption : Option<String> {
            SomeValueStringOption()
                : Option<String>(OptionType::somevalue_string) {}
            size_t minimum = 0;
            size_t maximum = 0;
        };

    }  // namespace cmdline
}  // namespace utils
