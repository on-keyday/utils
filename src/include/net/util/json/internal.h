/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// internal - json internal definition
#pragma once
#include <cstdint>

namespace utils {
    namespace net {
        namespace json {
            enum class JSONKind {
                undefined,
                null,
                boolean,
                number_i,
                number_f,
                number_u,
                string,
                object,
                array,
            };

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct JSONBase;

            namespace internal {
                template <class String, template <class...> class Vec, template <class...> class Object>
                struct JSONHolder {
                    using self_t = JSONBase<String, Vec, Object>;

                   private:
                    union {
                        std::int64_t i;
                        std::uint64_t u;
                        double f;
                        String* s;
                        Object<String, self_t>* o;
                        Vec<self_t>* a;
                        void* p = nullptr;
                    };
                    JSONKind kind = JSONKind::undefined;

                   public:
                    constexpr JSONHolder() {}
                    constexpr JSONHolder(std::nullptr_t)
                        : kind(JSONKind::null), p(nullptr) {}
                    constexpr JSONHolder(std::int64_t n)
                        : kind(JSONKind::number_i), i(n) {}
                    constexpr JSONHolder(std::uint64_t n)
                        : kind(JSONKind::number_u), u(n) {}
                    constexpr JSONHolder(double n)
                        : kind(JSONKind::number_f), f(n) {}
                    JSONHolder(const String& n)
                        : kind(JSONKind::string), s(new String(n)) {}
                };
            }  // namespace internal

        }  // namespace json

    }  // namespace net
}  // namespace utils