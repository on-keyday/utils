/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// env_win - system environment variables
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "../wrap/light/char.h"
#include "../wrap/light/string.h"
#include <memory>
#include "../view/iovec.h"
#include "../helper/disable_self.h"
#include "../unicode/utf/convert.h"

namespace utils {
    namespace env::sys {
        using env_value_t = view::basic_rvec<wrap::path_char>;

        namespace internal {
            template <class T>
            concept has_set = requires(T t) {
                { t.set(env_value_t{}, env_value_t{}) };
            };

        }  // namespace internal

        struct EnvSetter {
           private:
            void* p = nullptr;
            bool (*set_)(void*, env_value_t key, env_value_t value) = nullptr;
            template <class Ptr>
            constexpr static bool do_set(void* p, env_value_t key, env_value_t value) {
                auto& v = *static_cast<Ptr>(p);
                if constexpr (internal::has_set<decltype(v)>) {
                    return v.set(key, value);
                }
                else {
                    return v(key, value);
                }
            }

           public:
            template <class T, helper_disable_self(EnvSetter, T)>
            constexpr EnvSetter(T&& v)
                : p(std::addressof(v)), set_(do_set<decltype(std::addressof(v))>) {}

            constexpr EnvSetter() = default;
            constexpr EnvSetter(EnvSetter&&) = default;
            constexpr EnvSetter(const EnvSetter&) = default;
            constexpr EnvSetter& operator=(EnvSetter&&) = default;
            constexpr EnvSetter& operator=(const EnvSetter&) = default;

            constexpr bool set(env_value_t key, env_value_t value) {
                if (!set_) {
                    return false;
                }
                return set_(p, key, value);
            }

            template <class Buf = wrap::path_string, class Key, class Value>
            constexpr bool set(Key&& key, Value&& value) {
                Buf k, v;
                utf::convert(key, k);
                utf::convert(value, v);
                return set(env_value_t{k}, env_value_t{v});
            }
        };

        namespace internal {
            template <class T>
            concept has_get = requires(T t) {
                { t.get(env_value_t{}, EnvSetter{}) };
            };
        }

        struct EnvGetter {
           private:
            void* p = nullptr;
            bool (*get_)(void*, env_value_t key, EnvSetter set) = nullptr;
            template <class Ptr>
            constexpr static bool do_get(void* p, env_value_t key, EnvSetter set) {
                auto& v = *static_cast<Ptr>(p);
                if constexpr (internal::has_get<decltype(v)>) {
                    return v.get(key, set);
                }
                else {
                    return v(key, set);
                }
            }

           public:
            template <class T, helper_disable_self(EnvGetter, T)>
            constexpr EnvGetter(T&& v)
                : p(std::addressof(v)), get_(do_get<decltype(std::addressof(v))>) {}

            constexpr EnvGetter() = default;
            constexpr EnvGetter(EnvGetter&&) = default;
            constexpr EnvGetter(const EnvGetter&) = default;
            constexpr EnvGetter& operator=(EnvGetter&&) = default;
            constexpr EnvGetter& operator=(const EnvGetter&) = default;

            constexpr bool get(env_value_t key, EnvSetter set) {
                if (!get_) {
                    return false;
                }
                return get_(p, key, set);
            }

            template <class Buf = wrap::path_string, class Key, class Value,
                      std::enable_if_t<strutil::is_utf_convertable<Key>,
                                       int> = 0>
            constexpr bool get(Value&& value, Key&& key) {
                Buf buf;
                utf::convert(key, buf);
                auto set = [&](env_value_t key, env_value_t val) {
                    utf::convert(val, value);
                    return true;
                };
                return get(env_value_t{buf}, EnvSetter{set});
            }

            template <class Buf = wrap::path_string, class Key, class Value, class Default = Value,
                      std::enable_if_t<strutil::is_utf_convertable<Key>,
                                       int> = 0>
            constexpr void get_or(Value&& value, Key&& key, Default&& default_) {
                if (!get<Buf>(value, key)) {
                    value = utf::convert<std::decay_t<Value>>(default_);
                }
            }

            template <class Value, class Buf = wrap::path_string, class Key,
                      std::enable_if_t<strutil::is_utf_convertable<Key>,
                                       int> = 0>
            constexpr Value get(Key&& key) {
                Value val;
                get<Buf>(val, key);
                return val;
            }

            template <class Value, class Buf = wrap::path_string, class Key, class Default = Value,
                      std::enable_if_t<strutil::is_utf_convertable<Key>,
                                       int> = 0>
            constexpr Value get_or(Key&& key, Default&& default_) {
                Value buf;
                get_or<Buf>(buf, key, default_);
                return buf;
            }
        };

        namespace test {
            constexpr bool check_setter_getter() {
                EnvSetter set;
                bool ok = false;
                auto set_fn = [&](env_value_t key, env_value_t value) {
                    ok = true;
                    return true;
                };
                auto get_fn = [&](env_value_t key, EnvSetter set) {
                    return set.set(key, env_value_t{});
                };
                set = EnvSetter{set_fn};
                EnvGetter get;
                get = EnvGetter{get_fn};
                if (!std::is_constant_evaluated()) {
                    get.get(env_value_t{}, set);
                    return ok;
                }
                // set.set(env_value_t{}, env_value_t{});
                return true;
            }

            static_assert(check_setter_getter());
        }  // namespace test

        DLL EnvGetter STDCALL env_getter();
        DLL EnvSetter STDCALL env_setter();

        template <class BufType, class Convert = wrap::path_string>
        constexpr auto expand_sys() {
            return [](auto&& out, auto&& read) {
                BufType buf;
                if (!read(buf)) {
                    return;
                }
                Convert cvt;
                utf::convert(buf, cvt);
                auto env = env_getter();
                env.get(cvt, [&](env_value_t key, env_value_t value) {
                    utf::convert(value, out);
                    return true;
                });
            };
        }
    }  // namespace env::sys
}  // namespace utils
