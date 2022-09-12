/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
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
#include "../wrap/light/enum.h"
#include "../endian/reader.h"
#include "../endian/writer.h"
#include "../helper/equal.h"
#include "hpack_huffman.h"

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
        HpkErr encode(String& dst, Table& table, Header& header, std::uint32_t maxtablesize, bool adddymap = false) {
            endian::Writer<String&> se(dst);
            HpackError err = HpackError::none;
            for (auto&& h : header) {
                size_t index = 0;
                if (get_table_index(
                        [&](const auto& k, const auto& v) {
                            return helper::default_equal(k, get<0>(h)) && helper::default_equal(v, get<1>(h));
                        },
                        index, table)) {
                    err = encode_integer<7>(se, index, 0x80);
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
                            err = encode_integer<6>(se, index, 0x40);
                        }
                        else {
                            err = encode_integer<4>(se, index, 0);
                        }
                        if (err != HpackError::none) {
                            return err;
                        }
                    }
                    else {
                        se.write(std::uint8_t(0));
                        encode_string(se, get<0>(h));
                    }
                    encode_string(se, get<1>(h));
                    if (adddymap) {
                        table.push_front({get<0>(h), get<1>(h)});
                        size_t tablesize = calc_table_size(table);
                        while (tablesize > maxtablesize) {
                            if (!table.size()) return false;
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
        HpkErr decode(String& src, Table& table, Header& header, std::uint32_t& maxtablesize) {
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
            endian::Reader<String&> se(src);
            HpackError err = HpackError::none;
            while (!se.seq.eos()) {
                const unsigned char mask = se.seq.current();
                unsigned char tmp = 0;
                String key, value;
                auto read_two_literal = [&]() -> HpkErr {
                    auto err = decode_string(key, se);
                    if (err != HpackError::none) {
                        return err;
                    }
                    err = decode_string(value, se);
                    if (err != HpackError::none) {
                        return err;
                    }
                    header.emplace(key, value);
                    return true;
                };
                auto read_idx_and_literal = [&](size_t idx) -> HpkErr {
                    if (idx < predefined_header_size) {
                        key = get<0>(predefined_header[idx]);
                    }
                    else {
                        if (table.size() <= idx - predefined_header_size) {
                            return HpackError::not_exists;
                        }
                        key = get<0>(table[idx - predefined_header_size]);
                    }
                    auto err = decode_string(value, se);
                    if (err != HpackError::none) {
                        return err;
                    }
                    header.emplace(key, value);
                    return true;
                };
                if (mask & 0x80) {
                    size_t index = 0;
                    err = decode_integer<7>(se, index, tmp);
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
                    err = decode_integer<6>(se, code, tmp);
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
                    err = decode_integer<5>(se, code, tmp);
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
                    err = decode_integer<4>(se, code, tmp);
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

    namespace net {
        namespace hpack {
            /*
            namespace internal {
                using namespace utils::hpack::internal;

                template <class String, class Table, class Header>
                struct Hpack {
                    using string_coder = HpackStringCoder<String>;
                    using integer_coder = IntegerCoder<String>;
                    using string_t = String;
                    using table_t = Table;
                    using header_t = Header;

                   private:
                    template <class F>
                    static bool get_idx(F&& f, size_t& idx, table_t& dymap) {
                        if (auto found = std::find_if(predefined_header.begin() + 1, predefined_header.end(), [&](auto& c) {
                                return f(get<0>(c), get<1>(c));
                            });
                            found != predefined_header.end()) {
                            idx = std::distance(predefined_header.begin(), found);
                        }
                        else if (auto found = std::find_if(dymap.begin(), dymap.end(), [&](auto& c) {
                                     return f(get<0>(c), get<1>(c));
                                 });
                                 found != dymap.end()) {
                            idx = std::distance(dymap.begin(), found) + predefined_header_size;
                        }
                        else {
                            return false;
                        }
                        return true;
                    }

                    static size_t calc_table_size(table_t& table) {
                        size_t size = 32;
                        for (auto& e : table) {
                            size += e.first.size();
                            size += e.second.size();
                        }
                        return size;
                    }

                   public:
                    static HpkErr encode(bool adddy, header_t& src, string_t& dst,
                                         table_t& dymap, std::uint32_t maxtablesize) {
                        endian::Writer<string_t&> se(dst);
                        for (auto&& h : src) {
                            size_t idx = 0;
                            if (get_idx(
                                    [&](const auto& k, const auto& v) {
                                        return helper::default_equal(k, get<0>(h)) && helper::default_equal(v, get<1>(h));
                                    },
                                    idx, dymap)) {
                                TRY(integer_coder::template encode<7>(se, idx, 0x80));
                            }
                            else {
                                if (get_idx(
                                        [&](const auto& k, const auto&) {
                                            return helper::default_equal(k, get<0>(h));
                                        },
                                        idx, dymap)) {
                                    if (adddy) {
                                        TRY(integer_coder::template encode<6>(se, idx, 0x40));
                                    }
                                    else {
                                        TRY(integer_coder::template encode<4>(se, idx, 0));
                                    }
                                }
                                else {
                                    se.write(std::uint8_t(0));
                                    string_coder::encode(se, h.first);
                                }
                                string_coder::encode(se, h.second);
                                if (adddy) {
                                    dymap.push_front({h.first, h.second});
                                    size_t tablesize = calc_table_size(dymap);
                                    while (tablesize > maxtablesize) {
                                        if (!dymap.size()) return false;
                                        tablesize -= dymap.back().first.size();
                                        tablesize -= dymap.back().second.size();
                                        dymap.pop_back();
                                    }
                                }
                            }
                        }
                        return true;
                    }

                    static HpkErr decode(header_t& res, string_t& src,
                                         table_t& dymap, std::uint32_t& maxtablesize) {
                        auto update_dymap = [&] {
                            size_t tablesize = calc_table_size(dymap);
                            while (tablesize > maxtablesize) {
                                if (tablesize == 32) break;  // TODO: check what written in RFC means
                                if (!dymap.size()) {
                                    return false;
                                }
                                tablesize -= get<0>(dymap.back()).size();
                                tablesize -= get<1>(dymap.back()).size();
                                dymap.pop_back();
                            }
                            return true;
                        };
                        endian::Reader<string_t&> se(src);
                        while (!se.seq.eos()) {
                            unsigned char tmp = se.seq.current();
                            string_t key, value;
                            auto read_two_literal = [&]() -> HpkErr {
                                TRY(string_coder::decode(key, se));
                                TRY(string_coder::decode(value, se));
                                res.emplace(key, value);
                                return true;
                            };
                            auto read_idx_and_literal = [&](size_t idx) -> HpkErr {
                                if (idx < predefined_header_size) {
                                    key = predefined_header[idx].first;
                                }
                                else {
                                    if (dymap.size() <= idx - predefined_header_size) {
                                        return HpackError::not_exists;
                                    }
                                    key = dymap[idx - predefined_header_size].first;
                                }
                                TRY(string_coder::decode(value, se));
                                res.emplace(key, value);
                                return true;
                            };
                            if (tmp & 0x80) {
                                size_t idx = 0;
                                TRY(integer_coder::template decode<7>(se, idx, tmp));
                                if (idx == 0) {
                                    return HpackError::invalid_value;
                                }
                                if (idx < predefined_header_size) {
                                    if (!predefined_header[idx].second[0]) {
                                        return HpackError::not_exists;
                                    }
                                    res.emplace(predefined_header[idx].first, predefined_header[idx].second);
                                }
                                else {
                                    if (dymap.size() <= idx - predefined_header_size) {
                                        return HpackError::not_exists;
                                    }
                                    res.emplace(dymap[idx - predefined_header_size].first, dymap[idx - predefined_header_size].second);
                                }
                            }
                            else if (tmp & 0x40) {
                                size_t sz = 0;

                                TRY(integer_coder::template decode<6>(se, sz, tmp));

                                if (sz == 0) {
                                    TRY(read_two_literal());
                                }
                                else {
                                    TRY(read_idx_and_literal(sz));
                                }
                                dymap.push_front({key, value});
                                TRY(update_dymap());
                            }
                            else if (tmp & 0x20) {  // dynamic table size change
                                // unimplemented
                                size_t sz = 0;
                                TRY(integer_coder::template decode<5>(se, sz, tmp));
                                if (maxtablesize > 0x80000000) {
                                    return HpackError::too_large_number;
                                }
                                maxtablesize = (std::int32_t)sz;
                                TRY(update_dymap());
                            }
                            else {
                                size_t sz = 0;
                                TRY(integer_coder::template decode<4>(se, sz, tmp));
                                if (sz == 0) {
                                    TRY(read_two_literal());
                                }
                                else {
                                    TRY(read_idx_and_literal(sz));
                                }
                            }
                        }
                        return true;
                    }
                };
#undef TRY
            }  // namespace internal

            template <class Table, class Header, class String>
            utils::hpack::HpkErr encode(String& str, Table& table, Header& header, std::uint32_t maxtablesize, bool adddymap = false) {
                return internal::Hpack<String, Table, Header>::encode(adddymap, header, str, table, maxtablesize);
            }

            template <class Table, class Header, class String>
            utils::hpack::HpkErr decode(String& str, Table& table, Header& header, std::uint32_t& maxtablesize) {
                return internal::Hpack<String, Table, Header>::decode(header, str, table, maxtablesize);
            }*/

            using namespace utils::hpack;  // for backwoard compatibility

        }  // namespace hpack

    }  // namespace net

}  // namespace utils
