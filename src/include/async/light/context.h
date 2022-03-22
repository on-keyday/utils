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
                virtual ~Executor() {}
            };

            void* create_native_context();
            void invoke_executor(void* ctx, Executor* exec);
            void delete_native_context(void* ctx);
            bool switch_context(void* from, void* to);

            struct ContextBase {
               private:
                friend void invoke_executor(void*, Executor*);
                void* current = nullptr;
                ContextBase();

               public:
                void suspend();
            };

            template <class T>
            struct Future {
               private:
                Context<T> ctx;

               public:
            };

            template <class... Arg>
            struct ArgExecutor : Executor {
                FuncRecord<Arg...> args;
                void execute() override {
                    args.execute();
                }
            };

            template <class T>
            struct Context : ContextBase {
               private:
               public:
                void yield(T t) {
                    switch_context();
                }
                template <class U>
                U await(Future<U> u) {
                }
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
