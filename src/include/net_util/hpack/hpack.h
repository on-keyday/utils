/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
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

namespace utils {
    namespace hpack {
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
            size_t size = 32;
            for (auto& e : table) {
                size += e.first.size();
                size += e.second.size();
            }
            return size;
        }

        template <class String, class Table, class Header>
        constexpr HpkErr encode(String& dst, Table& table, Header& header, std::uint32_t maxtablesize, bool adddymap = false) {
            io::expand_writer<String&> w(dst);
            HpackError err = HpackError::none;
            for (auto&& h : header) {
                size_t index = 0;
                if (get_table_index(
                        [&](const auto& k, const auto& v) {
                            return helper::default_equal(k, get<0>(h)) && helper::default_equal(v, get<1>(h));
                        },
                        index, table)) {
                    err = encode_integer<7>(w, index, 0x80);
                    if (err != HpackError::none) {
                        return err;
                    }
                }
                else {
                    if (get_table_index(
                            [&](const auto& k, const auto&) {
                                return helper::default_equal(k, get<0>(h));
                            },
                            index, table)) {
                        if (adddymap) {
                            err = encode_integer<6>(w, index, 0x40);
                        }
                        else {
                            err = encode_integer<4>(w, index, 0);
                        }
                        if (err != HpackError::none) {
                            return err;
                        }
                    }
                    else {
                        w.write(std::uint8_t(0), 1);
                        encode_string(w, get<0>(h));
                    }
                    encode_string(w, get<1>(h));
                    if (adddymap) {
                        table.push_front({get<0>(h), get<1>(h)});
                        size_t tablesize = calc_table_size(table);
                        while (tablesize > maxtablesize) {
                            if (!table.size()) {
                                return false;
                            }
                            tablesize -= get<0>(table.back()).size();
                            tablesize -= get<1>(table.back()).size();
                            table.pop_back();
                        }
                    }
                }
            }
            return true;
        }

        template <class String, class Table, class Header>
        constexpr HpkErr decode(String& src, Table& table, Header& header, std::uint32_t& maxtablesize) {
            auto update_table = [&]() -> bool {
                size_t tablesize = calc_table_size(table);
                while (tablesize > maxtablesize) {
                    if (tablesize == 32) break;  // TODO: check what written in RFC means
                    if (!table.size()) {
                        return false;
                    }
                    tablesize -= get<0>(table.back()).size();
                    tablesize -= get<1>(table.back()).size();
                    table.pop_back();
                }
                return true;
            };
            io::reader r{src};
            HpackError err = HpackError::none;
            while (!r.empty()) {
                const unsigned char mask = r.top();
                String key, value;
                auto read_two_literal = [&]() -> HpkErr {
                    auto err = decode_string(key, r);
                    if (err != HpackError::none) {
                        return err;
                    }
                    err = decode_string(value, r);
                    if (err != HpackError::none) {
                        return err;
                    }
                    header.emplace(key, value);
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
                    auto err = decode_string(value, r);
                    if (err != HpackError::none) {
                        return err;
                    }
                    header.emplace(key, value);
                    return true;
                };
                if (mask & 0x80) {
                    size_t index = 0;
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
                        header.emplace(get<0>(predefined_header[index]), get<1>(predefined_header[index]));
                    }
                    else {
                        if (table.size() <= index - predefined_header_size) {
                            return HpackError::not_exists;
                        }
                        header.emplace(get<0>(table[index - predefined_header_size]), get<1>(table[index - predefined_header_size]));
                    }
                }
                else if (mask & 0x40) {
                    size_t code = 0;
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
                    table.push_front({key, value});
                    if (!update_table()) {
                        return HpackError::table_update;
                    }
                }
                else if (mask & 0x20) {  // dynamic table size change
                    // unimplemented
                    size_t code = 0;
                    err = decode_integer<5>(r, code);
                    if (err != HpackError::none) {
                        return err;
                    }
                    if (maxtablesize > 0x80000000) {
                        return HpackError::too_large_number;
                    }
                    maxtablesize = (std::int32_t)code;
                    if (!update_table()) {
                        return HpackError::table_update;
                    }
                }
                else {
                    size_t code = 0;
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
                }
            }
            return true;
        }

    }  // namespace hpack

    namespace net::hpack {

        using namespace utils::hpack;  // for backwoard compatibility

    }  // namespace net::hpack

}  // namespace utils
