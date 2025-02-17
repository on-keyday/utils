/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_json - to_json and from_json helper
#pragma once

#include "jsonbase.h"
#include "iterator.h"
#include <helper/template_instance.h>

namespace futils {
    namespace json {
        enum class FromFlag {
            none = 0x0,
            force_element = 0x1,
            allow_null_obj_array = 0x2,
            not_clear_string = 0x4,
            not_init_null_obj = 0x8,
        };

        DEFINE_ENUM_FLAGOP(FromFlag)

        namespace internal {

            template <class T>
            concept to_array_interface = requires(T t) {
                { t.size() };
                { t[size_t(0)] };
            };

            template <class T>
            concept to_map_interface = requires(T t) {
                { get<1>(*t.begin()) };
                { t.end() };
            };

            template <class T, class JSON>
            concept to_json_adl = requires(T t, JSON j) {
                { to_json(t, j) };
            };

            template <class T, class JSON>
            concept to_json_method = requires(T t, JSON j) {
                { t.to_json(j) };
            };

            template <class T, class JSON>
            concept from_json_adl = requires(T t, JSON j) {
                { from_json(t, j) };
            };

            template <class T, class JSON>
            concept from_json_method = requires(T t, JSON j) {
                { t.from_json(j) };
            };

            template <class T, class JSON>
            concept from_json_adl_flag = requires(T t, JSON j) {
                { from_json(t, j, FromFlag::none) };
            };

            template <class T, class JSON>
            concept from_json_method_flag = requires(T t, JSON j) {
                { t.from_json(j, FromFlag::none) };
            };

            template <class T>
            concept derefable = requires(T t) {
                { *t };
                { !t };
            };

            template <class T>
            concept indexable = requires(T t) {
                { t[1] };
            };

            template <class T>
            concept from_array_t = indexable<T> && requires(T t) {
                { t.size() };
            };

            template <class T>
            concept from_map_t = requires(T t) {
                { t["key"] };
            };

            template <class T>
            concept pushbackable = requires(T t) {
                { t.push_back('C') };
            };

            template <class T>
            concept to_int_from_elm = requires(T t) {
                { int(t[1]) };
            };

            template <class T>
            concept has_clear = requires(T t) {
                { t.clear() };
            };

            template <class JSON>
            struct is_json_t {
                using base_t = void;
                using string_t = void;
                using vec_t = void;
                using object_t = void;
            };

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct is_json_t<JSONBase<String, Vec, Object>> {
                using base_t = JSONBase<String, Vec, Object>;
                using string_t = String;
                using vec_t = Vec<base_t>;
                using object_t = Object<String, base_t>;
            };

            template <class T>
            concept primitive =
                std::is_arithmetic_v<T> ||
                std::is_same_v<T, const char*>;

