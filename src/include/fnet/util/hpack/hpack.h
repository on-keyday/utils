/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// hpack - http2 hpack
// almost copied from socklib
#pragma once

#include <utility>
#include <array>
#include <cstdint>
#include <algorithm>
#include "hpack_encode.h"
#include "hpack_header.h"
#include <fnet/http1/callback.h>
#include "hpack_internal.h"

namespace futils {
    namespace hpack {
        enum class Field {
            index,
            index_literal_insert,
            index_literal_never_indexed,
            index_literal_no_insert,
            dyn_table_update,
            undefined,
        };

        constexpr byte mask(Field f) noexcept {
            switch (f) {
                case Field::index:
                    return 0x80;
                case Field::index_literal_insert:
                    return 0xC0;
                case Field::dyn_table_update:
                    return 0xE0;
                case Field::index_literal_no_insert:
                case Field::index_literal_never_indexed:
                    return 0xF0;
                default:
                    return 0;
            }
        }

        constexpr byte match(Field f) noexcept {
            switch (f) {
                case Field::index:
                    return 0x80;
                case Field::index_literal_insert:
                    return 0x40;
                case Field::dyn_table_update:
                    return 0x20;
                case Field::index_literal_no_insert:
                    return 0x00;
                case Field::index_literal_never_indexed:
                    return 0x10;
                default:
                    return 0xff;
            }
        }

        constexpr Field get_field_type(byte m) {
            auto is = [&](Field ist) {
                return (mask(ist) & m) == match(ist);
            };
            if (is(Field::index)) {
                return Field::index;
            }
            else if (is(Field::index_literal_insert)) {
                return Field::index_literal_insert;
            }
            else if (is(Field::dyn_table_update)) {
                return Field::dyn_table_update;
            }
            else if (is(Field::index_literal_no_insert)) {
                return Field::index_literal_no_insert;
            }
            else if (is(Field::index_literal_never_indexed)) {
                return Field::index_literal_never_indexed;
            }
            return Field::undefined;
        }

        template <class F, class Table>
        constexpr bool get_table_index(F&& f, size_t& index, Table& dymap) {
            if (auto found = std::find_if(predefined_header.begin() + 1, predefined_header.end(), [&](auto& c) {
                    return f(get<0>(c), get<1>(c));
                });
                found != predefined_header.end()) {
                index = std::distance(predefined_header.begin(), found);
            }
            else if (auto found = std::find_if(dymap.begin(), dymap.end(), [&](auto& c) {
                         return f(get<0>(c), get<1>(c));
                     });
                     found != dymap.end()) {
                index = std::distance(dymap.begin(), found) + predefined_header_size;
            }
            else {
                return false;
            }
            return true;
        }

        template <class Table>
        constexpr size_t calc_table_size(Table& table) {
            size_t size = 0;
            for (auto& e : table) {
                size += e.first.size();
                size += e.second.size();
                size += 32;
            }
            return size;
        }

        struct OnModifyTable {
            constexpr void operator()(auto&& key, auto&& value, bool insert) {
            }
        };

        template <class Table>
        constexpr bool update_table(std::uint32_t table_size_limit, Table& table, auto&& on_modify_entry) {
            size_t tablesize = calc_table_size(table);
            while (tablesize > table_size_limit) {
                if (!table.size()) {
                    return false;
                }
                tablesize -= get<0>(table.back()).size();
                tablesize -= get<1>(table.back()).size();
                tablesize -= 32;
                on_modify_entry(get<0>(table.back()), get<1>(table.back()), false);
                table.pop_back();
            }
            return true;
        }

        template <class Table, class OnModify = OnModifyTable>
        constexpr HpkErr encode_table_size_update(binary::writer& w, std::uint32_t new_size, Table& table, OnModify&& on_modify_entry = OnModify{}) {
            auto err = hpack::encode_integer<5>(w, new_size, match(Field::dyn_table_update));
            if (err != HpackError::none) {
                return err;
            }
            return update_table(new_size, table, on_modify_entry) ? HpackError::none : HpackError::internal;
        }

        template <class String, class Table, class OnModify = OnModifyTable>
        constexpr HpkErr encode_field(binary::writer& w, Table& table, auto&& key, auto&& value, std::uint32_t table_size_limit, bool adddymap = false, OnModify&& on_modify_entry = OnModify{}) {
            HpackError err = HpackError::none;
            size_t index = 0;
            // search table entry
            if (get_table_index(
                    [&](const auto& k, const auto& v) {
                        return strutil::default_equal(k, key) && strutil::default_equal(v, value);
                    },
                    index, table)) {
                err = encode_integer<7>(w, index, match(Field::index));
                if (err != HpackError::none) {
                    return err;
                }
            }
            else {
                if (get_table_index(
                        [&](const auto& k, const auto&) {
                            return strutil::default_equal(k, key);
                        },
                        index, table)) {
                    if (adddymap) {
                        err = encode_integer<6>(w, index, match(Field::index_literal_insert));
                    }
                    else {
                        err = encode_integer<4>(w, index, match(Field::index_literal_no_insert));
                    }
                }
                else {
                    if (adddymap) {
                        err = w.write(match(Field::index_literal_insert), 1) ? HpackError::none : HpackError::output_length;
                    }
                    else {
                        err = w.write(match(Field::index_literal_no_insert), 1) ? HpackError::none : HpackError::output_length;
                    }
                    if (err != HpackError::none) {
                        return err;
                    }
                    err = encode_string(w, key);
                }
                if (err != HpackError::none) {
                    return err;
                }
                err = encode_string(w, value);
                if (err != HpackError::none) {
                    return err;
                }
                if (adddymap) {
                    table.push_front(std::make_pair(internal::convert_string<String>(key), internal::convert_string<String>(value)));
                    on_modify_entry(key, value, true);
                    if (!update_table(table_size_limit, table, on_modify_entry)) {
                        return HpackError::internal;
                    }
                }
            }
            return HpackError::none;
        }

