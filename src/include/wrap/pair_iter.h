/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pair_iter - pair to iterator wrapper
namespace utils {
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
        };

        template <class T>
        PairIter<T&> iter(T& t) {
            return PairIter<T&>{t};
        }

        template <class T>
        PairIter<T> iter(T&& t) {
            return PairIter<T>{t};
        }
    }  // namespace wrap
}  // namespace utils
