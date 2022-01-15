/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// jsonbase - json object base template
#pragma once

#include "internal.h"
#include "error.h"
#include <stdexcept>
#include "../utf/convert.h"
#include "../helper/append_charsize.h"
#include "../number/parse.h"
#include "../number/to_string.h"

namespace utils {
    namespace helper {
        template <class T, class Indent>
        struct IndentWriter;
    }

    namespace json {
        template <class T>
        concept StringLike = std::is_default_constructible_v<T> && helper::is_utf_convertable<T>;
        namespace internal {
            template <class Out, class Str, template <class...> class V, template <class...> class O>
            JSONErr to_string_detail(const JSONBase<Str, V, O>& json, helper::IndentWriter<Out, const char*>& out, FmtFlag flag);
        }

        template <class String, template <class...> class Vec, template <class...> class Object>
        struct JSONBase {
           private:
            template <class T, class Str, template <class...> class V, template <class...> class O>
            friend JSONErr parse(Sequencer<T>& seq, JSONBase<Str, V, O>& json, bool eof);
            template <class Out, class Str, template <class...> class V, template <class...> class O>
            friend JSONErr internal::to_string_detail(const JSONBase<Str, V, O>& json, helper::IndentWriter<Out, const char*>& out, FmtFlag flag);

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

            const holder_t& get_holder() const {
                return obj;
            }

            holder_t& get_holder() {
                return obj;
            }

           public:
            constexpr JSONBase() {}
            constexpr JSONBase(std::nullptr_t)
                : obj(nullptr) {}
            constexpr JSONBase(bool b)
                : obj(b) {}
            constexpr JSONBase(int i)
                : obj(i) {}
            constexpr JSONBase(std::int64_t i)
                : obj(i) {}
            constexpr JSONBase(std::uint64_t u)
                : obj(u) {}
            constexpr JSONBase(double f)
                : obj(f) {}
            JSONBase(const String& s)
                : obj(s) {}
            JSONBase(String&& s)
                : obj(std::move(s)) {}
            template <class C = char>
            JSONBase(C* p)
                : obj(String(p)) {}
            JSONBase(const object_t& o)
                : obj(o) {}
            JSONBase(object_t&& o)
                : obj(std::move(o)) {}
            JSONBase(const array_t& a)
                : obj(a) {}
            JSONBase(array_t&& a)
                : obj(std::move(a)) {}
            JSONBase(const JSONBase& o)
                : obj(o.obj) {}
            constexpr JSONBase(JSONBase&& o)
                : obj(std::move(o.obj)) {}

            JSONBase& operator=(const JSONBase& in) {
                obj = in.obj;
                return *this;
            }

            JSONBase& operator=(JSONBase&& in) {
                obj = std::move(in.obj);
                return *this;
            }

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
                auto objp = const_cast<object_t*>(obj.as_obj());
                if (!objp) {
                    bad_type(err);
                }
                auto it = objp->emplace(n, self_t{});
                if (!std::get<1>(it)) {
                    bad_type("failed to insert");
                }
                return const_cast<self_t&>(std::get<1>(*std::get<0>(it)));
            }

            self_t& operator[](size_t n) {
                return const_cast<self_t&>(as_const(*this)[n]);
            }

            bool erase(const String& n) {
                auto ptr = const_cast<object_t*>(obj.as_obj());
                if (!ptr) {
                    return false;
                }
                return ptr->erase(n);
            }

            bool erase(size_t i) {
                auto ptr = const_cast<array_t*>(obj.as_arr());
                if (!ptr) {
                    return false;
                }
                if (ptr->size() <= i) {
                    return false;
                }
                ptr->erase(ptr->begin() + i);
                return true;
            }

            bool pop_back() {
                auto a = const_cast<array_t*>(obj.as_arr());
                if (!a) {
                    return false;
                }
                if (a->size() == 0) {
                    return false;
                }
                a->pop_back();
                return true;
            }

            template <class T>
            bool push_back(T&& n) {
                if (obj.is_undef()) {
                    obj = new array_t{};
                }
                auto a = const_cast<array_t*>(obj.as_arr());
                if (!a) {
                    return false;
                }
                a->push_back(std::forward<T>(n));
                return true;
            }

