/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "common.h"
#include "qpack_header.h"

namespace utils {
    namespace qpack::fields {
        constexpr std::uint64_t max_entries(std::uint64_t max_capacity) {
            // floor(max_capacity / 5)
            return max_capacity >> 5;
        }

        constexpr std::uint64_t encode_required_insert_count(std::uint64_t required_insert_count, std::uint64_t max_capacity) {
            if (required_insert_count == 0) {
                return 0;
            }
            return required_insert_count % (max_entries(max_capacity) << 1) + 1;
        }

        struct SectionPrefix {
            std::uint64_t required_insert_count = 0;
            std::uint64_t base = 0;
        };

        struct EncodedSectionPrefix {
            std::uint64_t encoded_insert_count = 0;
            bool sign = false;
            std::uint64_t delta_base = 0;
            template <class C, class U>
            constexpr QpackError parse(io::basic_reader<C, U>& r) {
                if (auto err = hpack::decode_integer<8>(r, encoded_insert_count); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
                }
                byte first_bit = 0;
                if (auto err = hpack::decode_integer<7>(r, delta_base, &first_bit)) {
                    return internal::convert_hpack_error(err);
                }
                sign = first_bit & 0x80;
                return QpackError::none;
            }

           private:
            constexpr QpackError decode_required_insert_count(std::uint64_t& required_insert_count, std::uint64_t total_number_of_inserts, std::uint64_t max_capacity) const {
                auto max_entries_ = max_entries(max_capacity);
                auto fullrange = max_entries_ << 1;
                if (encoded_insert_count == 0) {
                    required_insert_count = 0;
                    return QpackError::none;
                }
                if (encoded_insert_count > fullrange) {
                    return QpackError::invalid_required_insert_count;
                }
                auto max_value = total_number_of_inserts + max_entries_;
                auto max_wrapped = (max_value / fullrange) * fullrange;
                required_insert_count = max_wrapped + encoded_insert_count - 1;
                if (required_insert_count > max_value) {
                    if (required_insert_count <= fullrange) {
                        return QpackError::invalid_required_insert_count;
                    }
                    required_insert_count -= fullrange;
                }
                return QpackError::none;
            }

           public:
            constexpr QpackError decode(SectionPrefix& prefix, std::uint64_t total_number_of_inserts, std::uint64_t max_capacity) const {
                auto err = decode_required_insert_count(prefix.required_insert_count, total_number_of_inserts, max_capacity);
                if (err != QpackError::none) {
                    return err;
                }
                if (!sign) {
                    prefix.base = prefix.required_insert_count + delta_base;
                }
                else {
                    if (prefix.required_insert_count > delta_base + 1) {
                        return QpackError::negative_base;
                    }
                    prefix.base = prefix.required_insert_count - delta_base - 1;
                }
                return QpackError::none;
            }
        };

        enum class FieldType {
            undefined,
            index,
            post_base_index,
            literal_with_name_ref,
            literal_with_post_base_name_ref,
            literal_with_literal_name,
        };

        constexpr byte mask(FieldType ist) {
            switch (ist) {
                case FieldType::index:
                    return 0x80;
                case FieldType::post_base_index:
                case FieldType::literal_with_post_base_name_ref:
                    return 0xF0;
                case FieldType::literal_with_name_ref:
                    return 0xC0;
                case FieldType::literal_with_literal_name:
                    return 0xE0;
                default:
                    return 0;
            }
        }

        constexpr byte match(FieldType ist) {
            switch (ist) {
                case FieldType::index:
                    return 0x80;
                case FieldType::post_base_index:
                    return 0x10;
                case FieldType::literal_with_name_ref:
                    return 0x40;
                case FieldType::literal_with_post_base_name_ref:
                    return 0x00;
                case FieldType::literal_with_literal_name:
                    return 0x20;
                default:
                    return 0xff;
            }
        }

        constexpr FieldType get_type(byte m) {
            auto is = [&](FieldType ist) {
                return (mask(ist) & m) == match(ist);
            };
            if (is(FieldType::index)) {
                return FieldType::index;
            }
            else if (is(FieldType::literal_with_name_ref)) {
                return FieldType::literal_with_name_ref;
            }
            else if (is(FieldType::literal_with_literal_name)) {
                return FieldType::literal_with_literal_name;
            }
            else if (is(FieldType::post_base_index)) {
                return FieldType::post_base_index;
            }
            else if (is(FieldType::literal_with_post_base_name_ref)) {
                return FieldType::literal_with_post_base_name_ref;
            }
            return FieldType::undefined;
        }