            template <class T, class JSON>
            bool dispatch_from_json(T&& t, const JSON& js, FromFlag flag = FromFlag::none) {
                using T_ = std::remove_reference_t<T>;
                using T_cv = std::remove_cvref_t<T>;
                using json_t = is_json_t<JSON>;
                if constexpr (from_json_method_flag<T, JSON>) {
                    return t.from_json(js, flag);
                }
                else if constexpr (from_json_adl_flag<T, JSON>) {
                    return from_json(t, js, flag);
                }
                else if constexpr (from_json_method<T, JSON>) {
                    return t.from_json(js);
                }
                else if constexpr (from_json_adl<T, JSON>) {
                    return from_json(t, js);
                }
                else if constexpr (std::is_same_v<T_cv, typename json_t::base_t>) {
                    t = js;
                    return true;
                }
                else if constexpr (std::is_same_v<T_cv, bool>) {
                    if (any(flag & FromFlag::force_element)) {
                        return js.force_as_bool(t);
                    }
                    return js.as_bool(t);
                }
                else if constexpr (std::is_same_v<T_cv, void*> ||
                                   std::is_same_v<T_cv, std::nullptr_t>) {
                    if (any(flag & FromFlag::force_element)) {
                        return js.force_is_null();
                    }
                    t = nullptr;
                    return js.is_null();
                }
                else if constexpr (derefable<T>) {
                    if constexpr (futils::helper::is_template<T_cv>) {
                        using param_t = typename futils::helper::template_of_t<T_cv>::template param_at<0>;
                        if constexpr (std::is_default_constructible_v<param_t> &&
                                      std::is_convertible_v<param_t, T_cv>) {
                            if (js.is_null()) {
                                t = {};
                                return true;  // null
                            }
                            if (!any(flag & FromFlag::not_init_null_obj) && !t) {
                                t = param_t{};
                            }
                        }
                    }
                    if (!t) {
                        return false;
                    }
                    return dispatch_from_json(*t, js, flag);
                }
                else if constexpr (std::is_arithmetic_v<T_cv>) {
                    if (any(flag & FromFlag::force_element)) {
                        return js.force_as_number(t);
                    }
                    return js.as_number(t);
                }
                else if constexpr (std::is_same_v<T_cv, typename json_t::string_t> ||
                                   (strutil::is_utf_convertable<T> && pushbackable<T>)) {
                    if constexpr (has_clear<T>) {
                        if (!any(flag & FromFlag::not_clear_string)) {
                            t.clear();
                        }
                    }
                    if (any(flag & FromFlag::force_element)) {
                        return js.force_as_string(t);
                    }
                    return js.as_string(t);
                }

                else if constexpr (from_map_t<T>) {
                    if (!js.is_object()) {
                        return any(flag & FromFlag::allow_null_obj_array) &&
                               js.is_null();
                    }
                    for (auto&& o : as_object(js)) {
                        if (!dispatch_from_json(t[get<0>(o)], get<1>(o), flag)) {
                            return false;
                        }
                    }
                    return true;
                }
                else if constexpr (from_array_t<T>) {
                    if (!js.is_array()) {
                        return any(flag & FromFlag::allow_null_obj_array) &&
                               js.is_null();
                    }
                    for (auto&& a : as_array(js)) {
                        using elm_t = strutil::append_size_t<T>;
                        elm_t v;
                        if (!dispatch_from_json(v, a, flag)) {
                            return false;
                        }
                        t.push_back(std::move(v));
                    }
                    return true;
                }
                else {
                    static_assert(from_json_adl<T, JSON> || from_json_method<T, JSON>, "json type dispatch failed at this type");
                }
            };

            template <class T, class JSON>
            bool dispatch_to_json(T&& t, JSON& js) {
                using T_ = std::remove_reference_t<T>;
                using T_cv = std::remove_cvref_t<T>;
                using json_t = is_json_t<JSON>;
                if constexpr (to_json_method<T, JSON>) {
                    return t.to_json(js);
                }
                else if constexpr (to_json_adl<T, JSON>) {
                    return to_json(t, js);
                }
                else if constexpr (std::is_same_v<T_cv, void*>) {
                    if (!t) {
                        js = nullptr;
                        return true;
                    }
                    js = std::uintptr_t(t);
                    return true;
                }
                else if constexpr (std::is_same_v<T_cv, typename json_t::base_t> ||
                                   std::is_same_v<T_cv, typename json_t::string_t> ||
                                   std::is_same_v<T_cv, typename json_t::object_t> ||
                                   std::is_same_v<T_cv, typename json_t::vec_t> ||
                                   std::is_same_v<T_cv, std::nullptr_t> ||
                                   primitive<T_>) {
                    js = JSON(t);
                    return true;
                }
                else if constexpr ((strutil::is_utf_convertable<T_> && to_int_from_elm<T_>)) {
                    js = utf::convert<typename json_t::string_t>(t);
                    return true;
                }
                else if constexpr (derefable<T_>) {
                    if (!t) {
                        js = nullptr;
                        return true;
                    }
                    return dispatch_to_json(*t, js);
                }
                else if constexpr (to_map_interface<T>) {
                    js.init_obj();
                    for (auto&& v : t) {
                        if (!dispatch_to_json(get<1>(v), js[get<0>(v)])) {
                            return false;
                        }
                    }
                    return true;
                }
                else if constexpr (to_array_interface<T>) {
                    js.init_array();
                    for (size_t i = 0; i < t.size(); i++) {
                        JSON arg;
                        auto&& target = t[i];
                        if (!dispatch_to_json(target, arg)) {
                            return false;
                        }
                        js.push_back(std::move(arg));
                    }
                    return true;
                }
                else {
                    static_assert(to_json_adl<T, JSON> || to_json_method<T, JSON>, "json type dispatch failed at this type");
                }
            }
        }  // namespace internal

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        bool convert_to_json(T&& t, JSONBase<String, Vec, Object>& json) {
            return internal::dispatch_to_json(t, json);
        }

