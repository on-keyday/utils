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

        struct HandlerTraits {
            SFINAE_BLOCK_TU_BEGIN(has_default_value, T::template default_value<U>())
            static U invoke() {
                return T::template default_value<U>();
            }
            SFINAE_BLOCK_TU_ELSE(has_default_value)
            static U invoke() {
                return DefaultHandler::template default_value<U>();
            }
            SFINAE_BLOCK_TU_END()

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

            template <class T, class U>
            U default_value() {
                return has_default_value<T, U>::invoke();
            }
        };

        template <class Ret, class Handler = DefaultHandler, class... Args>
        struct Callback {
            struct Base {
                virtual Ret operator()(Args&&...) = 0;
                virtual Ret operator()(Args&&...) const = 0;
                virtual ~Base() {}
            };

            template <class T>
            struct Impl {
                T t;
                SFINAE_BLOCK_T_BEGIN(is_callable, static_cast<Ret>(std::declval<T>()(std::declval<Args>()...)))
                static Ret invoke(T& t, Args&&... args) {
                    return static_cast<Ret>(t(std::forward<Args>(args)));
                }
                SFINAE_BLOCK_T_ELSE(is_callable)
                SFINAE_BLOCK_T_BEGIN(is_callable_u, std::declval<T>()(std::declval<Args>()...))
                static Ret invoke(T& t, Args&&... args) {
                    t(std::forward<Args>(args));
                    return Handler<Ret>::default_value();
                }
                SFINAE_BLOCK_T_ELSE(is_callable_u)
                static Ret invoke(T& t, Args&&... args) {
                    return HandlerTraits::default_value<Handler, Ret>();
                }
                SFINAE_BLOCK_T_END()
                static Ret invoke(T& t, Args&&... args) {
                    return is_callable_u<T>::invoke(t, std::forward<Args>(args)...);
                }
                SFINAE_BLOCK_T_END()

                template <class... Args>
                Impl(Args&&... args)
                    : t(std::forward<Args>(args)...) {}

                Ret operator()(Args&&... args) override {
                    return is_callable<T>::invoke(t, std::forward<Args>(args));
                }

                Ret operator()(Args&&... args) const override {
                    return is_callable<const T>::invoke(t, std::forward<Args>(args));
                }
                virtual ~Impl() {}
            };

           private:
            Base* base = nullptr;
            template <class T>
            void make_cb(T&& t) {
                using decay_T = std::decay_t<T>;
                base = HandlerTraits::new_cb<Handler, Impl<decay_T>>(Impl<decay_T>(std::forward<T>(t)));
            }

            void del_cb() {
                HandlerTraits::delete_cb<Handler, Base>(base);
                base = nullptr;
            }

           public:
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

            template <class Carg>
            Ret operator()(Carg&&... args) {
                return (*base)(stf::forward<Carg>(carg)...);
            }

            template <class Carg>
            Ret operator()(Carg&&... args) const {
                return (*base)(stf::forward<Carg>(carg)...);
            }

            ~Callback() {
                del_cb();
            }
        };
    }  // namespace ops
}  // namespace utils