        template <class T>
        constexpr QpackError render_field_line_static(io::expand_writer<T>& w, Field field) {
            auto& tb = header::internal::field_hash_table.table[header::internal::field_hash_index(field.head, field.value)];
            for (auto& e : tb) {
                if (e == 0) {
                    break;
                }
                auto index = e - 1;
                auto kv = header::predefined_headers<>.table[index];
                if (kv.first == field.head && kv.second == field.value) {
                    if (auto err = hpack::encode_integer<6>(w, index, match(FieldType::index) | 0x40); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return QpackError::none;
                }
            }
            auto& hd = header::internal::header_hash_table.table[header::internal::header_hash_index(field.head)];
            for (auto& e : hd) {
                if (e == 0) {
                    break;
                }
                auto index = e - 1;
                auto kv = header::predefined_headers<>.table[index];
                if (kv.first == field.head) {
                    if (auto err = hpack::encode_integer<4>(w, index, match(FieldType::literal_with_name_ref) | 0x20); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    if (auto err = hpack::encode_string(w, field.value); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return QpackError::none;
                }
            }
            return QpackError::no_entry_exists;
        }

        template <class T>
        constexpr QpackError render_field_line_literal(io::expand_writer<T>& w, Field field) {
            if (auto err = hpack::encode_string<3>(w, field.head, match(FieldType::literal_with_literal_name), 0x08); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            if (auto err = hpack::encode_string(w, field.value); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        // returns (error,used_reference)
        template <class TypeConfig, class T>
        constexpr std::pair<QpackError, std::uint64_t> render_field_line_dynamic(const fields::FieldEncodeContext<TypeConfig>& ctx, const SectionPrefix& prefix, io::expand_writer<T>& w, Field field) {
            Reference ref = ctx.find_ref(field.head, field.value);
            if (ref.head_value != no_ref) {
                auto src_index = ref.head_value;
                // large............small
                // |n-1|n-2|n-3|...|d
                //         |0  |...|n-d-3
                if (src_index <= prefix.base) {
                    auto index = prefix.base - src_index;
                    if (auto err = hpack::encode_integer<6>(w, index, match(FieldType::index)); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                }
                // large............small
                // |n-1|n-2|n-3|...|d
                // |1  |0  |
                else {
                    auto index = src_index - prefix.base - 1;
                    if (auto err = hpack::encode_integer<4>(w, index, match(FieldType::post_base_index)); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                }
                return {QpackError::none, ref.head_value};
            }
            if (ref.head != no_ref) {
                auto src_index = ref.head;
                // large............small
                // |n-1|n-2|n-3|...|d
                //         |0  |...|n-d-3
                if (src_index <= prefix.base) {
                    auto index = prefix.base - src_index;
                    if (auto err = hpack::encode_integer<4>(w, index, match(FieldType::literal_with_name_ref)); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                    if (auto err = hpack::encode_string(w, field.value); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                }
                // large............small
                // |n-1|n-2|n-3|...|d
                // |1  |0  |
                else {
                    auto index = src_index - prefix.base;
                    if (auto err = hpack::encode_integer<3>(w, index, match(FieldType::literal_with_post_base_name_ref)); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                    if (auto err = hpack::encode_string(w, field.value); err != hpack::HpackError::none) {
                        return {internal::convert_hpack_error(err), no_ref};
                    }
                }
                return {QpackError::none, ref.head};
            }
            return {QpackError::no_entry_exists, no_ref};
        }

        enum class FieldMode {
            static_only,
            dynamic_insert,
            dynamic_no_insert,
            dynamic_no_drop,
        };

        template <class TypeConfig, class T>
        constexpr QpackError render_field_line(StreamID id, fields::FieldEncodeContext<TypeConfig>& ctx, const SectionPrefix& prefix, io::expand_writer<T>& w, Field field, FieldMode mode) {
            auto err = render_field_line_static(w, field);
            if (err == QpackError::none) {
                return QpackError::none;
            }
            if (err != QpackError::no_entry_exists) {
                return err;
            }
            if (mode == FieldMode::static_only) {
                return render_field_line_literal(w, field);
            }
            auto [err2, index] = render_field_line_dynamic(ctx, prefix, w, field);
            if (err == QpackError::none) {
                return ctx.add_ref(index, id);
            }
            if (err != QpackError::no_entry_exists) {
                return err;
            }
            if (mode == FieldMode::dynamic_no_insert) {
                return render_field_line_literal(w, field);
            }
            index = ctx.add_field(field.head, field.value);
            if (index == no_ref) {
                if (mode != FieldMode::dynamic_no_drop) {
                    if (auto err = ctx.drop_field(); err != QpackError::dynamic_ref_exists) {
                        index = ctx.add_field(field.head, field.value);
                    }
                }
                if (index == no_ref) {
                    return render_field_line_literal(w, field);
                }
            }
            std::tie(err, index) = render_field_line_dynamic(ctx, prefix, w, field);
            if (err != QpackError::none) {
                return err
            }
            return ctx.add_ref(index, id);
        }

        template <class TypeConfig, class C, class U, class Field>
        constexpr QpackError parse_field_line(FieldDecodeContext<TypeConfig>& ctx, SectionPrefix& prefix, io::basic_reader<C, U>& r, Field& field) {
            if (r.empty()) {
                return QpackError::input_length;
            }
            const FieldType type = get_type(r.top());
            auto fetch_index_field = [&](std::uint64_t index, bool is_static, bool only_name) {
                if (is_static) {
                    if (index >= header::predefined_header_count) {
                        return QpackError::invalid_static_table_ref;
                    }
                    auto got = header::predefined_headers<byte>.table[index];
                    field.set_header(view::rvec(got.first));
                    if (!only_name) {
                        field.set_value(view::rvec(got.second));
                    }
                }
                else {
                    if (prefix.base > index + 1) {
                        return QpackError::invalid_index;
                    }
                    auto h = ctx.find_ref(prefix.base - 1 - index);
                    if (!h) {
                        return QpackError::no_entry_for_index;
                    }
                    field.set_header(h->first);
                    if (!only_name) {
                        field.set_value(h->second);
                    }
                }
                return QpackError::none;
            };
            auto fetch_post_index_field = [&](std::uint64_t index, bool only_header) {
                auto h = ctx.find_ref(prefix.base + index);
                if (!h) {
                    return QpackError::no_entry_for_index;
                }
                field.set_header(h->first);
                if (!only_header) {
                    field.set_value(h->second);
                }
                return QpackError::none;
            };
            switch (type) {
                default: {
                    return QpackError::undefined_instruction;
                }
                case FieldType::index: {
                    bool is_static = false;
                    byte mask = 0;
                    std::uint64_t index = 0;
                    if (auto err = hpack::decode_integer<6>(r, index, &mask); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    is_static = mask & 0x40;
                    return fetch_index_field(index, is_static, false);
                }
                case FieldType::post_base_index: {
                    std::uint64_t index = 0;
                    if (auto err = hpack::decode_integer<4>(r, index); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    return fetch_post_index_field(index, false);
                }
                case FieldType::literal_with_name_ref: {
                    bool is_static = false;
                    byte mask = 0;
                    std::uint64_t index = 0;
                    if (auto err = hpack::decode_integer<4>(r, index, &mask); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    is_static = mask & 0x20;
                    if (auto err = fetch_index_field(index, is_static, true); err != qpack::QpackError::none) {
                        return err;
                    }
                    auto value = field.get_string();
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    field.set_value(std::move(value));
                    return QpackError::none;
                }
                case FieldType::literal_with_post_base_name_ref: {
                    byte mask = 0;
                    std::uint64_t index = 0;
                    if (auto err = hpack::decode_integer<3>(r, index, &mask); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    (void)mask;
                    if (auto err = fetch_post_index_field(index, true); err != qpack::QpackError::none) {
                        return err;
                    }
                    auto value = field.get_string();
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    field.set_value(std::move(value));
                    return QpackError::none;
                }
                case FieldType::literal_with_literal_name: {
                    byte mask = 0;
                    std::uint64_t index = 0;
                    auto head = field.get_string();
                    if (auto err = hpack::decode_string<3>(head, r, 0x08, &mask); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    (void)mask;
                    auto value = field.get_string();
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    field.set_value(std::move(head));
                    field.set_value(std::move(value));
                    return QpackError::none;
                }
            }
        }

    }  // namespace qpack::fields
}  // namespace utils
