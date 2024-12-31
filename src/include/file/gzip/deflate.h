/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../binary/bit.h"
#include "../../binary/number.h"
#include "../../strutil/append.h"
#include "huffman.h"

namespace futils {
    namespace file::gzip::deflate {

        constexpr auto deflate_huffman_size = 286;
        constexpr auto deflate_distance_huffman_size = 30;

        constexpr auto make_deflate_fixed_huffman() {
            huffman::CanonicalTable<288> fixed_huf;
            huffman::CanonicalTable<32> distance_huf;
            for (auto i = 0; i <= 143; i++) {
                fixed_huf.codes[i].literal = i;
                fixed_huf.codes[i].bits = 8;
            }
            for (auto i = 144; i <= 255; i++) {
                fixed_huf.codes[i].literal = i;
                fixed_huf.codes[i].bits = 9;
            }
            for (auto i = 256; i <= 279; i++) {
                fixed_huf.codes[i].literal = i;
                fixed_huf.codes[i].bits = 7;
            }
            for (auto i = 280; i <= 287; i++) {
                fixed_huf.codes[i].literal = i;
                fixed_huf.codes[i].bits = 8;
            }
            fixed_huf.index = 288;
            fixed_huf.canonization();
            for (auto i = 0; i < 32; i++) {
                distance_huf.codes[i].literal = i;
                distance_huf.codes[i].bits = 5;
            }
            distance_huf.index = 32;
            distance_huf.canonization();
            return std::pair{fixed_huf, distance_huf};
        }

        constexpr auto deflate_fixed_huffman = make_deflate_fixed_huffman();

        constexpr huffman::StaticDecodeTable<288> deflate_fixed_huffman_table{
            [](huffman::DecodeTable& table, huffman::DecodeTree* place, size_t size) {
                auto [d, _] = deflate_fixed_huffman;
                table = d.static_decode_table(place, size);
            }};

        constexpr huffman::StaticDecodeTable<32> deflate_fixed_huffman_dist_table{
            [](huffman::DecodeTable& table, huffman::DecodeTree* place, size_t size) {
                auto [_, d] = deflate_fixed_huffman;
                table = d.static_decode_table(place, size);
            }};

        constexpr std::uint16_t length_extra_begin = 257;

        // clang-format off
        constexpr byte length_extra_bit_count[]{
            0,0,0,0,0,0,0,0, // 257~264
            1,1,1,1,         // 265~268
            2,2,2,2,         // 269~272
            3,3,3,3,         // 273~276 
            4,4,4,4,         // 277~280 
            5,5,5,5,         // 281~284 
            0,               // 285
        };

        constexpr std::uint16_t length_extra_length[]{
            3,4,5,6,7,8,9,10, // 257~264
            11,13,15,17,      // 265~268
            19,23,27,31,      // 269~272
            35,43,51,59,      // 273~276 
            67,83,99,115,     // 277~280 
            131,163,195,227,  // 281~284 
            258,              // 285
        };

        constexpr byte dist_extra_bit_count[]{
            0,0,0,0, 
            1,1,         
            2,2,         
            3,3,
            4,4,        
            5,5,         
            6,6,         
            7,7,
            8,8,        
            9,9,         
            10,10,         
            11,11,
            12,12,        
            13,13,              
        };

        constexpr std::uint16_t dist_extra_length[]{
            1,2,3,4, 
            5,7,         
            9,13,         
            17,25,
            33,49,        
            65,97,         
            129,193,         
            257,385,
            513,769,        
            1025,1537,         
            2049,3073,         
            4097,6145,
            8193,12289,        
            16385,24577,              
        };
        // clang-format on

        enum class DeflateError {
            none,
            input_length,
            invalid_btype,
            internal_bug,
            non_compressed_len,
            dynamic_huffman_tree,
            dynamic_huffman_litlen_code_len,
            dynamic_huffman_litlen_code,
            dynamic_huffman_dist_code,
            huffman_tree,
            distance,
            invalid_header,
            broken_data,
        };

        struct DynHuffmanHeader {
            huffman::DecodeTable code;
            huffman::DecodeTable dist;
        };

