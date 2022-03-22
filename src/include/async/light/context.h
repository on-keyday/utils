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
            struct Context;
#ifdef _WIN32
#define ASYNC_NO_VTABLE_ __declspec(novtable)
#else
#define ASYNC_NO_VTABLE_
#endif
            struct ASYNC_NO_VTABLE_ Executor {
                virtual void execute(Context) = 0;
                virtual ~Executor() {}
            };

            template <class Ret, class Fn, class... Arg>
            struct FuncRecord {
                Fn fn;
                Args<Arg...> args;
                Args<Ret> retobj;

                FuncRecord(Fn&& in, Arg&&... arg)
                    : fn(std::forward<Fn>(in)),
                      args(make_arg(std::forward<Arg>(arg)...)) {
                }

                void execute() {
                    if constexpr (std::is_same_v<Ret, void>) {
                        args.invoke(fn);
                    }
                    else {
                        retobj = args.invoke(fn);
                    }
                }
            };

            template <class Fn, class... Arg>
            auto make_funcrecord(Fn&& fn, Arg&&... arg) {
                using invoke_res = decltype(std::declval<Args<arg_t<Arg>...>>().invoke(std::declval<Fn>()));
                return FuncRecord<invoke_res, std::decay_t<Fn>, arg_t<Arg>...>{
                    std::forward<Fn>(fn),
                    std::forward<Arg>(arg)...,
                };
            }

            struct Context {
            };
        }  // namespace light
    }      // namespace async
}  // namespace utils
