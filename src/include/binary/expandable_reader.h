/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <binary/reader.h>

namespace futils {
    namespace binary {
        template <class T, class V>
        concept has_append = requires(T t, V v) {
            t.append(v);
        };

        template <class T>
        concept has_input = requires(T t) {
            t.input(size_t{});
        };

        template <class T, class V>
        concept has_push_back = requires(T t, V v) {
            t.push_back(v);
        };

        template <class T>
        concept has_bool = requires(T t) {
            { t } -> std::convertible_to<bool>;
        };

        template <class T>
        concept has_erase = requires(T t) {
            { t.erase(size_t(), size_t()) };
        };

        template <class T>
        concept has_shift_front = requires(T t) {
            { t.shift_front(size_t()) };
        };

        template <class T>
        concept has_discard = requires(T t) {
            { t.discard(size_t()) };
        };

        template <class T, class C>
        struct basic_expand_reader {
           private:
            T buf;
            basic_reader<C> r;

           public:
            constexpr basic_expand_reader()
                : r(buf) {}

            constexpr basic_expand_reader(auto&& value)
                : buf(std::forward<decltype(value)>(value)), r(buf) {}

            constexpr const basic_reader<C>& reader() const {
                return r;
            }

            constexpr bool append(auto&& a) {
                if constexpr (has_append<decltype(buf), decltype(a)>) {
                    if constexpr (has_bool<decltype(buf.append(a))>) {
                        return buf.append(a);
                    }
                    else {
                        buf.append(a);
                        return true;
                    }
                }
                return false;
            }

            constexpr bool push_back(C c) {
                if constexpr (has_append<decltype(buf), C>) {
                    if constexpr (has_bool<decltype(buf.push_back(c))>) {
                        return buf.push_back(c);
                    }
                    else {
                        buf.push_back(c);
                        return true;
                    }
                }
                return false;
            }

            constexpr view::basic_rvec<C> read() {
                return r.read();
            }

            constexpr bool input(size_t expect) {
                if constexpr (has_input<decltype(buf)>) {
                    if constexpr (has_bool<decltype(buf.input(expect))>) {
                        if (!buf.input(expect)) {
                            return false;
                        }
                    }
                    else {
                        buf.input(expect);
                    }
                    r.reset(buf, r.offset());
                    return true;
                }
                return false;
            }

            constexpr bool check_input(size_t n) {
                if (r.remain().size() < n) {
                    if (!input(n - r.remain().size())) {
                        return false;
                    }
                    return r.remain().size() >= n;
                }
                return true;
            }

            constexpr view::basic_rvec<C> read_best(size_t n) {
                check_input(n);
                return r.read_best(n);
            }

            constexpr bool read(view::basic_rvec<C>& data, size_t n) {
                if (!check_input(n)) {
                    return false;
                }
                return r.read(data, n);
            }

            constexpr std::pair<view::basic_rvec<C>, bool> read(size_t n) {
                if (!check_input(n)) {
                    return {{}, false};
                }
                return r.read(n);
            }

            constexpr bool read(view::basic_wvec<C> data) {
                if (!check_input(data.size())) {
                    return false;
                }
                return r.read(data);
            }

            constexpr C top() const {
                return r.top();
            }

            constexpr void offset(size_t add) {
                r.offset(add);
            }

            [[nodiscard]] constexpr size_t offset() const {
                return r.offset();
            }

            constexpr view::basic_rvec<C> remain() {
                return r.remain();
            }

            constexpr void reset(size_t pos = 0) {
                r.reset(pos);
            }

            constexpr void reset(T&& b, size_t pos = 0) {
                buf = std::move(b);
                r.reset(buf, pos);
            }

            constexpr bool discard(size_t n) {
                view::basic_rvec<C> d = r.read();
                if (n > d.size()) {
                    return false;
                }
                if constexpr (has_discard<decltype(buf)>) {
                    if constexpr (has_bool<decltype(buf.discard(0, n))>) {
                        if (!buf.discard(0, n)) {
                            return false;
                        }
                    }
                    else {
                        buf.discard(0, n);
                    }
                }
                else if constexpr (has_erase<decltype(buf)>) {
                    if constexpr (has_bool<decltype(buf.erase(0, n))>) {
                        if (!buf.erase(0, n)) {
                            return false;
                        }
                    }
                    else {
                        buf.erase(0, n);
                    }
                }
                else if constexpr (has_shift_front<decltype(buf)>) {
                    if constexpr (has_bool<decltype(buf.shift_front(n))>) {
                        if (!buf.shift_front(n)) {
                            return false;
                        }
                    }
                    else {
                        buf.shift_fonrt(n);
                    }
                }
                r.reset(buf, r.offset() - n);
                return true;
            }
        };

        template <class T>
        using expand_reader = basic_expand_reader<T, byte>;
    }  // namespace binary
}  // namespace futils
