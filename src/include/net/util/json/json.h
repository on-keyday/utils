/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// json - json object
#pragma once

#include "internal.h"

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
                    : obj(std::move(s)) {}
                JSONBase(String&& s)
                    : obj(std::move(s)) {}
                JSONBase(const object_t& o)
                    : obj(std::move(o)) {}
                JSONBase(object_t&& o)
                    : obj(std::move(o)) {}
                JSONBase(const array_t& a)
                    : obj(std::move(a)) {}
                JSONBase(array_t&& a)
                    : obj(std::move(a)) {}
            };
        }  // namespace json
    }      // namespace net
}  // namespace utils
