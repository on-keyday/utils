/*license*/

// option_result - option parsed result
#pragma once

#include "option.h"

#include "../wrap/lite/smart_ptr.h"

namespace utils {
    namespace cmdline {

        template <class String, template <class...> class Vec>
        struct OptionResult {
            OptionType type_;
        };

        template <class String, template <class...> class Vec>
        struct IntOptionResult : OptionResult<String, Vec> {
            IntOption<String>* base = nullptr;
            std::int64_t value;
        };

        template <class String, template <class...> class Vec>
        struct StringOptionResult : OptionResult<String, Vec> {
            StringOption<String>* base = nullptr;
            std::int64_t value;
        };
    }  // namespace cmdline
}  // namespace utils
