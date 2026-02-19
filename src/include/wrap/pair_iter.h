/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pair_iter - pair to iterator wrapper
#pragma once
#include <iterator>

namespace futils {
    namespace wrap {
        template <class Wrap>
        struct PairIter {
            Wrap it;

            auto begin() {
                return get<0>(it);
            }

            auto end() {
                return get<1>(it);
            }

            bool empty() {
                return begin() == end();
            }
        };

        template <class T>
        PairIter<T&> iter(T& t) {
            return PairIter<T&>{t};
        }

        template <class T>
        PairIter<T> iter(T&& t) {
            return PairIter<T>{t};
        }

        template <class It, class Unwraper>
        struct WrapIterator {
            It it_;
            Unwraper unwrap;

            WrapIterator() = default;
            WrapIterator(It it, Unwraper uw) noexcept
                : it_(it), unwrap(uw) {}
            WrapIterator(const WrapIterator&) = default;
            WrapIterator(WrapIterator&&) = default;
            WrapIterator& operator=(const WrapIterator&) = default;
            WrapIterator& operator=(WrapIterator&&) = default;

            decltype(auto) operator*() noexcept {
                return unwrap.unwrap(it_);
            }

            decltype(auto) operator*() const noexcept {
                return unwrap.const_unwrap(it_);
            }

            template <class OIt, class OUw>
            bool operator==(const WrapIterator<OIt, OUw>& it) const {
                return unwrap.equal(it_, it.it_);
            }

            WrapIterator& operator++() noexcept {
                unwrap.increment(this->it_);
                return *this;
            }

            WrapIterator& operator--() noexcept {
                unwrap.decrement(this->it_);
                return *this;
            }
        };

        struct DefaultUnwrap {
            decltype(auto) unwrap(auto& v) noexcept {
                return *v;
            }

            auto const_unwrap(auto& v) const noexcept {
                return *v;
            }

            void increment(auto& v) {
                ++v;
            }

            void decrement(auto v) {
                --v;
            }

            bool equal(auto& a, auto& b) {
                return a == b;
            }
        };

        struct NothingUnwrap {
            decltype(auto) unwrap(auto& v) noexcept {
                assert(false);
            }

            auto const_unwrap(auto& v) const noexcept {
                assert(false);
            }

            void increment(auto& v) {
                assert(false);
            }

            void decrement(auto v) {
                assert(false);
            }

            bool equal(auto& a, auto& b) {
                assert(false);
                return false;
            }
        };

        template <class T, class TUw, class U, class UUw>
        struct WrapRange {
            WrapIterator<T, TUw> f;
            WrapIterator<U, UUw> e;
            auto begin() {
                return f;
            }

            auto end() {
                return e;
            }
        };

        template <class It, class Unwrap = DefaultUnwrap>
        auto make_wrapiter(It it, Unwrap unwrap = DefaultUnwrap{}) {
            return WrapIterator<It, Unwrap>{it, unwrap};
        }

        template <class It1, class Wrap1, class It2, class Wrap2>
        auto make_wraprange(It1 it1, Wrap1 wrap1, It2 it2, Wrap2 wrap2) {
            return WrapRange<It1, Wrap1, It2, Wrap2>{make_wrapiter(it1, wrap1), make_wrapiter(it2, wrap2)};
        }
    }  // namespace wrap
}  // namespace futils