        // returns:
        // 1: ok
        // 0: invalid tree
        // -1: no input
        constexpr int read_huffman_code(const huffman::DecodeTree*& result, const huffman::DecodeTable& table, binary::bit_reader& r) {
            result = table.get_root();
            while (true) {
                if (!result) {
                    return 0;
                }
                if (result->has_value()) {
                    return 1;
                }
                bool next = false;
                if (!r.read(next)) {
                    return -1;
                }
                result = table.get_next(result, next);
            }
            return 0;
        }

        constexpr DeflateError read_dynamic_table(DynHuffmanHeader& head, const huffman::DecodeTable& root, binary::bit_reader& r, std::uint16_t litlen, std::uint16_t dist) {
            const huffman::DecodeTree* cur = nullptr;
            constexpr auto size = deflate_huffman_size + deflate_distance_huffman_size;
            huffman::CanonicalTable<deflate_huffman_size> codeh;
            huffman::CanonicalTable<deflate_distance_huffman_size> disth;
            size_t index = 0;
            byte prev = 0;
            auto add_literal = [&](byte lit) {
                if (codeh.index < litlen) {
                    codeh.codes[codeh.index].bits = lit;
                    codeh.codes[codeh.index].literal = codeh.index;
                    codeh.index++;
                }
                else if (disth.index < dist) {
                    disth.codes[disth.index].bits = lit;
                    disth.codes[disth.index].literal = disth.index;
                    disth.index++;
                }
                prev = lit;
            };
            auto read_tree = [&] {
                switch (read_huffman_code(cur, root, r)) {
                    default:
                        return DeflateError::dynamic_huffman_litlen_code_len;
                    case 1:
                        return DeflateError::none;
                    case -1:
                        return DeflateError::input_length;
                }
            };
            auto add_repeat = [&](byte bits, byte add, byte copy) {
                byte count = 0;
                if (!r.read(count, bits, false)) {
                    return DeflateError::input_length;
                }
                count += add;
                for (auto i = 0; i < count; i++) {
                    if (index >= size) {
                        return DeflateError::dynamic_huffman_litlen_code;
                    }
                    add_literal(copy);
                    index++;
                }
                return DeflateError::none;
            };

            auto limit = litlen + dist;
            while (index < limit) {
                if (auto err = read_tree(); err != DeflateError::none) {
                    return err;
                }
                auto val = cur->get_value();
                if (val < 16) {
                    if (index >= size) {
                        return DeflateError::dynamic_huffman_litlen_code;
                    }
                    add_literal(val);
                    index++;
                }
                else if (val == 16) {
                    if (index == 0) {
                        return DeflateError::dynamic_huffman_litlen_code;
                    }
                    if (auto err = add_repeat(2, 3, prev); err != DeflateError::none) {
                        return err;
                    }
                }
                else if (val == 17) {
                    if (auto err = add_repeat(3, 3, 0); err != DeflateError::none) {
                        return err;
                    }
                }
                else if (val == 18) {
                    if (auto err = add_repeat(7, 11, 0); err != DeflateError::none) {
                        return err;
                    }
                }
                else {
                    return DeflateError::dynamic_huffman_litlen_code;
                }
            }
            codeh.canonization();
            disth.canonization();
            head.code = codeh.decode_table();
            if (!head.code.has_table()) {
                return DeflateError::dynamic_huffman_litlen_code;
            }
            head.dist = disth.decode_table();
            if (!head.code.has_table()) {
                return DeflateError::dynamic_huffman_dist_code;
            }
            return DeflateError::none;
        }

