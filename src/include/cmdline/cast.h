/*license*/

// cast - option cast helper
#pragma once

#include "option.h"
#include "somevalue_option.h"
#include "../wrap/lite/smart_ptr.h"

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

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, BoolSomeValueOption<String, Vec>> {
                static BoolSomeValueOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() != OptionType::somevalue) {
                        return nullptr;
                    }
                    auto sv = static_cast<SomeValueOption<String, Vec>*>(std::addressof(*in));
                    if (sv->has_type != OptionType::boolean) {
                        return nullptr;
                    }
                    return static_cast<BoolSomeValueOption<String, Vec>*>(sv);
                }
            };

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, IntSomeValueOption<String, Vec>> {
                static IntSomeValueOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() != OptionType::somevalue) {
                        return nullptr;
                    }
                    auto sv = static_cast<SomeValueOption<String, Vec>*>(std::addressof(*in));
                    if (sv->has_type != OptionType::integer) {
                        return nullptr;
                    }
                    return static_cast<IntSomeValueOption<String, Vec>*>(sv);
                }
            };

            template <class String, template <class...> class Vec>
            struct CastHelper<String, Vec, StringSomeValueOption<String, Vec>> {
                static StringSomeValueOption<String, Vec>* cast(auto& in) {
                    if (!in) {
                        return nullptr;
                    }
                    if (in->type() != OptionType::somevalue) {
                        return nullptr;
                    }
                    auto sv = static_cast<SomeValueOption<String, Vec>*>(std::addressof(*in));
                    if (sv->has_type != OptionType::string) {
                        return nullptr;
                    }
                    return static_cast<StringSomeValueOption<String, Vec>*>(sv);
                }
            };

        }  // namespace internal

        template <template <class, template <class...> class> class T, class String, template <class...> class Vec>
        T<String, Vec>* cast(wrap::shared_ptr<Option<String, Vec>>& v) {
            return internal::CastHelper<String, Vec, T<String, Vec>>::cast(v);
        }

        template <template <class, template <class...> class> class T, class String, template <class...> class Vec>
        T<String, Vec>* cast(Option<String, Vec>* v) {
            return internal::CastHelper<String, Vec, T<String, Vec>>::cast(v);
        }

    }  // namespace cmdline
}  // namespace utils
