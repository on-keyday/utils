/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error_log - error and log interface
#pragma once
#include "storage.h"
#include "base_iface.h"

namespace utils {
    namespace iface {
        template <class T = void>
        struct LogObject {
            const char* file;
            size_t line;
            const char* kind;
            const char* msg;
            T* ctx;
            std::int64_t code;
        };

        namespace internal {
            template <class T>
            concept has_code = requires(T t) {
                { t.code() } -> std::integral;
            };

            template <class T>
            concept has_kind_of = requires(T t) {
                {!t.kind_of("kind")};
            };

            template <class T>
            concept has_unwrap = requires(T t) {
                {t.unwrap()};
            };

            template <class T>
            concept has_cancel = requires(T t) {
                {!t.cancel()};
            };

            template <class T>
            concept has_err = requires(T t) {
                {t.err()};
            };

            template <class T, class C>
            concept has_log = requires(T t) {
                {t.log(std::declval<LogObject<C>*>())};
            };
        }  // namespace internal

        template <size_t N>
        struct fixedBuf {
            char buf[N + 1]{0};
            size_t index = 0;
            const char* c_str() const {
                return buf;
            }
            void push_back(char v) {
                if (index >= N) {
                    return;
                }
                buf[index] = v;
                index++;
            }
            char operator[](size_t i) {
                if (index >= i) {
                    return char();
                }
                return buf[i];
            }
        };

        template <class Box, class Unwrap = void>
        struct Error : Box {
           private:
            using unwrap_t = std::conditional_t<std::is_same_v<Unwrap, void>, Error, Unwrap>;
            MAKE_FN_VOID(error, PushBacker<Ref>)
            std::int64_t (*code_ptr)(void*) = nullptr;
            bool (*kind_of_ptr)(void*, const char* key) = nullptr;
            unwrap_t (*unwrap_ptr)(void*) = nullptr;

            template <class T>
            static std::int64_t code_fn(void* ptr) {
                if constexpr (internal::has_code<T>) {
                    return static_cast<T*>(ptr)->code();
                }
                else {
                    return false;
                }
            }

            template <class T>
            static bool kind_of_fn(void* ptr, const char* kind) {
                if constexpr (internal::has_kind_of<T>) {
                    return static_cast<T*>(ptr)->kind_of(kind);
                }
                else {
                    return false;
                }
            }

            template <class T>
            static unwrap_t unwrap_fn(void* ptr) {
                if constexpr (internal::has_unwrap<T>) {
                    return static_cast<T*>(ptr)->unwrap();
                }
                else {
                    return {};
                }
            }

           public:
            DEFAULT_METHODS(Error)

            template <class T>
            constexpr Error(T&& t)
                : APPLY2_FN(error, PushBacker<Ref>),
                  APPLY2_FN(code),
                  APPLY2_FN(kind_of),
                  APPLY2_FN(unwrap),
                  Box(std::forward<T>(t)) {}

            void error(PushBacker<Ref> ref) const {
                DEFAULT_CALL(error, (void)0, ref);
            }

            template <class String = fixedBuf<100>>
            constexpr String serror() const {
                String s;
                error(s);
                return s;
            }

            std::int64_t code() const {
                DEFAULT_CALL(code, 0);
            }

            bool kind_of(const char* kind) const {
                DEFAULT_CALL(kind_of, false, kind);
            }

            unwrap_t unwrap() const {
                DEFAULT_CALL(unwrap, unwrap_t{});
            }

            explicit operator bool() const {
                return (*this) == nullptr;
            }
        };

        template <class Box, class Err, class LogT = void>
        struct Stopper : Box {
           private:
            MAKE_FN(stop, bool)
            void (*log_ptr)(void*, LogObject<LogT>* obj) = nullptr;
            bool (*cancel_ptr)(void*) = nullptr;
            Err (*err_ptr)(void*) = nullptr;

            template <class T>
            static void log_fn(void* ptr, LogObject<LogT>* b) {
                if constexpr (internal::has_log<T, LogT>) {
                    static_cast<T*>(ptr)->log(b);
                }
            }

            template <class T>
            static bool cancel_fn(void* ptr) {
                if constexpr (internal::has_cancel<T>) {
                    return static_cast<T*>(ptr)->cancel();
                }
                else {
                    return false;
                }
            }

            template <class T>
            static Err err_fn(void* ptr) {
                if constexpr (internal::has_err<T>) {
                    return static_cast<T*>(ptr)->err();
                }
                else {
                    return {};
                }
            }

           public:
            DEFAULT_METHODS(Stopper)

            template <class T>
            constexpr Stopper(T&& t)
                : APPLY2_FN(stop),
                  APPLY2_FN(cancel),
                  APPLY2_FN(err),
                  APPLY2_FN(log),
                  Box(std::forward<T>(t)) {}

            bool stop() {
                DEFAULT_CALL(stop, false);
            }

            bool cancel() {
                DEFAULT_CALL(cancel, false);
            }

            Err err() {
                DEFAULT_CALL(err, Err{});
            }

            void log(LogObject<LogT> obj) {
                return this->ptr() && log_ptr ? log_ptr(this->ptr(), &obj) : (void)0;
            }
        };
    }  // namespace iface
}  // namespace utils