        constexpr DeflateError read_dyn_huffman_header(DynHuffmanHeader& h, binary::bit_reader& r) {
            std::uint16_t hlit = 0, hdist = 0, hclen = 0;
            if (!r.read(hlit, 5, false) ||
                !r.read(hdist, 5, false) ||
                !r.read(hclen, 4, false)) {
                return DeflateError::input_length;
            }
            hlit += 257;
            hclen += 4;
            hdist += 1;
            byte indeces[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            huffman::CanonicalTable<19, huffman::Code, huffman::Direct> table;
            for (auto i = 0; i < hclen; i++) {
                table.codes[i].literal = indeces[i];
                byte bits = 0;
                if (!r.read(bits, 3, false)) {
                    return DeflateError::input_length;
                }
                table.codes[i].bits = bits;
            }
            for (auto i = hclen; i < 19; i++) {
                table.codes[i].literal = indeces[i];
                table.codes[i].bits = 0;
            }
            table.index = 19;
            table.canonization();
            huffman::DecodeTable tree = table.decode_table();
            if (!tree.has_table()) {
                return DeflateError::dynamic_huffman_tree;
            }
            if (auto err = read_dynamic_table(h, tree, r, hlit, hdist); err != DeflateError::none) {
                return err;
            }
            return DeflateError::none;
        }

        template <class Out>
        constexpr DeflateError decode_block(Out& out, const huffman::DecodeTable& litlen, const huffman::DecodeTable& dist, binary::bit_reader& r) {
            const huffman::DecodeTree* value;
            auto read_litlen = [&] {
                switch (read_huffman_code(value, litlen, r)) {
                    default:
                        return DeflateError::huffman_tree;
                    case 1:
                        return DeflateError::none;
                    case -1:
                        return DeflateError::input_length;
                }
            };
            auto read_dist = [&] {
                switch (read_huffman_code(value, dist, r)) {
                    default:
                        return DeflateError::huffman_tree;
                    case 1:
                        return DeflateError::none;
                    case -1:
                        return DeflateError::input_length;
                }
            };
            while (true) {
                if (auto err = read_litlen(); err != DeflateError::none) {
                    return err;
                }
                auto val = value->get_value();
                if (val <= 0xff) {
                    out.push_back(byte(val));
                    continue;
                }
                if (val == 256) {
                    break;
                }
                auto extra = length_extra_bit_count[val - length_extra_begin];
                std::uint16_t len = 0;
                if (!r.read(len, extra, false)) {
                    return DeflateError::input_length;
                }
                len += length_extra_length[val - length_extra_begin];
                if (auto err = read_dist(); err != DeflateError::none) {
                    return err;
                }
                val = value->get_value();
                std::uint16_t dist = 0;
                extra = dist_extra_bit_count[val];
                if (!r.read(dist, extra, false)) {
                    return DeflateError::input_length;
                }
                dist += dist_extra_length[val];
                if (out.size() < dist) {
                    return DeflateError::distance;
                }
                auto pos = out.size() - dist;
                for (auto i = 0; i < len; i++) {
                    if (pos + i >= out.size()) {
                        return DeflateError::distance;
                    }
                    auto c = out[pos + i];
                    out.push_back(c);
                }
            }
            return DeflateError::none;
        }

        template <class Out>
        DeflateError decode_deflate(Out& out, binary::bit_reader& r) {
            r.set_direction(true);
            bool fin = false;
            while (!fin) {
                if (!r.read(fin)) {
                    return DeflateError::input_length;
                }
                byte btype = 0;
                if (!r.read(btype, 2, false)) {
                    return DeflateError::input_length;
                }
                switch (btype) {
                    default:
                        return DeflateError::invalid_btype;
                    case 0b00: {
                        if (!r.skip_align()) {
                            return DeflateError::input_length;
                        }
                        auto& br = r.get_base();
                        if (!br.load_stream(4)) {
                            return DeflateError::input_length;
                        }
                        std::uint16_t len = 0, nlen = 0;
                        if (!binary::read_num(br, len, false) ||
                            !binary::read_num(br, nlen, false)) {
                            return DeflateError::internal_bug;
                        }
                        if (std::uint16_t(~len) != nlen) {
                            return DeflateError::non_compressed_len;
                        }
                        if (!br.load_stream(len)) {
                            return DeflateError::input_length;
                        }
                        auto [data, ok] = br.read_direct(len);
                        if (!ok) {
                            return DeflateError::internal_bug;
                        }
                        strutil::append(out, data);
                        break;
                    }
                    case 0b01: {
                        auto& fix = deflate_fixed_huffman_table.table();
                        auto& dist = deflate_fixed_huffman_dist_table.table();
                        auto err = decode_block(out, fix, dist, r);
                        if (err != DeflateError::none) {
                            return err;
                        }
                        break;
                    }
                    case 0b10: {
                        DynHuffmanHeader head;
                        auto err = read_dyn_huffman_header(head, r);
                        if (err != DeflateError::none) {
                            return err;
                        }
                        err = decode_block(out, head.code, head.dist, r);
                        if (err != DeflateError::none) {
                            return err;
                        }
                        break;
                    }
                }
            }
            return DeflateError::none;
        }
    }  // namespace file::gzip::deflate
}  // namespace futils
