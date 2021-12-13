/*license*/

// cast - option cast helper
#pragma once

#include "option.h"
#include "somevalue_option.h"

namespace utils {
    namespace cmdline {
        namespace internal {
            template <class String, template <class...> class Vec, class T>
            struct CastHelper {
                static std::nullptr_t cast(auto& in) {
                    return nullptr;
                }
            };

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, BoolOption<String, Vec>> {
                static BoolOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() == OptionType::boolean) {
                        return static_cast<BoolOption<String, Vec>*>(std::addressof(*in));
                    }
                }
            };

        }  // namespace internal
    }      // namespace cmdline
}  // namespace utils