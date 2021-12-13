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

    }  // namespace cmdline
}  // namespace utils
