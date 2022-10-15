/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// vec - vector
#pragma once
#include "alloc.h"
#include <utility>
#include <cassert>

namespace utils {
    namespace quic {
        namespace mem {

            template <class T>
            struct Vec {
               private:
                allocate::Alloc* a = nullptr;
                T* vec = nullptr;
                tsize len = 0;
                tsize cap = 0;

                bool recap(tsize new_cap) {
                    if (!a) {
                        return false;
                    }
                    auto new_vec = a->resize_vec(vec, cap, new_cap);
                    if (!new_vec) {
                        return false;
                    }
                    vec = new_vec;
                    cap = new_cap;
                    return true;
                }

                void free_vec() {
                    if (!a || !vec) {
                        return;
                    }
                    a->deallocate_vec(vec, cap);
                    vec = nullptr;
                    len = 0;
                    cap = 0;
                    a = nullptr;
                }

               public:
                constexpr Vec() = default;
                constexpr Vec(allocate::Alloc* a)
                    : a(a) {}
                constexpr Vec(Vec&& v)
                    : a(std::exchange(v.a, nullptr)),
                      vec(std::exchange(v.vec, nullptr)),
                      len(std::exchange(v.len, 0)),
                      cap(std::exchange(v.cap, 0)) {}

                tsize size() const {
                    return len;
                }

                T& operator[](tsize i) {
                    return vec[i];
                }

                T* begin() {
                    return vec;
                }

                T* end() {
                    return vec + len;
                }

                Vec& operator=(Vec&& v) noexcept {
                    if (this == &v) {
                        return *this;
                    }
                    free_vec();
                    a = std::exchange(v.a, nullptr);
                    vec = std::exchange(v.vec, nullptr);
                    len = std::exchange(v.len, 0);
                    cap = std::exchange(v.cap, 0);
                    return *this;
                }

                bool will_append(tsize reserve) {
                    if (len + reserve < cap) {
                        return true;
                    }
                    return recap(len + reserve + 3);
                }

                tsize append(T&& t) {
                    if (!will_append(1)) {
                        return false;
                    }
                    assert(vec);
                    vec[len] = std::move(t);
                    len++;
                    return 1;
                }

                tsize append(const T& t) {
                    if (!will_append(1)) {
                        return false;
                    }
                    assert(vec);
                    vec[len] = t;
                    len++;
                    return 1;
                }

                bool remove_index(tsize i) {
                    if (i >= len) {
                        return false;
                    }
                    auto _ = std::move(vec[i]);
                    for (tsize l = i; l < len; l++) {
                        vec[i] = std::move(vec[i + 1]);
                    }
                    vec[len - 1].~T();
                    len--;
                    return true;
                }

                ~Vec() {
                    free_vec();
                }
            };

            template <class T>
            tsize append_in(Vec<T>& v) {
                return 0;
            }

            template <class T, class V, class... Args>
            tsize append_in(Vec<T>& v, V&& t, Args&&... args) {
                return v.append(std::forward<V>(t)) + append(v, std::forward<Args>(args)...);
            }

            template <class T, class... Args>
            tsize append(Vec<T>& v, Args&&... args) {
                if (!v.will_append(sizeof...(Args))) {
                    return 0;
                }
                return append_in(v, std::forward<Args>(args)...);
            }
        }  // namespace mem
    }      // namespace quic
}  // namespace utils
