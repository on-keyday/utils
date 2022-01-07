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
                    using object_t = Object<String, self_t>;
                    using array_t = Vec<self_t>;

                   private:
                    union {
                        std::int64_t i;
                        std::uint64_t u;
                        double f;
                        String* s;
                        object_t* o;
                        array_t* a;
                        void* p = nullptr;
                    };
                    JSONKind kind = JSONKind::undefined;

                    void copy(const JSONHolder& n) {
                        if (kind == JSONKind::array) {
                            a = new array_t(*n.a);
                        }
                        else if (kind == JSONKind::object) {
                            o = new object_t(*n.o);
                        }
                        else if (kind == JSONKind::string) {
                            s = new String(*n.s);
                        }
                        else {
                            p = n.p;
                        }
                    }

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
                    JSONHolder(String&& n)
                        : kind(JSONKind::string), s(new String(std::move(n))) {}
                    JSONHolder(const object_t& n)
                        : kind(JSONKind::object), o(new object_t(n)) {}
                    JSONHolder(object_t&& n)
                        : kind(JSONKind::object), o(new object_t(std::move(n))) {}
                    JSONHolder(const array_t& n)
                        : kind(JSONKind::array), a(new array_t(n)) {}
                    JSONHolder(array_t&& n)
                        : kind(JSONKind::array), a(new array_t(std::move(n))) {}

                    constexpr JSONHolder(JSONHolder&& n)
                        : kind(n.kind), p(n.p) {
                        n.p = nullptr;
                        n.kind = JSONKind::undefined;
                    }

                    JSONHolder& operator=(JSONHolder&& n) {
                        this->~JSONHolder();
                        kind = n.kind;
                        p = n.p;
                        n.p = nullptr;
                        n.kind = JSONKind::undefined;
                        return *this;
                    }

                    JSONHolder(const JSONHolder& n)
                        : kind(n.kind) {
                        copy(n);
                    }

                    JSONHolder& operator=(const JSONHolder& n) {
                        this->~JSONHolder();
                        kind = n.kind;
                        copy(n);
                        return *this;
                    }

                    ~JSONHolder() {
                        if (kind == JSONKind::array) {
                            delete a;
                        }
                        else if (kind == JSONKind::object) {
                            delete o;
                        }
                        else if (kind == JSONKind::string) {
                            delete s;
                        }
                        p = nullptr;
                        kind = JSONKind::undefined;
                    }
                };
            }  // namespace internal

        }  // namespace json

    }  // namespace net
}  // namespace utils