            void init_obj() {
                obj = new object_t{};
            }

            void init_array() {
                obj = new array_t{};
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

            bool force_is_null() const {
                bool b = false;
                if (force_as_bool(b)) {
                    return b == false;
                }
                return true;
            }

            bool force_as_bool(bool& r) const {
                if (as_bool(r)) {
                    return true;
                }
                if (obj.is_null() || obj.is_undef()) {
                    r = false;
                    return true;
                }
                else if (auto s = obj.as_str()) {
                    r = s->size() != 0;
                    return true;
                }
                else if (auto i = obj.as_numi()) {
                    r = *i != 0;
                }
                else if (auto u = obj.as_numu()) {
                    r = *i != 0;
                }
                else if (auto f = obj.as_numf()) {
                    r = *f != 0 && *f == *f;
                }
                else if (auto o = obj.as_obj()) {
                    r = o->size() != 0;
                }
                else if (auto a = obj.as_arr()) {
                    r = a->size() != 0;
                }
                else {
                    return false;
                }
                return true;
            }

            template <class T>
            bool as_number(T& to) const {
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
                    to = T(*f);
                    return true;
                }
                return false;
            }

            template <class T>
            bool force_as_number(T& t) const {
                if (as_number(t)) {
                    return true;
                }
                if (obj.is_null() || obj.is_undef()) {
                    t = 0;
                    return true;
                }
                else if (auto b = obj.as_bool()) {
                    t = *b ? 1 : 0;
                    return true;
                }
                else if (auto s = obj.as_str()) {
                    std::int64_t i;
                    if (number::parse_integer(*s, t)) {
                        t = T(i);
                        return true;
                    }
                    std::uint64_t u;
                    if (number::parse_integer(*s, u)) {
                        t = T(u);
                        return true;
                    }
                    double d;
                    if (number::parse_float(*s, d)) {
                        t = T(d);
                        return true;
                    }
                    return false;
                }
                else {
                    return false;
                }
            }

            template <class Outstr>
            bool as_string(Outstr& str) const {
                auto s = obj.as_str();
                if (!s) {
                    return false;
                }
                utf::convert(*s, str);
                return true;
            }

            template <class OutStr>
            bool force_as_string(OutStr& str) const {
                if (as_string(str)) {
                    return true;
                }
                if (obj.is_null() || obj.is_undef()) {
                    utf::convert("null", str);
                }
                else if (auto b = obj.as_bool()) {
                    utf::convert(*b ? "true" : "false", str);
                }
                else if (auto i = obj.as_numi()) {
                    number::to_string(str, *i);
                }
                else if (auto u = obj.as_numu()) {
                    number::to_string(str, *u);
                }
                else if (auto f = obj.as_numf()) {
                    number::to_string(str, *f);
                }
                else {
                    return false;
                }
                return true;
            }

            template <class T>
            requires std::is_integral_v<T>
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

            template <class T>
            requires std::is_floating_point_v<T>
            explicit operator T() const {
                T t;
                if (!to_number(t)) {
                    bad_type("not number type");
                }
                return t;
            }

            template <StringLike T>
            explicit operator T() const {
                T t;
                if (!as_string(t)) {
                    bad_type("not string type");
                }
                return t;
            }

            auto obegin() const {
                auto o = obj.as_obj();
                if (!o) {
                    bad_type("not object type");
                }
                return o->begin();
            }

            auto oend() const {
                auto o = obj.as_obj();
                if (!o) {
                    bad_type("not object type");
                }
                return o->end();
            }

            auto abegin() const {
                auto a = obj.as_arr();
                if (!a) {
                    bad_type("not array type");
                }
                return a->begin();
            }

            auto aend() const {
                auto a = obj.as_arr();
                if (!a) {
                    bad_type("not array type");
                }
                return a->end();
            }

            bool operator==(const JSONBase<String, Vec, Object>& v) const {
                return obj == v.obj;
            }
        };

    }  // namespace json

}  // namespace utils
