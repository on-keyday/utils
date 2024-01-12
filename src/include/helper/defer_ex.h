/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "defer.h"
#include <memory>
#include "disable_self.h"

namespace futils::helper {
    struct DynDefer {
       private:
        void* ptr = nullptr;
        void (*fn)(void*, bool del) = nullptr;

       public:
        constexpr DynDefer() = default;
        template <class F, helper_disable_self(DynDefer, F)>
        constexpr DynDefer(F&& f)
            : ptr(new std::decay_t<F>(std::forward<F>(f))), fn([](void* ptr, bool del) {
                  const auto d = helper::defer([&] {
                      delete static_cast<std::decay_t<F>*>(ptr);
                  });
                  if (del) {
                      return;
                  }
                  (*static_cast<std::decay_t<F>*>(ptr))();
              }) {}

        constexpr DynDefer(DynDefer&& f)
            : ptr(std::exchange(f.ptr, nullptr)), fn(std::exchange(f.fn, nullptr)) {}

        constexpr DynDefer& operator=(DynDefer&& f) {
            if (this == &f) return *this;
            cancel();
            ptr = std::exchange(f.ptr, nullptr);
            fn = std::exchange(f.fn, nullptr);
            return *this;
        }

        constexpr void execute() {
            if (ptr && fn) {
                auto d = helper::defer([&] {
                    ptr = nullptr;
                    fn = nullptr;
                });
                fn(ptr, false);
            }
        }

        constexpr void cancel() {
            if (ptr && fn) {
                auto d = helper::defer([&] {
                    ptr = nullptr;
                    fn = nullptr;
                });
                fn(ptr, true);
            }
        }

        constexpr ~DynDefer() {
            execute();
        }
    };

    [[nodiscard]] constexpr DynDefer defer_ex(auto&& fn) {
        return DynDefer(std::forward<decltype(fn)>(fn));
    }
}  // namespace futils::helper
