/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// internal - json internal definition
#pragma once
#include <cstdint>
#include <cassert>
#include <cstddef>
#include <utility>
#include <limits>
#include <memory>
#include <core/byte.h>

namespace futils {

    namespace json {
        enum class JSONKind : unsigned char {
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

        constexpr const char* to_string(JSONKind k) {
            switch (k) {
                case JSONKind::undefined:
                    return "undefined";
                case JSONKind::null:
                    return "null";
                case JSONKind::boolean:
                    return "boolean";
                case JSONKind::number_i:
                    return "number_i";
                case JSONKind::number_f:
                    return "number_f";
                case JSONKind::number_u:
                    return "number_u";
                case JSONKind::string:
                    return "string";
                case JSONKind::object:
                    return "object";
                case JSONKind::array:
                    return "array";
                default:
                    return "unknown";
            }
        }

        template <class String, template <class...> class Vec, template <class...> class Object>
        struct JSONBase;

        namespace internal {

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct JSONHolder {
                using self_t = JSONBase<String, Vec, Object>;
                using object_t = Object<String, self_t>;
                using array_t = Vec<self_t>;
                using string_t = String;

               private:
                static constexpr size_t storage_size =
                    [] {
                        size_t size = sizeof(bool);
                        if (size < sizeof(std::int64_t)) {
                            size = sizeof(std::int64_t);
                        }
                        if (size < sizeof(std::uint64_t)) {
                            size = sizeof(std::uint64_t);
                        }
                        if (size < sizeof(double)) {
                            size = sizeof(double);
                        }
                        if (size < sizeof(String)) {
                            size = sizeof(String);
                        }
                        if (size < sizeof(object_t)) {
                            size = sizeof(object_t);
                        }
                        if (size < sizeof(array_t)) {
                            size = sizeof(array_t);
                        }
                        return size;
                    };

                union {
                    byte storage[storage_size];
                    bool b;
                    std::int64_t i;
                    std::uint64_t u;
                    double f;
                    String s;
                    object_t o;
                    array_t a;
                };
                JSONKind kind_ = JSONKind::undefined;

                constexpr void move(JSONHolder&& n) {
                    if (kind_ == JSONKind::array) {
                        std::construct_at(std::addressof(a), std::move(n.a));
                    }
                    else if (kind_ == JSONKind::object) {
                        std::construct_at(std::addressof(o), std::move(n.o));
                    }
                    else if (kind_ == JSONKind::string) {
                        std::construct_at(std::addressof(s), std::move(n.s));
                    }
                    else if (kind_ == JSONKind::number_f) {
                        f = n.f;
                    }
                    else if (kind_ == JSONKind::number_i) {
                        i = n.i;
                    }
                    else if (kind_ == JSONKind::number_u) {
                        u = n.u;
                    }
                    else if (kind_ == JSONKind::boolean) {
                        b = n.b;
                    }
                }

                constexpr void copy(const JSONHolder& n) {
                    if (kind_ == JSONKind::array) {
                        std::construct_at(std::addressof(a), n.a);
                    }
                    else if (kind_ == JSONKind::object) {
                        std::construct_at(std::addressof(o), n.o);
                    }
                    else if (kind_ == JSONKind::string) {
                        std::construct_at(std::addressof(s), n.s);
                    }
                    else if (kind_ == JSONKind::number_f) {
                        f = n.f;
                    }
                    else if (kind_ == JSONKind::number_i) {
                        i = n.i;
                    }
                    else if (kind_ == JSONKind::number_u) {
                        u = n.u;
                    }
                    else if (kind_ == JSONKind::boolean) {
                        b = n.b;
                    }
                }

               public:
                constexpr JSONHolder() {}
                constexpr JSONHolder(std::nullptr_t)
                    : kind_(JSONKind::null) {}
                constexpr JSONHolder(bool n)
                    : kind_(JSONKind::boolean), b(n) {}
                constexpr JSONHolder(int n)
                    : kind_(JSONKind::number_i), i(n) {}
                constexpr JSONHolder(std::int64_t n)
                    : kind_(JSONKind::number_i), i(n) {}
                constexpr JSONHolder(std::uint64_t n)
                    : kind_(JSONKind::number_u), u(n) {}
                constexpr JSONHolder(double n)
                    : kind_(JSONKind::number_f), f(n) {}
                constexpr JSONHolder(const String& n)
                    : kind_(JSONKind::string), s(n) {}
                constexpr JSONHolder(String&& n)
                    : kind_(JSONKind::string), s(std::move(n)) {}
                constexpr JSONHolder(const object_t& n)
                    : kind_(JSONKind::object), o(n) {}
                constexpr JSONHolder(object_t&& n)
                    : kind_(JSONKind::object), o(std::move(n)) {}
                constexpr JSONHolder(const array_t& n)
                    : kind_(JSONKind::array), a(n) {}
                constexpr JSONHolder(array_t&& n)
                    : kind_(JSONKind::array), a(std::move(n)) {}

                constexpr object_t& init_as_object() {
                    this->destroy();
                    std::construct_at(std::addressof(o));
                    kind_ = JSONKind::object;
                    return o;
                }

                constexpr array_t& init_as_array() {
                    this->destroy();
                    std::construct_at(std::addressof(a));
                    kind_ = JSONKind::array;
                    return a;
                }

                constexpr string_t& init_as_string() {
                    this->destroy();
                    std::construct_at(std::addressof(s));
                    kind_ = JSONKind::string;
                    return s;
                }

                constexpr JSONHolder(JSONHolder&& n)
                    : kind_(n.kind_) {
                    move(std::move(n));
                    n.destroy();
                }

                constexpr JSONKind kind() const {
                    return kind_;
                }

                constexpr JSONHolder& operator=(JSONHolder&& n) {
                    if (this == &n) {
                        return *this;
                    }
                    this->destroy();
                    kind_ = n.kind_;
                    move(std::move(n));
                    n.destroy();
                    return *this;
                }

                constexpr JSONHolder(const JSONHolder& n)
                    : kind_(n.kind_) {
                    copy(n);
                }

                constexpr JSONHolder& operator=(const JSONHolder& n) {
                    if (this == &n) {
                        return *this;
                    }
                    this->destroy();
                    kind_ = n.kind_;
                    copy(n);
                    return *this;
                }

               private:
                constexpr void destroy() {
                    if (kind_ == JSONKind::array) {
                        std::destroy_at(std::addressof(a));
                    }
                    else if (kind_ == JSONKind::object) {
                        std::destroy_at(std::addressof(o));
                    }
                    else if (kind_ == JSONKind::string) {
                        std::destroy_at(std::addressof(s));
                    }
                    kind_ = JSONKind::undefined;
                }

               public:
                constexpr ~JSONHolder() {
                    destroy();
                }

                constexpr bool is_undef() const {
                    return kind_ == JSONKind::undefined;
                }

                constexpr bool is_null() const {
                    return kind_ == JSONKind::null;
                }

                const std::int64_t* as_numi() const {
                    if (kind_ == JSONKind::number_i) {
                        return &i;
                    }
                    return nullptr;
                }

                const std::uint64_t* as_numu() const {
                    if (kind_ == JSONKind::number_u) {
                        return &u;
                    }
                    return nullptr;
                }

                const double* as_numf() const {
                    if (kind_ == JSONKind::number_f) {
                        return &f;
                    }
                    return nullptr;
                }

                constexpr const bool* as_bool() const {
                    if (kind_ == JSONKind::boolean) {
                        return &b;
                    }
                    return nullptr;
                }

                constexpr const String* as_str() const {
                    if (kind_ == JSONKind::string) {
                        return std::addressof(s);
                    }
                    return nullptr;
                }

                constexpr const object_t* as_obj() const {
                    if (kind_ == JSONKind::object) {
                        return std::addressof(o);
                    }
                    return nullptr;
                }

                constexpr const array_t* as_arr() const {
                    if (kind_ == JSONKind::array) {
                        return std::addressof(a);
                    }
                    return nullptr;
                }
            };

            template <class String, template <class...> class Vec, template <class...> class Object>
            bool operator==(const JSONHolder<String, Vec, Object>& a, const JSONHolder<String, Vec, Object>& b) {
                if (a.kind() != b.kind()) {
                    return false;
                }
                if (a.kind() == JSONKind::object) {
                    return *a.as_obj() == *b.as_obj();
                }
                else if (a.kind() == JSONKind::array) {
                    return *a.as_arr() == *b.as_arr();
                }
                else if (a.kind() == JSONKind::string) {
                    return *a.as_str() == *b.as_str();
                }
                else if (a.kind() == JSONKind::number_f) {
                    return *a.as_numf() - *b.as_numf() < std::numeric_limits<double>::epsilon();
                }
                else if (a.kind() == JSONKind::number_i) {
                    return *a.as_numi() == *b.as_numi();
                }
                else if (a.kind() == JSONKind::number_u) {
                    return *a.as_numu() == *b.as_numu();
                }
                else if (a.kind() == JSONKind::boolean) {
                    return *a.as_bool() == *b.as_bool();
                }
                return true;
            }

        }  // namespace internal

    }  // namespace json
}  // namespace futils
