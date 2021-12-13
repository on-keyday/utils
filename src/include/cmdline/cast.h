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
                    return nullptr;
                }
            };

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, IntOption<String, Vec>> {
                static IntOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() == OptionType::integer) {
                        return static_cast<IntOption<String, Vec>*>(std::addressof(*in));
                    }
                    return nullptr;
                }
            };

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, StringOption<String, Vec>> {
                static StringOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() == OptionType::string) {
                        return static_cast<StringOption<String, Vec>*>(std::addressof(*in));
                    }
                    return nullptr;
                }
            };

        }  // namespace internal
    }      // namespace cmdline
}  // namespace utils
