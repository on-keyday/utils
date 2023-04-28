/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <utility>
#include "../hpack/hpack_encode.h"

namespace utils {
    namespace qpack {
        enum class QpackError {
            none,
            input_length,
            undefined_instruction,
            encoding_error,
            unexpected_error,
            large_capacity,
            invalid_static_table_ref,
            invalid_dynamic_table_relative_ref,
            no_entry_for_index,
        };

        template <class StringT, class Deque>
        struct DequeTable {
            Deque deq;

            void insert(StringT&& header, StringT&& value) {
                deq.push_back({std::move(header), std::move(value)});
            }
        };

        struct InsertDropManager {
           private:
            std::uint64_t inserted = 0;
            std::uint64_t dropped = 0;

           public:
            constexpr std::uint64_t get_relative_range() const {
                return inserted - dropped;
            }

            constexpr bool is_valid_relatvie_index(std::uint64_t index) const {
                const auto range_max = get_relative_range();
                return index < range_max;
            }

            constexpr std::pair<std::uint64_t, bool> abs_to_rel(std::uint64_t index) const {
                if (index < dropped || inserted <= index) {
                    return {-1, false};
                }
                return {inserted - 1 - index, true};
            }

            constexpr void insert(size_t in = 1) {
                inserted += in;
            }
        };

        struct Field {
            view::rvec head;
            view::rvec value;
        };

        template <class Table>
        struct Context {
            std::uint64_t max_capacity = 0;
            std::uint64_t capacity = 0;
            InsertDropManager isdr;

            Table table;

            void insert(auto&& h, auto&& v) {
                table.insert(std::move(h), std::move(v));
                isdr.insert();
            }

            std::int64_t get_duplicated(Field field) {
                return table.get_duplicated(field);
            }

            std::int64_t get_index(view::rvec head) {
                return table.get_index(head);
            }
        };

        namespace internal {
            QpackError convert_hpack_error(hpack::HpackError err) {
                switch (err) {
                    case hpack::HpackError::input_length:
                        return QpackError::input_length;
                    case hpack::HpackError::too_large_number:
                    case hpack::HpackError::too_short_number:
                        return QpackError::encoding_error;
                    default:
                        return QpackError::unexpected_error;
                }
            }
        }  // namespace internal

    }  // namespace qpack
}  // namespace utils
