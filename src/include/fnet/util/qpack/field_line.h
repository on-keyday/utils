/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "common.h"
#include "qpack_header.h"

namespace futils {
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

            constexpr std::uint64_t encode_delta_base() const {
                if (required_insert_count <= base) {
                    return base - required_insert_count;
                }
                else {
                    return required_insert_count - 1 - base;
                }
            }

            constexpr bool sign() const {
                return required_insert_count > base;
            }
        };

        struct EncodedSectionPrefix {
            std::uint64_t encoded_insert_count = 0;
            bool sign = false;
            std::uint64_t delta_base = 0;
            template <class C>
            constexpr QpackError parse(binary::basic_reader<C>& r) {
                if (auto err = hpack::decode_integer<8>(r, encoded_insert_count); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
                }
                byte first_bit = 0;
                if (auto err = hpack::decode_integer<7>(r, delta_base, &first_bit); err != hpack::HpackError::none) {
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

            constexpr void encode(SectionPrefix prefix, std::uint64_t max_capacity) {
                encoded_insert_count = encode_required_insert_count(prefix.required_insert_count, max_capacity);
                sign = prefix.sign();
                delta_base = prefix.encode_delta_base();
            }

            constexpr QpackError render(binary::writer& w) const {
                if (auto err = hpack::encode_integer<8>(w, encoded_insert_count, 0); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
                }
                if (auto err = hpack::encode_integer<7>(w, delta_base, sign ? 0x80 : 0); err != hpack::HpackError::none) {
                    return internal::convert_hpack_error(err);
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

        constexpr QpackError render_field_line_index(binary::writer& w, std::uint64_t index, bool is_static) {
            if (auto err = hpack::encode_integer<6>(w, index, match(FieldType::index) | (is_static ? 0x40 : 0)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        constexpr QpackError render_field_line_post_base_index(binary::writer& w, std::uint64_t index) {
            if (auto err = hpack::encode_integer<4>(w, index, match(FieldType::post_base_index)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        constexpr QpackError render_field_line_index_literal(binary::writer& w, std::uint64_t index, bool is_static, auto&& value, bool must_forward_as_literal = false) {
            if (auto err = hpack::encode_integer<4>(w, index, match(FieldType::literal_with_name_ref) | (is_static ? 0x10 : 0) | (must_forward_as_literal ? 0x20 : 0)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            if (auto err = hpack::encode_string(w, value); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        constexpr QpackError render_field_line_post_base_index_literal(binary::writer& w, std::uint64_t index, auto&& value, bool must_forward_as_literal = false) {
            if (auto err = hpack::encode_integer<3>(w, index, match(FieldType::literal_with_post_base_name_ref) | (must_forward_as_literal ? 0x08 : 0)); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            if (auto err = hpack::encode_string(w, value); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        constexpr QpackError render_field_line_literal(binary::writer& w, auto&& head, auto&& value, bool must_forward_as_literal = false) {
            if (auto err = hpack::encode_string<3>(w, head, match(FieldType::literal_with_literal_name) | (must_forward_as_literal ? 0x10 : 0), 0x08);
                err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            if (auto err = hpack::encode_string(w, value); err != hpack::HpackError::none) {
                return internal::convert_hpack_error(err);
            }
            return QpackError::none;
        }

        constexpr QpackError render_field_line_dynamic_index(const SectionPrefix& prefix, binary::writer& w, std::uint64_t abs_index) {
            auto src_index = abs_index;
            // large............small
            // |n-1|n-2|n-3|...|d
            //         |0  |...|n-d-3
            if (src_index <= prefix.base) {
                auto index = prefix.base - src_index;
                return render_field_line_index(w, index, false);
            }
            // large............small
            // |n-1|n-2|n-3|...|d
            // |1  |0  |
            else {
                auto index = src_index - prefix.base - 1;
                return render_field_line_post_base_index(w, index);
            }
        }

        constexpr QpackError render_field_line_dynamic_index_literal(const SectionPrefix& prefix, binary::writer& w, std::uint64_t abs_index, auto&& value, bool must_forward_as_literal = false) {
            auto src_index = abs_index;
            // large............small
            // |n-1|n-2|n-3|...|d
            //         |0  |...|n-d-3
            if (src_index <= prefix.base) {
                auto index = prefix.base - src_index;
                return render_field_line_index_literal(w, index, false, value, must_forward_as_literal);
            }
            // large............small
            // |n-1|n-2|n-3|...|d
            // |1  |0  |
            else {
                auto index = src_index - prefix.base - 1;
                return render_field_line_post_base_index_literal(w, index, value, must_forward_as_literal);
            }
        }

        // field has member functions
        // + field.set_value(string)
        // + field.set_header(string)
        // + field.set_forward_mobe(bool as_literal)
        template <class TypeConfig, class C, class Field>
        constexpr QpackError parse_field_line(FieldDecodeContext<TypeConfig>& ctx, SectionPrefix& prefix, binary::basic_reader<C>& r, Field& field) {
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
                    if (prefix.base < index) {
                        return QpackError::invalid_index;
                    }
                    auto h = ctx.find_ref(prefix.base - index);
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
                auto h = ctx.find_ref(prefix.base + 1 + index);
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
                    is_static = mask & 0x10;
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
                    field.set_forward_mode(bool(mask & 0x08));
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
                    field.set_forward_mode(bool(mask & 0x10));
                    auto value = field.get_string();
                    if (auto err = hpack::decode_string(value, r); err != hpack::HpackError::none) {
                        return internal::convert_hpack_error(err);
                    }
                    field.set_header(std::move(head));
                    field.set_value(std::move(value));
                    return QpackError::none;
                }
            }
        }

    }  // namespace qpack::fields
}  // namespace futils
