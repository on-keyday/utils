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
                    return has_to_json_member<T>::invoke(t, json);
                }
            };

            template <class String, template <class...> class Vec, template <class...> class Object>
            struct FromJSONHelper {
                using JSON = JSONBase<String, Vec, Object>;

                SFINAE_BLOCK_T_BEGIN(has_array_interface, std::declval<T&>().push_back(std::declval<typename T::value_type>()))
                static bool invoke(T& t, JSON& json) {
                    if (!json.is_array()) {
                        return false;
                    }
                    for (auto a = json.abegin(); a != json.aend(); a++) {
                        typename T::value_type v;
                        using recursion = is_bool<std::remove_reference_t<typename T::value_type>>;
                        auto err = recursion::invoke(v, *a);
                        if (!err) {
                            return false;
                        }
                        t.push_back(std::move(v));
                    }
                    return true;
                }
                SFINAE_BLOCK_T_ELSE(has_array_interface)
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_map_interface, std::declval<T&>().emplace(std::declval<const String>(), std::declval<typename T::value_type>()))
                static bool invoke(T& t, const JSON& json) {
                    if (!json.is_object()) {
                        return false;
                    }
                    for (auto o = json.obegin(); o != json.oend(); o++) {
                        typename T::value_type v;
                        using recursion = is_bool<std::remove_reference_t<typename T::value_type>>;
                        auto err = recursion::invoke(v, std::get<1>(*o));
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
                static bool invoke(T& t, const JSON& json) {
                    return has_array_interface<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_from_json_adl, from_json(std::declval<T&>(), std::declval<JSON&>()))
                static bool invoke(T& t, const JSON& json) {
                    return from_json(t, json);
                }
                SFINAE_BLOCK_T_ELSE(has_from_json_adl)
                static bool invoke(T& t, const JSON& json) {
                    return has_map_interface<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(has_from_json_member, std::declval<T&>().from_json(std::declval<JSON&>()))
                static bool invoke(T& t, const JSON& json) {
                    return t.from_json(json);
                }
                SFINAE_BLOCK_T_ELSE(has_from_json_member)
                static bool invoke(T& t, const JSON& json) {
                    return has_from_json_adl<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_string, (std::enable_if_t<helper::is_utf_convertable<T>>)0)
                static bool invoke(T& t, const JSON& json) {
                    return json.as_string(t);
                }
                SFINAE_BLOCK_T_ELSE(is_string)
                static bool invoke(T& t, const JSON& json) {
                    return has_from_json_member<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_number, (std::enable_if_t<std::is_arithmetic_v<T>>)0)
                static bool invoke(T& t, const JSON& json) {
                    return json.as_number(t);
                }
                SFINAE_BLOCK_T_ELSE(is_number)
                static bool invoke(T& t, const JSON& json) {
                    return is_string<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_bool, (std::enable_if_t<std::is_same_v<T, bool>>)0)
                static bool invoke(T& t, const JSON& json) {
                    return json.as_bool(t);
                }
                SFINAE_BLOCK_T_ELSE(is_bool)
                static bool invoke(T& t, const JSON& json) {
                    return is_number<T>::invoke(t, json);
                }
                SFINAE_BLOCK_T_END()

                template <class T>
                static bool invoke(T& t, const JSON& json) {
                    return is_bool<T>::invoke(t, json);
                }
            };

            struct ConvertToJSONObject {
                constexpr ConvertToJSONObject() {}

                ConvertToJSONObject(const ConvertToJSONObject&) = delete;
                ConvertToJSONObject& operator=(const ConvertToJSONObject&) = delete;

                template <class T, class String, template <class...> class Vec, template <class...> class Object>
                bool operator()(T&& t, JSONBase<String, Vec, Object>& json) const {
                    return ToJSONHelper<String, Vec, Object>::invoke(t, json);
                }

                template <class String1, template <class...> class Vec1, template <class...> class Object1,
                          class String2, template <class...> class Vec2, template <class...> class Object2>
                bool operator()(JSONBase<String1, Vec1, Object1>& t, JSONBase<String2, Vec2, Object2>& json);
            };

            struct ConvertFromJSONObject {
                constexpr ConvertFromJSONObject() {}

                ConvertFromJSONObject(const ConvertFromJSONObject&) = delete;
                ConvertFromJSONObject& operator=(const ConvertFromJSONObject&) = delete;
                template <class T, class String, template <class...> class Vec, template <class...> class Object>
                bool operator()(const JSONBase<String, Vec, Object>& json, T& t) const {
                    return FromJSONHelper<String, Vec, Object>::invoke(t, json);
                }

                template <class String1, template <class...> class Vec1, template <class...> class Object1,
                          class String2, template <class...> class Vec2, template <class...> class Object2>
                bool operator()(const JSONBase<String1, Vec1, Object1>& t, const JSONBase<String2, Vec2, Object2>& json);
            };
        }  // namespace internal

        constexpr internal::ConvertToJSONObject convert_to_json{};

        constexpr internal::ConvertFromJSONObject convert_from_json{};
#define FROM_JSON_PARAM_BEGIN(base, json) \
    {                                     \
        auto& ref____ = base;             \
        auto& json___ = json;
#define FROM_JSON_PARAM(param, name)                     \
    {                                                    \
        auto err___ = json___.at(name);                  \
        if (!err___) {                                   \
            return false;                                \
        }                                                \
        if (!convert_from_json(*err___, ref___.param)) { \
            return false;                                \
        }                                                \
    }

#define FROM_JSON_PARAM_END() }
    }  // namespace json
}  // namespace utils