        template <class String, class Table, class OnModify = OnModifyTable>
        constexpr HpkErr encode(String& dst, Table& table, auto&& header, std::uint32_t table_size_limit, bool add_dyn_map_default = false, OnModify&& on_modify_entry = OnModify{}) {
            binary::writer w(binary::resizable_buffer_writer<String>(), std::addressof(dst));
            HpackError err = HpackError::none;
            fnet::http1::apply_call_or_iter(header, [&](auto&& key, auto&& value, bool add_dyn_map = false) {
                err = encode_field<String>(w, table, key, value, table_size_limit, add_dyn_map || add_dyn_map_default, on_modify_entry);
                if (err != HpackError::none) {
                    return fnet::http1::header::HeaderError::validation_error;
                }
                return fnet::http1::header::HeaderError::none;
            });
            return err;
        }

        template <class String, class Table, class Header, class T, class OnModify = OnModifyTable>
        constexpr HpkErr decode_field(binary::basic_reader<T>& r, Table& table, Header&& header, std::uint32_t& table_size_limit, std::uint32_t max_table_size, OnModify&& on_modify_entry = OnModify{}) {
            if (r.empty()) {
                return HpackError::input_length;
            }
            auto update_table = [&]() -> bool {
                return hpack::update_table(table_size_limit, table, on_modify_entry);
            };
            HpackError err = HpackError::none;
            String key, value;
            auto read_two_literal = [&]() -> HpkErr {
                err = decode_string(key, r);
                if (err != HpackError::none) {
                    return err;
                }
                err = decode_string(value, r);
                if (err != HpackError::none) {
                    return err;
                }
                return true;
            };
            auto read_idx_and_literal = [&](size_t idx) -> HpkErr {
                if (idx < predefined_header_size) {
                    key = String(get<0>(predefined_header[idx]));
                }
                else {
                    if (table.size() <= idx - predefined_header_size) {
                        return HpackError::not_exists;
                    }
                    key = get<0>(table[idx - predefined_header_size]);
                }
                err = decode_string(value, r);
                if (err != HpackError::none) {
                    return err;
                }
                return true;
            };
            switch (get_field_type(r.top())) {
                default: {
                    return HpackError::invalid_mask;
                }
                case Field::index: {
                    std::uint64_t index = 0;
                    err = decode_integer<7>(r, index);
                    if (err != HpackError::none) {
                        return err;
                    }
                    if (index == 0) {
                        return HpackError::invalid_value;
                    }
                    if (index < predefined_header_size) {
                        if (!get<1>(predefined_header[index])[0]) {
                            return HpackError::not_exists;
                        }
                        fnet::http1::apply_call_or_emplace(header, get<0>(predefined_header[index]), get<1>(predefined_header[index]));
                    }
                    else {
                        if (table.size() <= index - predefined_header_size) {
                            return HpackError::not_exists;
                        }
                        fnet::http1::apply_call_or_emplace(header, get<0>(table[index - predefined_header_size]), get<1>(table[index - predefined_header_size]));
                    }
                    return HpackError::none;
                }
                case Field::index_literal_insert: {
                    std::uint64_t code = 0;
                    err = decode_integer<6>(r, code);
                    if (err != HpackError::none) {
                        return err;
                    }
                    if (code == 0) {
                        err = read_two_literal();
                    }
                    else {
                        err = read_idx_and_literal(code);
                    }
                    if (err != HpackError::none) {
                        return err;
                    }
                    table.push_front(std::make_pair(key, value));
                    on_modify_entry(std::as_const(key), std::as_const(value), true);
                    fnet::http1::apply_call_or_emplace(header, std::move(key), std::move(value));
                    if (!update_table()) {
                        return HpackError::table_update;
                    }
                    return HpackError::none;
                }
                case Field::index_literal_no_insert:
                case Field::index_literal_never_indexed: {
                    std::uint64_t code = 0;
                    err = decode_integer<4>(r, code);
                    if (err != HpackError::none) {
                        return err;
                    }
                    if (code == 0) {
                        err = read_two_literal();
                    }
                    else {
                        err = read_idx_and_literal(code);
                    }
                    if (err != HpackError::none) {
                        return err;
                    }
                    fnet::http1::apply_call_or_emplace(header, std::move(key), std::move(value));
                    return HpackError::none;
                }
                case Field::dyn_table_update: {
                    std::uint64_t code = 0;
                    err = decode_integer<5>(r, code);
                    if (err != HpackError::none) {
                        return err;
                    }
                    if (code > max_table_size) {
                        return HpackError::too_large_number;
                    }
                    table_size_limit = code;
                    if (!update_table()) {
                        return HpackError::table_update;
                    }
                    return HpackError::none;
                }
            }
        }

        template <class String, class Table, class Header, class OnModify = OnModifyTable>
        constexpr HpkErr decode(auto&& src, Table& table, Header&& header, std::uint32_t& table_size_limit, std::uint32_t max_table_size, OnModify&& on_modify = OnModify{}) {
            binary::reader r{src};
            while (!r.empty()) {
                auto err = decode_field<String>(r, table, header, table_size_limit, max_table_size, on_modify);
                if (err != HpackError::none) {
                    return err;
                }
            }
            return HpackError::none;
        }

    }  // namespace hpack

    namespace net::hpack {

        using namespace futils::hpack;  // for backward compatibility

    }  // namespace net::hpack

}  // namespace futils
