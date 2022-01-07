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
            };

        }  // namespace json

    }  // namespace net
}  // namespace utils