        template <class JSON, class T>
        JSON convert_to_json(const T& t) {
            JSON js;
            if (!convert_to_json(t, js)) {
                return JSON{};
            }
            return js;
        }

        template <class String1, template <class...> class Vec1, template <class...> class Object1,
                  class String2, template <class...> class Vec2, template <class...> class Object2>
        bool convert_to_json(const JSONBase<String1, Vec1, Object1>& t, const JSONBase<String2, Vec2, Object2>& json);

        template <class String1, template <class...> class Vec1, template <class...> class Object1>
        bool convert_to_json(const JSONBase<String1, Vec1, Object1>& from, JSONBase<String1, Vec1, Object1>& to) {
            to = from;  // copy
            return true;
        }

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        bool convert_from_json(const JSONBase<String, Vec, Object>& json, T& t, FromFlag flag = FromFlag::none) {
            return internal::dispatch_from_json(t, json, flag);
        }

        template <class T, class JSON>
        T convert_from_json(const JSON& json, FromFlag flag = FromFlag::none) {
            T t;
            if (!convert_from_json(json, t, flag)) {
                return T{};
            }
            return t;
        }

        template <class String1, template <class...> class Vec1, template <class...> class Object1,
                  class String2, template <class...> class Vec2, template <class...> class Object2>
        bool convert_from_json(const JSONBase<String1, Vec1, Object1>& t, const JSONBase<String2, Vec2, Object2>& json, FromFlag flag = FromFlag::none);

        template <class String1, template <class...> class Vec1, template <class...> class Object1>
        bool convert_from_json(const JSONBase<String1, Vec1, Object1>& from, JSONBase<String1, Vec1, Object1>& to, FromFlag flag = FromFlag::none) {
            to = from;  // copy
            return true;
        }

#define JSON_PARAM_BEGIN(base, json)                       \
    {                                                      \
        auto& ref____ = base;                              \
        auto& json___ = json;                              \
        if (!json___.is_undef() && !json___.is_object()) { \
            return false;                                  \
        }

#define FROM_JSON_PARAM(param, name, ...)                                            \
    {                                                                                \
        auto elm___ = json___.at(name);                                              \
        if (!elm___) {                                                               \
            return false;                                                            \
        }                                                                            \
        if (!convert_from_json(*elm___, ref____.param __VA_OPT__(, ) __VA_ARGS__)) { \
            return false;                                                            \
        }                                                                            \
    }
#define FROM_JSON_OPT(param, name, ...)                                              \
    if (auto elm___ = json___.at(name)) {                                            \
        if (!convert_from_json(*elm___, ref____.param __VA_OPT__(, ) __VA_ARGS__)) { \
            return false;                                                            \
        }                                                                            \
    }

#define FROM_JSON_OPT_SET(param, name, set_flag, ...)                                 \
    if (auto elm___ = json___.at(name)) {                                             \
        if (!convert_from_json(*elm___, ref____.param, __VA_OPT__(, ) __VA_ARGS__)) { \
            return false;                                                             \
        }                                                                             \
        set_flag = true;                                                              \
    }

#define TO_JSON_PARAM(param, name)                            \
    {                                                         \
        if (!convert_to_json(ref____.param, json___[name])) { \
            return false;                                     \
        }                                                     \
    }

#define TO_JSON_OPT(cond, param, name)                        \
    if (cond) {                                               \
        if (!convert_to_json(ref____.param, json___[name])) { \
            return false;                                     \
        }                                                     \
    }

#define JSON_PARAM_END() \
    return true;         \
    }
#define JSON_PARAM_END_NORET() \
    }

    }  // namespace json
}  // namespace futils
