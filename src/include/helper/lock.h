/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "defer.h"
#include <tuple>

namespace futils {
    namespace helper {

        [[nodiscard]] constexpr auto lock(auto& l) {
            l.lock();
            return defer([&] {
                l.unlock();
            });
        }

        namespace internal {
            template <size_t idx, size_t term, class... TupT>
            constexpr bool do_try_lock(std::tuple<TupT...>& t) {
                constexpr auto size = std::tuple_size_v<std::decay_t<decltype(t)>>;
                if constexpr (idx % size == term) {
                    return true;
                }
                else {
                    if (!get<idx % size>(t).try_lock()) {
                        return false;
                    }
                    auto d = defer([&] { get<idx % size>(t).unlock(); });
                    if (!do_try_lock<idx + 1, term>(t)) {
                        return false;
                    }
                    d.cancel();
                    return true;
                }
            }

            template <size_t start, class... TupT>
            constexpr bool do_multi_lock_from_index(std::tuple<TupT...>& t) {
                get<start>(t).lock();
                auto d = defer([&] { get<start>(t).unlock(); });
                if (!do_try_lock<start + 1, start>(t)) {
                    return false;
                }
                d.cancel();
                return true;
            }

            template <size_t... idx, class... TupT>
            constexpr void do_multi_lock(std::tuple<TupT...>& t, std::index_sequence<idx...>) {
                while (!(... || do_multi_lock_from_index<idx>(t))) {
                }
            }

        }  // namespace internal

        [[nodiscard]] constexpr auto lock(auto& l1, auto& l2, auto&... l3) {
            auto tup = std::forward_as_tuple(l1, l2, l3...);
            internal::do_multi_lock(tup, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(tup)>>>{});
            return defer([tup] {
                std::apply(
                    [&](auto&&... u) {
                        (..., u.unlock());
                    },
                    tup);
            });
        }

        namespace test {
            struct Lock {
                size_t base = 0;
                size_t* cur = nullptr;

                constexpr void lock() {
                    *cur = base + 1;
                }

                constexpr bool try_lock() {
                    if (*cur != base) {
                        return false;
                    }
                    *cur = base + 1;
                    return true;
                }

                constexpr void unlock() {}
            };

            constexpr auto check_lock() {
                constexpr auto n_lock = 5;
                constexpr auto shift = 3;
                Lock locks[n_lock];
                size_t cur = 0;
                for (size_t i = 0; i < n_lock; i++) {
                    locks[i].cur = &cur;
                    locks[i].base = ((i + shift) % n_lock);
                }
                auto l = lock(locks[0], locks[1], locks[2], locks[3], locks[4]);
                return true;
            }

            static_assert(check_lock());
        }  // namespace test
    }      // namespace helper
}  // namespace futils
