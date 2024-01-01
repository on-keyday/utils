/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// iterator - json iterator helper
#pragma once

namespace futils {
    namespace json {
        namespace internal {

            template <class JSON>
            struct ObjIter {
                JSON& js;
                auto begin() {
                    return js.obegin();
                }
                auto end() {
                    return js.oend();
                }

                auto begin() const {
                    return js.obegin();
                }
                auto end() const {
                    return js.oend();
                }
            };

            template <class JSON>
            struct ArrIter {
                JSON& js;
                auto begin() {
                    return js.abegin();
                }
                auto end() {
                    return js.aend();
                }

                auto begin() const {
                    return js.abegin();
                }
                auto end() const {
                    return js.aend();
                }
            };
        }  // namespace internal

        template <class JSON>
        auto as_object(JSON&& json) {
            return internal::ObjIter<JSON>{json};
        }

        template <class JSON>
        auto as_array(JSON&& json) {
            return internal::ArrIter<JSON>{json};
        }
    }  // namespace json
}  // namespace futils
