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
            once_in_cmd = 0x8,     // allow only once to set
        };

        template <class String, template <class...> class Vec>
        struct Option {
           protected:
            OptionType type_;
            Option(OptionType t)
                : type_(t) {}

           public:
            OptionAttribute attr = OptionAttribute::none;

            OptionType type() {
                return type_;
            }
        };

        template <class String, template <class...> class Vec>
        struct BoolOption : Option<String, Vec> {
            BoolOption()
                : Option<String, Vec>(OptionType::boolean) {}
            bool value = false;
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

    }  // namespace cmdline
}  // namespace utils
