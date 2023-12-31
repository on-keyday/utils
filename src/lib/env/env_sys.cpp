/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>
#include <env/env_sys.h>
#include <env/env.h>
#include <helper/defer.h>
#include <binary/term.h>
#include <platform/detect.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <Windows.h>
#else
extern char** environ;
#include <stdlib.h>
#endif

namespace utils {
    namespace env::sys {
        bool get_cb(env_value_t k, auto&& cb) {
            // HACK(on-keyday):
            // avoid allocation if end is null character
            // but maybe this area is out of range
            if (*k.end() == 0) {
                return cb(k);
            }
            wrap::path_string key;
            key.assign(k.data(), k.size());
            return cb(key);
        }

        bool set_cb(env_value_t k, env_value_t v, auto&& cb) {
            if (k.null() || binary::has_term(k, 0) || binary::has_term(v, 0)) {
                return false;
            }
            auto visit = [&](auto&& cb) {
                auto visit_value = [&](env_value_t key) {
                    if (v.null()) {
                        return cb(key, v);
                    }
                    if (*v.end() == 0) {
                        return cb(key, v);
                    }
                    wrap::path_string value;
                    value.assign(v.data(), v.size());
                    return cb(key, value);
                };
                // HACK(on-keyday):
                // avoid allocation if end is null character
                // but maybe this area is out of range
                if (*k.end() == 0) {
                    return visit_value(k);
                }
                wrap::path_string key;
                key.assign(k.data(), k.size());
                return visit_value(key);
            };
            return visit(cb);
        }

#ifdef UTILS_PLATFORM_WINDOWS
        struct WinEnv {
            bool get_environ(EnvSetter s) {
                auto f = GetEnvironmentStringsW();
                if (!f) {
                    return false;
                }
                const auto d = helper::defer([&] {
                    FreeEnvironmentStringsW(f);
                });
                auto i = 0;
                auto get = [&] {
                    auto p = i;
                    while (f[i] != 0) {
                        i++;
                    }
                    i++;  // consume null byte
                    return env_value_t(f + p, f + i - 1);
                };
                for (auto kv = get(); kv.size() != 0; kv = get()) {
                    auto [key, value] = env::get_key_value(kv, true);
                    if (key.null()) {
                        return false;
                    }
                    if (!s.set(key, value)) {
                        return false;
                    }
                }
                return true;
            }

            bool get(env_value_t k, EnvSetter s) {
                if (k.null()) {
                    return get_environ(s);
                }
                return get_cb(k, [&](env_value_t key) {
                    wrap::path_char d[101]{};
                    SetLastError(NO_ERROR);
                    auto len = GetEnvironmentVariableW(key.data(), d, 100);
                    if (len == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
                        return false;
                    }
                    if (len <= 100) {
                        return s.set(k, env_value_t{d, len});
                    }
                    wrap::path_string ps;
                    ps.resize(len);
                    len = GetEnvironmentVariableW(key.data(), ps.data(), ps.size());
                    if (len == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
                        return false;
                    }
                    return s.set(k, env_value_t{ps.data(), len});
                });
            }

            bool set(env_value_t k, env_value_t v) {
                return set_cb(k, v, [](env_value_t key, env_value_t value) {
                    return SetEnvironmentVariableW(key.data(), value.data());
                });
            }
        };

        static WinEnv sys_env;
#else

        struct UnixEnv {
            bool get_environ(EnvSetter s) {
                for (auto i = 0; environ[i]; i++) {
                    auto [key, value] = env::get_key_value(env_value_t(static_cast<const char*>(environ[i])));
                    if (key.null()) {
                        return false;
                    }
                    if (!s.set(key, value)) {
                        return false;
                    }
                }
                return true;
            }

            bool get(env_value_t k, EnvSetter s) {
                if (k.null()) {
                    return get_environ(s);
                }
                return get_cb(k, [&](env_value_t key) {
                    auto res = getenv(key.data());
                    if (!res) {
                        return false;
                    }
                    return s.set(key, env_value_t{static_cast<const char*>(res)});
                });
            }

            bool set(env_value_t k, env_value_t v) {
                return set_cb(k, v, [](env_value_t key, env_value_t value) {
                    if (value.null()) {
                        return unsetenv(key.data()) == 0;
                    }
                    return setenv(key.data(), value.data(), 1) == 0;
                });
            }
        };

        static UnixEnv sys_env;
#endif

        EnvGetter STDCALL env_getter() {
            return sys_env;
        }

        EnvSetter STDCALL env_setter() {
            return sys_env;
        }
    }  // namespace env::sys
}  // namespace utils
