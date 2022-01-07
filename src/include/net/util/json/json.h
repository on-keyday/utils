/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - json object
#pragma once

#include "internal.h"
#include <stdexcept>
#include "../../../utf/convert.h"

namespace utils {
    namespace net {
        namespace json {
            template <class String, template <class...> class Vec, template <class...> class Object>
            struct JSONBase {
               private:
                using holder_t = internal::JSONHolder<String, Vec, Object>;
                holder_t obj;
                using object_t = typename holder_t::object_t;
                using array_t = typename holder_t::array_t;
                using self_t = JSONBase<String, Vec, Object>;

                [[noreturn]] static void bad_type(const char* err) {
                    throw std::invalid_argument(err);
                }

                static const self_t& as_const(self_t& in) {
                    return in;
                }

               public:
                JSONBase(std::nullptr_t)
                    : obj(nullptr) {}
                JSONBase(bool b)
                    : obj(b) {}
                JSONBase(int i)
                    : obj(i) {}
                JSONBase(std::int64_t i)
                    : obj(i) {}
                JSONBase(std::uint64_t u)
                    : obj(u) {}
                JSONBase(double f)
                    : obj(f) {}
                JSONBase(const String& s)
                    : obj(s) {}
                JSONBase(String&& s)
                    : obj(std::move(s)) {}
                JSONBase(const object_t& o)
                    : obj(o) {}
                JSONBase(object_t&& o)
                    : obj(std::move(o)) {}
                JSONBase(const array_t& a)
                    : obj(a) {}
                JSONBase(array_t&& a)
                    : obj(std::move(a)) {}

                size_t size() const {
                    if (obj.is_null() || obj.is_undef()) {
                        return 0;
                    }
                    else if (auto o = obj.as_obj()) {
                        return o->size();
                    }
                    else if (auto a = obj.as_arr()) {
                        return a->size();
                    }
                    return 1;
                }

                const self_t* at(size_t n, const char** err = nullptr) const {
                    auto a = obj.as_arr();
                    if (!a) {
                        if (err) {
                            *err = "not array type";
                        }
                        return nullptr;
                    }
                    if (a->size() <= n) {
                        if (err) {
                            *err = "index out of range";
                        }
                        return nullptr;
                    }
                    return &(*a)[n];
                }

                const self_t* at(const String& n, const char** err = nullptr) const {
                    auto a = obj.as_obj();
                    if (!a) {
                        if (err) {
                            *err = "not object type";
                        }
                        return nullptr;
                    }
                    auto found = a->find(n);
                    if (found == a->end()) {
                        if (err) {
                            *err = "key not found";
                        }
                        return nullptr;
                    }
                    return &std::get<1>(*found);
                }

                self_t* at(size_t n, const char** err = nullptr) {
                    return const_cast<self_t*>(as_const(*this).at(n, err));
                }

                self_t* at(const String& n, const char** err = nullptr) {
                    return const_cast<self_t*>(as_const(*this).at(n, err));
                }

                const self_t& operator[](size_t n) const {
                    const char* err = nullptr;
                    auto res = at(n, &err);
                    if (!res) {
                        bad_type(err);
                    }
                    return *res;
                }

                const self_t& operator[](const String& n) const {
                    const char* err = nullptr;
                    auto res = at(n, &err);
                    if (!res) {
                        bad_type(err);
                    }
                    return *res;
                }

                self_t& operator[](const String& n) {
                    if (obj.is_undef()) {
                        obj = new object_t{};
                    }
                    const char* err = nullptr;
                    auto res = at(n, &err);
                    if (res) {
                        return *res;
                    }
                    auto obj = obj.as_obj();
                    if (!obj) {
                        bad_type(err);
                    }
                    auto it = obj->emplace(n, {});
                    if (!std::get<1>(it)) {
                        bad_type("failed to insert");
                    }
                    return std::get<1>(*std::get<0>(it));
                }

                self_t& operator[](size_t n) {
                    return const_cast<self_t&>(as_const(*this)[n]);
                }

                bool is_undef() const {
                    return obj.is_undef();
                }

                bool is_null() const {
                    return obj.is_null();
                }

                bool is_bool() const {
                    return obj.as_bool();
                }

                bool is_number() const {
                    return obj.as_numi() || obj.as_numu() || obj.as_numf();
                }

                bool is_float() const {
                    return obj.as_numf();
                }

                bool is_string() const {
                    return obj.as_str();
                }

                bool is_object() const {
                    return obj.as_obj();
                }

                bool is_array() const {
                    return obj.as_arr();
                }

                bool as_bool(bool& r) const {
                    auto b = obj.as_bool();
                    if (!b) {
                        return false;
                    }
                    r = *b;
                    return true;
                }

                template <class T>
                bool as_number(T& to) {
                    auto i = obj.as_numi();
                    if (i) {
                        to = T(*i);
                        return true;
                    }
                    auto u = obj.as_numu();
                    if (u) {
                        to = T(*u);
                        return true;
                    }
                    auto f = obj.as_numf();
                    if (f) {
                        to = T(*u);
                        return true;
                    }
                    return false;
                }

                template <class String>
                bool as_string(String& str) {
                    auto s = obj.as_str();
                    if (!s) {
                        return false;
                    }
                    utf::convert(*s, str);
                    return true;
                }

                template <std::integral T>
                explicit operator T() const {
                    T t;
                    if (!to_number(t)) {
                        bad_type("not number type");
                    }
                    return t;
                }

                explicit operator bool() const {
                    auto b = obj.as_bool();
                    if (!b) {
                        bad_type("not boolean type");
                    }
                    return *b;
                }

                template <std::floating_point T>
                explicit operator T() const {
                    T t;
                    if (!to_number(t)) {
                        bad_type("not number type");
                    }
                    return t;
                }
            };
        }  // namespace json

    }  // namespace net
}  // namespace utils
