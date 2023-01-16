/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// sort_internal - internal sort and en-tuple
#pragma once
#include <utility>
#include <type_traits>
#include <array>
#include <algorithm>
#include "../../helper/equal.h"
#include "../../core/strlen.h"

namespace utils {
    namespace minilang::token {
        namespace internal {
            constexpr auto check_length(auto&& src) {
                for (auto symbol : src) {
                    if (symbol == nullptr) {
                        return false;
                    }
                }
                return true;
            }

            template <size_t len>
            constexpr auto sort_map(auto src_mapping) {
                std::array<const char*, len> mp;
                for (size_t i = 0; i < mp.size(); i++) {
                    mp[i] = src_mapping[i];
                }
                std::sort(mp.begin(), mp.end(), [](auto a, auto b) {
                    return utils::strlen(a) > utils::strlen(b);
                });
                return mp;
            }

            template <size_t len>
            using IdxSeq = std::make_index_sequence<len>;

            template <size_t i>
            constexpr auto to_tuple(auto&& mp) {
                return std::make_tuple(mp[i]);
            }

            template <class Kind, size_t len>
            constexpr Kind find_index(auto src_mapping, auto str) {
                for (auto i = 0; i < len; i++) {
                    if (helper::equal(src_mapping[i], str)) {
                        return Kind(i);
                    }
                }
                return Kind(len);
            }

            template <class Kind, size_t... index>
            constexpr auto apply_impl(auto&& cb, auto&& src, auto&& mp, std::index_sequence<index...>) {
                auto fold = [&](auto str) {
                    return std::make_pair(str, find_index<Kind, sizeof...(index)>(src, str));
                };
                return cb(fold(std::get<index>(mp))...);
            }

            template <class Kind, size_t len>
            constexpr auto apply(auto&& cb, auto&& src) {
                auto sorted = internal::sort_map<len>(src);
                return internal::apply_impl<Kind>(cb, src, sorted, internal::IdxSeq<len>{});
            }
        }  // namespace internal

        constexpr auto one_of_kind(auto&&... value) {
            return [=](auto&& cmp) {
                return (... || (cmp == value));
            };
        }

        constexpr auto not_of_kind(auto&&... value) {
            return [=](auto&& cmp) {
                return (... && (cmp != value));
            };
        }
    }  // namespace minilang::token
}  // namespace utils
