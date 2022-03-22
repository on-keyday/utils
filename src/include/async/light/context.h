/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// context - light context
#pragma once
#include "make_arg.h"

namespace utils {
    namespace async {
        namespace light {
            struct ContextBase;

#ifdef _WIN32
#define ASYNC_NO_VTABLE_ __declspec(novtable)
#else
#define ASYNC_NO_VTABLE_
#endif
            struct ASYNC_NO_VTABLE_ Executor {
                virtual void execute() = 0;
                virtual void* return_pointer() = 0;
                virtual ~Executor() {}
            };

            struct Setter {
                virtual void set(void* toptr, void* fromptr);
            };

            // native context suite

            struct native_context;

            // create native_context
            native_context* create_native_context(Executor* exec, void (*deleter)(Executor*));

            // invoke executor
            bool invoke_executor(native_context* ctx);

            // delete native_context
            void delete_native_context(native_context* ctx);
            bool switch_context(native_context* from, native_context* to);
            bool set_return_value(native_context* ctx, void* valptr, Setter* setter);

            template <class Ret, class... Other>
            struct ArgExecutor : Executor {
                FuncRecord<Ret, Other...> record;
                void execute() override {
                    record.execute();
                }

                void* return_pointer() override {
                    if constexpr (std::is_same_v<Ret, void>) {
                        return nullptr;
                    }
                    else {
                        return std::addressof(record.retobj.value);
                    }
                }
            };

            template <class T>
            struct RetSetter : Setter {
                void set(void* to, void* from) override {
                    if constexpr (std::is_same_v<T, void>) {
                        return;
                    }
                    else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<T>) {
                        auto to_assign = static_cast<std::remove_reference_t<T>**>(to);
                        *to_assign = static_cast<std::remove_reference_t<T>*>(from);
                    }
                    else if constexpr (std::is_const_v<T>) {
                        auto to_assign = static_cast<T*>(to);
                        auto assign_from = static_cast<T*>(from);
                        *to_assign = *assign_from;
                    }
                    else {
                        auto to_assign = static_cast<T*>(to);
                        auto assign_from = static_cast<T*>(from);
                        *to_assign = std::move(*assign_from);
                    }
                }
            };

            template <class T>
            struct Future {
               private:
                // Context<T> ctx;

               public:
            };

            struct ContextBase {
               private:
                friend void invoke_executor(void*, Executor*);
                void* current = nullptr;
                ContextBase();

               public:
                void suspend();
            };

            template <class T>
            struct Context : ContextBase {
               private:
                void yield(T t) {
                    auto v = RetSetter<T>{};
                    set_return_value(this->current, std::addressof(t), &v);
                    suspend();
                }
                template <class U>
                U await(Future<U> u) {
                }
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
