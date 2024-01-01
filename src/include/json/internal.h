/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
                union {
                    bool b;
                    std::int64_t i;
                    std::uint64_t u;
                    double f;
                    String* s;
                    object_t* o;
                    array_t* a;
                    void* p = nullptr;
                };
                JSONKind kind_ = JSONKind::undefined;

                void copy(const JSONHolder& n) {
                    if (kind_ == JSONKind::array) {
                        a = new array_t(*n.a);
                    }
                    else if (kind_ == JSONKind::object) {
                        o = new object_t(*n.o);
                    }
                    else if (kind_ == JSONKind::string) {
                        s = new String(*n.s);
                    }
                    else {
                        p = n.p;
                    }
                }

               public:
                constexpr JSONHolder() {}
                constexpr JSONHolder(std::nullptr_t)
                    : kind_(JSONKind::null), p(nullptr) {}
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
                JSONHolder(const String& n)
                    : kind_(JSONKind::string), s(new String(n)) {}
                JSONHolder(String&& n)
                    : kind_(JSONKind::string), s(new String(std::move(n))) {}
                JSONHolder(const object_t& n)
                    : kind_(JSONKind::object), o(new object_t(n)) {}
                JSONHolder(object_t&& n)
                    : kind_(JSONKind::object), o(new object_t(std::move(n))) {}
                JSONHolder(const array_t& n)
                    : kind_(JSONKind::array), a(new array_t(n)) {}
                JSONHolder(array_t&& n)
                    : kind_(JSONKind::array), a(new array_t(std::move(n))) {}

                constexpr JSONHolder(String* n)
                    : kind_(JSONKind::string), s(n) {
                    assert(n);
                }

                constexpr JSONHolder(object_t* n)
                    : kind_(JSONKind::object), o(n) {
                    assert(n);
                }

                constexpr JSONHolder(array_t* n)
                    : kind_(JSONKind::array), a(n) {
                    assert(n);
                }

                constexpr JSONHolder(JSONHolder&& n)
                    : kind_(n.kind_), p(n.p) {
                    n.p = nullptr;
                    n.kind_ = JSONKind::undefined;
                }

                constexpr JSONKind kind() const {
                    return kind_;
                }

                JSONHolder& operator=(JSONHolder&& n) {
                    if (this == &n) {
                        return *this;
                    }
                    this->~JSONHolder();
                    kind_ = n.kind_;
                    p = n.p;
                    n.p = nullptr;
                    n.kind_ = JSONKind::undefined;
                    return *this;
                }

                JSONHolder(const JSONHolder& n)
                    : kind_(n.kind_) {
                    copy(n);
                }

                JSONHolder& operator=(const JSONHolder& n) {
                    if (this == &n) {
                        return *this;
                    }
                    this->~JSONHolder();
                    kind_ = n.kind_;
                    copy(n);
                    return *this;
                }

                constexpr ~JSONHolder() {
                    if (kind_ == JSONKind::array) {
                        delete a;
                    }
                    else if (kind_ == JSONKind::object) {
                        delete o;
                    }
                    else if (kind_ == JSONKind::string) {
                        delete s;
                    }
                    p = nullptr;
                    kind_ = JSONKind::undefined;
                }

                bool is_undef() const {
                    return kind_ == JSONKind::undefined;
                }

                bool is_null() const {
                    return kind_ == JSONKind::null;
                }

                const std::int64_t* as_numi() const {
                    return kind_ == JSONKind::number_i ? &i : nullptr;
                }

                const std::uint64_t* as_numu() const {
                    return kind_ == JSONKind::number_u ? &u : nullptr;
                }

                const double* as_numf() const {
                    return kind_ == JSONKind::number_f ? &f : nullptr;
                }

                const bool* as_bool() const {
                    return kind_ == JSONKind::boolean ? &b : nullptr;
                }

                const String* as_str() const {
                    return kind_ == JSONKind::string ? s : nullptr;
                }

                const object_t* as_obj() const {
                    return kind_ == JSONKind::object ? o : nullptr;
                }

                const array_t* as_arr() const {
                    return kind_ == JSONKind::array ? a : nullptr;
                }

                const void* as_rawp() const {
                    return p;
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
                else {
                    return a.as_rawp() == b.as_rawp();
                }
            }

        }  // namespace internal

    }  // namespace json
}  // namespace futils
