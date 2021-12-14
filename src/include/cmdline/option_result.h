/*license*/

// option_result - option parsed result
#pragma once

#include "option.h"

#include "../wrap/lite/smart_ptr.h"

namespace utils {
    namespace cmdline {

        template <class String, template <class...> class Vec>
        struct OptionResult {
            Option<String, Vec>* base = nullptr;
            wrap::shared_ptr<Option<String, Vec>> value;
        };

        template <class String, template <class...> class Vec, template <class...> class MultiMap>
        struct OptionResultSet {
            MultiMap<String, OptionResult<String, Vec>> result;

            bool exists(const String& name) {
                return result.find(name) != result.end();
            }
        };

    }  // namespace cmdline
}  // namespace utils
