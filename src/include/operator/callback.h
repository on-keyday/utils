/*license*/

// callback - callback object
#pragma once

#include "../helper/sfinae.h"

namespace utils {
    namespace ops {

        struct DefaultHandler {
            template <class T>
            static T default_value() {
                return T();
            }

            template <class U, class... Args>
            static U* new_cb(Args&&... args) {
                return new U(std::forward<Args>(args)...);
            }

            template <class U>
            static void delete_cb(U* v) {
                delete v;
            }
        };

        template <class Ret>
        struct HandlerTraits {
            SFINAE_BLOCK_T_BEGIN(has_default_value, T::default_value())
            static Ret invoke() {
                return T::default_value();
            }
            SFINAE_BLOCK_T_ELSE(has_default_value)
            static Ret invoke() {
                return DefaultHandler::template default_value<Ret>();
            }
            SFINAE_BLOCK_T_END()

            SFINAE_BLOCK_TU_BEGIN(has_delete_cb, T::template delete_cb<U>())
            template <class... Args>
            static U invoke(Args&&... args) {
                return T::template delete_cb<U>(std::forward<Args>(args));
            }
            SFINAE_BLOCK_TU_ELSE(has_delete_cb)
            template <class... Args>
            static U invoke(Args&&... args) {
                return DefaultHandler::template delete_cb<U>(std::forward<Args>(args));
            }
            SFINAE_BLOCK_TU_END()

            SFINAE_BLOCK_TU_BEGIN(has_new_cb, T::template new_cb<U>())
            template <class... Args>
            static U invoke(Args&&... args) {
                return T::template new_cb<U>(std::forward<Args>(args));
            }
            SFINAE_BLOCK_TU_ELSE(has_new_cb)
            template <class... Args>
            static U invoke(Args&&... args) {
                return DefaultHandler::template new_cb<U>(std::forward<Args>(args));
            }
            SFINAE_BLOCK_TU_END()

            template <class T, class U, class... Args>
            U* new_cb(Args&&... args) {
                return has_new_cb<T, U>::template invoke(std::forward<Args>(args)...);
            }

            template <class T, class U, class... Args>
            U* delete_cb(Args&&... args) {
                return has_delete_cb<T, U>::template invoke(std::forward<Args>(args)...);
            }

            template <class T>
            Ret default_value() {
                return has_default_value<T>::invoke();
            }
        };

        template <class Ret, class Handler = DefaultHandler, class... Args>
        struct Callback {
            struct Base {
                virtual Ret operator()(Args&&...) = 0;
                virtual Ret operator()(Args&&...) const = 0;
                virtual ~Base() {}
            };

            template <class V>
            struct Impl {
                V t;

                SFINAE_BLOCK_T_BEGIN(is_callable_u, std::declval<T>()(std::declval<Args>()...))
                static Ret invoke(T& t, Args&&... args) {
                    t(std::forward<Args>(args));
                    return HandlerTraits<Ret>::template default_value<Handler>();
                }
                SFINAE_BLOCK_T_ELSE(is_callable_u)
                static Ret invoke(T& t, Args&&... args) {
                    return HandlerTraits<Ret>::template default_value<Handler>();
                }
                SFINAE_BLOCK_T_END()

                SFINAE_BLOCK_T_BEGIN(is_callable, static_cast<Ret>(std::declval<T>()(std::declval<Args>()...)))
                static Ret invoke(T& t, Args&&... args) {
                    return static_cast<Ret>(t(std::forward<Args>(args)));
                }

                SFINAE_BLOCK_T_ELSE(is_callable)

                static Ret invoke(T& t, Args&&... args) {
                    return is_callable_u<T>::invoke(t, std::forward<Args>(args)...);
                }
                SFINAE_BLOCK_T_END()

                template <class... Arg>
                Impl(Arg&&... args)
                    : t(std::forward<Arg>(args)...) {}

                Ret operator()(Args&&... args) override {
                    return is_callable<V>::invoke(t, std::forward<Args>(args));
                }

                Ret operator()(Args&&... args) const override {
                    return is_callable<const V>::invoke(t, std::forward<Args>(args));
                }
                virtual ~Impl() {}
            };

           private:
            Base* base = nullptr;
            template <class T>
            void make_cb(T&& t) {
                using decay_T = std::decay_t<T>;
                base = HandlerTraits<Ret>::template new_cb<Handler, Impl<decay_T>>(Impl<decay_T>(std::forward<T>(t)));
            }

            void del_cb() {
                HandlerTraits<Ret>::template delete_cb<Handler, Base>(base);
                base = nullptr;
            }

           public:
            constexpr Callback() {}

            template <class T>
            Callback(T&& t) {
                make_cb(std::forward<T>(t));
            }

            Callback(Callback&& cb) {
                base = cb.base;
                cb.base = nullptr;
            }

            Callback& operator=(Callback&& cb) {
                del_cb();
                base = cb.base;
                cb.base = nullptr;
                return *this;
            }

            template <class... Carg>
            Ret operator()(Carg&&... args) {
                return (*base)(std::forward<Carg>(args)...);
            }

            template <class... Carg>
            Ret operator()(Carg&&... args) const {
                return (*base)(std::forward<Carg>(args)...);
            }

            ~Callback() {
                del_cb();
            }
        };

        template <class Ret, class... Args>
        using DefaultCallback = Callback<Ret, DefaultHandler, Args...>;
    }  // namespace ops
}  // namespace utils
