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
            somevalue,
        };

        enum class OptionAttribute {
            none = 0,
            required = 0x1,        // option must set
            must_assign = 0x2,     // value must set with `=`
            no_option_like = 0x4,  // `somevalue` type disallow string like `option`
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
                : Option<String>(OptionType::string) {}
            String value;
        };

    }  // namespace cmdline
}  // namespace utils
