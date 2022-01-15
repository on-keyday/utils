/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// to_json - to_json and from_json helper
#pragma once

#include "jsonbase.h"
#include "../helper/sfinae.h"

namespace utils {
    namespace json {
        enum class FromFlag {
            none = 0x0,
            force_element = 0x1,
        };

        namespace internal {

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct ToJSONHelper {
                using JSON = JSONBase<String, Vec, Object>;

                SFINAE_BLOCK_T_BEGIN(has_array_interface, (std::declval<T&>()[0], std::declval<T&>().size()))
                static bool invoke(T& t, JSON& json) {
                    json.init_array();
                    for (size_t i = 0; i < t.size(); i++) {
                        JSON v;
                        using recursion = is_primitive<std::remove_reference_t<decltype(t[i])>>;
                        auto err = recursion::invoke(t[i], v);
                        if (!err) {
                            return false;
                        }
                        json.push_back(std::move(v));
                    }
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(has_array_interface)
                static bool invoke(...) {
                    static_assert(false, R"(type not implemented any attribute to_json)");
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_map_interface, (std::get<1>(*std::declval<T&>().begin()), std::declval<T&>.end()))
                static bool invoke(T& t, JSON& json) {
                    json.init_obj();
                    for (auto& v : t) {
                        using recursion = is_primitive<std::remove_reference_t<decltype(v)>>;
                        auto err = recursion::invoke(std::get<1>(v), json[std::get<0>(v)]);
                        if (!err) {
                            return false;
                        }
                    }
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(has_map_interface)
                static bool invoke(T& t, JSON& json) {
                    return has_array_interface<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_to_json_adl, to_json(std::declval<T&>(), std::declval<JSON&>()))
                static bool invoke(T& t, JSON& json) {
                    return to_json(t, json);
                }
                SFINAE_BLOCK_T_ELSE(has_to_json_adl)
                static bool invoke(T& t, JSON& json) {
                    return has_map_interface<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_to_json_member, std::declval<T&>().to_json(std::declval<JSON&>()))
                static bool invoke(T& t, JSON& json) {
                    return t.to_json(json);
                }
                SFINAE_BLOCK_T_ELSE(has_to_json_member)
                static bool invoke(T& t, JSON& json) {
                    return has_to_json_adl<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_primitive, std::declval<JSON&>() = std::declval<T>())
                static bool invoke(T& t, JSON& json) {
                    json = t;
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(is_primitive)
                static bool invoke(T& t, JSON& json) {
                    return has_to_json_member<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                template <class T>
                static bool invoke(T& t, JSON& json) {
                    return is_primitive<T>::invoke(t, json);
                }
            };

            DEFINE_ENUM_FLAGOP(FromFlag)

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct FromJSONHelper {
                using JSON = JSONBase<String, Vec, Object>;

                SFINAE_BLOCK_T_BEGIN(has_array_interface, std::declval<T&>().push_back(std::declval<helper::append_size_t<T>>()))
                static bool invoke(T& t, JSON& json, FromFlag flag) {
                    if (!json.is_array()) {
                        return false;
                    }
                    for (auto a = json.abegin(); a != json.aend(); a++) {
                        using elm_t = helper::append_size_t<T>;
                        elm_t v;
                        using recursion = is_json<elm_t>;
                        auto err = recursion::invoke(v, *a, flag);
                        if (!err) {
                            return false;
                        }
                        t.push_back(std::move(v));
                    }
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(has_array_interface)
                static bool invoke(...) {
                    static_assert(false, R"(type not implemented any attribute from_json)");
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_map_interface, std::declval<T&>().emplace(std::declval<const String>(), std::declval<T>()[std::declval<const String>()]))
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    if (!json.is_object()) {
                        return false;
                    }
                    for (auto o = json.obegin(); o != json.oend(); o++) {
                        using elm_t = std::remove_reference_t<decltype(std::declval<T>()[std::declval<const String>()])>;
                        elm_t v;
                        using recursion = is_json<elm_t>;
                        auto err = recursion::invoke(v, std::get<1>(*o), flag);
                        if (!err) {
                            return false;
                        }
                        auto e = t.emplace(std::get<0>(*o), std::move(v));
                        if (!std::get<1>(e)) {
                            return false;
                        }
                    }
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(has_map_interface)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return has_array_interface<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_from_json_adl, from_json(std::declval<T&>(), std::declval<JSON&>()))
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return from_json(t, json);
                }
                SFINAE_BLOCK_T_ELSE(has_from_json_adl)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return has_map_interface<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_from_json_member, std::declval<T&>().from_json(std::declval<JSON&>()))
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return t.from_json(json);
                }
                SFINAE_BLOCK_T_ELSE(has_from_json_member)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return has_from_json_adl<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_string, (std::enable_if_t<helper::is_utf_convertable<T>>)0)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    t = T{};
                    if (any(flag & FromFlag::force_element)) {
                        return json.force_as_string(t);
                    }
                    else {
                        return json.as_string(t);
                    }
                }
                SFINAE_BLOCK_T_ELSE(is_string)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return has_from_json_member<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_number, (std::enable_if_t<std::is_arithmetic_v<T>>)0)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    if (any(flag & FromFlag::force_element)) {
                        return json.force_as_number(t);
                    }
                    else {
                        return json.as_number(t);
                    }
                }
                SFINAE_BLOCK_T_ELSE(is_number)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return is_string<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_null, (std::enable_if_t<std::is_same_v<T, std::nullptr_t>>)0)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    if (any(flag & FromFlag::force_element)) {
                        return json.force_is_null();
                    }
                    else {
                        return json.is_null();
                    }
                }
                SFINAE_BLOCK_T_ELSE(is_null)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return is_number<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_bool, (std::enable_if_t<std::is_same_v<T, bool>>)0)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    if (any(flag & FromFlag::force_element)) {
                        return json.force_as_bool(t);
                    }
                    else {
                        return json.as_bool(t);
                    }
                }
                SFINAE_BLOCK_T_ELSE(is_bool)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return is_json<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_json, (std::enable_if_t<std::is_same_v<T, JSONBase<String, Vec, Object>>>)0)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    t = json;
                }
                SFINAE_BLOCK_T_ELSE(is_json)
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return is_bool<T>::invoke(t, json, flag);
                }
                SFINAE_BLOCK_T_END()

                template <class T>
                static bool invoke(T& t, const JSON& json, FromFlag flag) {
                    return is_json<T>::invoke(t, json, flag);
                }
            };

        }  // namespace internal

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        bool convert_to_json(T&& t, JSONBase<String, Vec, Object>& json) {
            return internal::ToJSONHelper<String, Vec, Object>::invoke(t, json);
        }

        template <class String1, template <class...> class Vec1, template <class...> class Object1,
                  class String2, template <class...> class Vec2, template <class...> class Object2>
        bool convert_to_json(const JSONBase<String1, Vec1, Object1>& t, const JSONBase<String2, Vec2, Object2>& json);

        template <class T, class String, template <class...> class Vec, template <class...> class Object>
        bool convert_from_json(const JSONBase<String, Vec, Object>& json, T& t, FromFlag flag = FromFlag::none) {
            return internal::FromJSONHelper<String, Vec, Object>::invoke(t, json, flag);
        }

        template <class String1, template <class...> class Vec1, template <class...> class Object1,
                  class String2, template <class...> class Vec2, template <class...> class Object2>
        bool convert_from_json(const JSONBase<String1, Vec1, Object1>& t, const JSONBase<String2, Vec2, Object2>& json, FromFlag flag = FromFlag::none);

#define JSON_PARAM_BEGIN(base, json)                       \
    {                                                      \
        auto& ref____ = base;                              \
        auto& json___ = json;                              \
        if (!json___.is_undef() && !json___.is_object()) { \
            return false;                                  \
        }

#define FROM_JSON_PARAM(param, name)                      \
    {                                                     \
        auto elm___ = json___.at(name);                   \
        if (!elm___) {                                    \
            return false;                                 \
        }                                                 \
        if (!convert_from_json(*elm___, ref____.param)) { \
            return false;                                 \
        }                                                 \
    }
#define FROM_JSON_JSONPARAM(param, name) \
    {                                    \
        auto elm___ = json___.at(name);  \
        if (!elm___) {                   \
            return false;                \
        }                                \
        ref____.param = *elm___;         \
    }

#define TO_JSON_PARAM(param, name)                            \
    {                                                         \
        if (!convert_to_json(ref____.param, json___[name])) { \
            return false;                                     \
        }                                                     \
    }

#define TO_JSON_JSONPARAM(param, name) \
    { json___[name] = ref____.param; }

#define JSON_PARAM_END() \
    return true;         \
    }
    }  // namespace json
}  // namespace utils
