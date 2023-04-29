/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../hpack/hpack_encode.h"
#include "qpack_header.h"
#include "common.h"

namespace utils {
    namespace qpack::encoder {
        enum class Instruction {
            undefined,
            dyn_table_capacity,
            insert_with_name_ref,
            insert_with_literal_name,
            duplicate,
        };

        constexpr byte mask(Instruction ist) {
            switch (ist) {
                case Instruction::dyn_table_capacity:
                case Instruction::duplicate:
                    return 0xE0;
                case Instruction::insert_with_name_ref:
                    return 0x80;
                case Instruction::insert_with_literal_name:
                    return 0xC0;
                default:
                    return 0;
            }
        }

        constexpr byte match(Instruction ist) {
            switch (ist) {
                case Instruction::dyn_table_capacity:
                    return 0x20;
                case Instruction::duplicate:
                    return 0x00;
                case Instruction::insert_with_name_ref:
                    return 0x80;
                case Instruction::insert_with_literal_name:
                    return 0x40;
                default:
                    return 0xFF;
            }
        }

        constexpr Instruction get_istr(byte m) {
            auto is = [&](Instruction ist) {
                return (mask(ist) & m) == match(ist);
            };
            if (is(Instruction::insert_with_name_ref)) {
                return Instruction::insert_with_name_ref;
            }
            else if (is(Instruction::insert_with_literal_name)) {
                return Instruction::insert_with_literal_name;
            }
            else if (is(Instruction::dyn_table_capacity)) {
                return Instruction::dyn_table_capacity;
            }
            else if (is(Instruction::duplicate)) {
                return Instruction::duplicate;
            }
            return Instruction::undefined;
        }

        struct CapacityUpdate {
            std::uint64_t capacity = 0;
        };

        template <class T>
        constexpr QpackError render_capacity(io::expand_writer<T>& w, std::uint64_t capacity) {
            if (auto err = hpack::encode_integer<5>(w, capacity, match(Instruction::dyn_table_capacity)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        template <class T>
        constexpr QpackError render_duplicate(io::expand_writer<T>& w, std::uint64_t index) {
            if (auto err = hpack::encode_integer<5>(w, index, match(Instruction::duplicate)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        template <class T, class Table>
        constexpr QpackError render_insertion(Context<Table>& table, io::expand_writer<T>& w, Field& ins) {
            auto dup = table.get_duplicated(ins);
            if (dup >= 0) {
                return render_duplicate(w, dup);
            }
            auto& refs = header::internal::header_hash_table.table[header::internal::header_hash_index(ins.head)];
            std::int64_t name_ref = -1;
            byte is_static = 0;
            for (auto ref : refs) {
                if (ref == 0) {
                    break;
                }
                auto& field = header::predefined_headers<char>.table[ref - 1];
                if (ins.head == field.first) {
                    name_ref = ref - 1;
                    is_static = 0x40;
                    break;
                }
            }
            if (name_ref == -1) {
                name_ref = table.get_index(ins.head);
            }
            if (name_ref >= 0) {
                if (auto err = hpack::encode_integer<6>(w, name_ref, match(Instruction::insert_with_name_ref) | is_static); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
                }
            }
            else {
                if (auto err = hpack::encode_string<5>(w, ins.head, match(Instruction::insert_with_literal_name), 0x20); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
                }
            }
            if (auto err = hpack::encode_string<7>(w, ins.value); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        template <class StringT, class Table, class C, class U>
        constexpr QpackError parse_instruction(Context<Table>& table, io::basic_reader<C, U>& r) {
            if (r.empty()) {
                return QpackError::input_length;
            }
            using field_t = std::pair<view::basic_rvec<C, U>, view::basic_rvec<C, U>>;
            switch (get_istr(r.top())) {
                default:
                    return QpackError::undefined_instruction;
                case Instruction::dyn_table_capacity: {
                    std::uint64_t capacity = 0;
                    if (auto err = hpack::decode_integer<5>(r, capacity); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    if (capacity > table.max_capacity) {
                        return QpackError::large_capacity;
                    }
                    table.capacity = capacity;
                }
                case Instruction::insert_with_name_ref: {
                    std::uint64_t name_index = 0;
                    byte mask = 0;
                    if (auto err = hpack::decode_integer<6>(r, name_index, &mask); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    const bool is_static = mask & 0x40;
                    view::basic_rvec<C, U> head;
                    if (is_static) {
                        if (name_index >= header::predefined_header_count) {
                            return QpackError::invalid_static_table_ref;
                        }
                        head = view::basic_rvec<C, U>(header::predefined_headers<C>.table[name_index].first);
                    }
                    else {
                        if (!table.isdr.is_valid_relatvie_index(name_index)) {
                            return QpackError::invalid_dynamic_table_relative_ref;
                        }
                        field_t h = table.table.get_field(name_index);
                        if (h.first.null()) {
                            return QpackError::no_entry_for_index;
                        }
                        head = h.first;
                    }
                    StringT value;
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    table.insert(StringT(head), std::move(value));
                }
                case Instruction::insert_with_literal_name: {
                    StringT head, value;
                    if (auto err = hpack::decode_string<5>(head, r, 0x20); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    table.insert(std::move(head), std::move(value));
                }
                case Instruction::duplicate: {
                    std::uint64_t index = 0;
                    if (auto err = hpack::decode_integer<5>(r, index); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    if (!table.isdr.is_valid_relatvie_index(index)) {
                        return QpackError::invalid_dynamic_table_relative_ref;
                    }
                    field_t h = table.table.get_field(index);
                    if (h.first.null()) {
                        return QpackError::no_entry_for_index;
                    }
                    table.insert(StringT(h.first), StringT(h.second));
                }
            }
            return QpackError::none;
        }

    }  // namespace qpack::encoder
}  // namespace utils