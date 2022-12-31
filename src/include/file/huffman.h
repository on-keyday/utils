/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <cstddef>
#include <memory>
#include <algorithm>
#include <compare>
#include <initializer_list>
#include "../core/byte.h"

namespace utils {
    namespace huffman {
        using byte = unsigned char;
        struct Vec {
            std::uint32_t value;
            byte bitcount;

            constexpr Vec()
                : value(0), bitcount(0) {}

            static constexpr std::uint32_t shift(size_t i) {
                if (i >= 32) {
                    return 0;
                }
                return std::uint32_t(1) << (31 - i);
            }

            constexpr Vec(byte bitsize, std::uint32_t value)
                : Vec() {
                if (bitsize > 32) {
                    bitsize = 32;
                }
                for (auto i = 0; i < bitsize; i++) {
                    push_back(shift(31 - (bitsize - 1 - i)) & value);
                }
            }

            constexpr Vec(std::initializer_list<int> list)
                : Vec() {
                constexpr auto test = shift(0);
                for (auto v : list) {
                    if (!push_back(v)) {
                        break;
                    }
                }
            }

            constexpr bool push_back(bool v) {
                if (bitcount >= 32) {
                    return false;
                }
                value |= v ? shift(bitcount) : 0;
                bitcount++;
                return true;
            }

            constexpr bool operator[](size_t i) const {
                if (i >= bitcount) {
                    return false;
                }
                return shift(i) & value;
            }

            constexpr size_t size() const {
                return bitcount;
            }
        };

        struct Byte {
            byte label = 0;
            size_t count = 0;
            Vec result;

            constexpr std::strong_ordering operator<=>(const Byte& b) const {
                return count <=> b.count;
            }

            constexpr bool operator==(const Byte& b) const {
                return (*this <=> b) == 0;
            }
        };

        struct Tree {
            Byte b;
            std::shared_ptr<Tree> zero;
            std::shared_ptr<Tree> one;
        };

        template <class Str>
        struct WVec {
            Str str;
            size_t bit = 0;

            void push_back(bool v) {
                if (bit % 8 == 0) {
                    str.push_back(0);
                }
                str[str.size() - 1] |= (v ? 1 << (7 - bit) : 0);
                bit++;
            }

            void append(const Vec& v) {
                for (auto i = 0; i < v.size(); i++) {
                    push_back(v[i]);
                }
            }

            void fill() {
                while (bit % 8 != 0) {
                    push_back(1);
                }
            }
        };

        struct ByteTable {
            size_t table[256]{0};
            constexpr size_t& operator[](size_t i) {
                return table[i];
            }
            constexpr const size_t& operator[](size_t i) const {
                return table[i];
            }
            constexpr size_t size() const {
                return sizeof(table) / sizeof(table[0]);
            }
            constexpr size_t* begin() {
                return table;
            }
            constexpr size_t* end() {
                return table + size();
            }
        };

        struct OrderTable {
            Byte table[256];
            constexpr OrderTable(const ByteTable& b) {
                for (auto i = 0; i < b.size(); i++) {
                    table[i] = Byte{byte(i), b[i]};
                }
            }

            constexpr Byte& operator[](size_t i) {
                return table[i];
            }
            constexpr const Byte& operator[](size_t i) const {
                return table[i];
            }

            constexpr size_t size() const {
                return sizeof(table) / sizeof(table[0]);
            }

            constexpr Byte* begin() {
                return table;
            }
            constexpr Byte* end() {
                return table + size();
            }
        };

        inline void append(const Byte& v, std::shared_ptr<Tree>& p, int i = 0) {
            if (!p) {
                p = std::make_shared<Tree>();
                p->b = {};
            }
            if (i == v.result.size()) {
                p->b = v;
                return;
            }
            if (v.result[i]) {
                append(v, p->one, i + 1);
            }
            else {
                append(v, p->zero, i + 1);
            }
        }

        constexpr bool convert_table(const ByteTable& table, byte head_bit, byte fragsize, auto&& cb) {
            OrderTable order = table;
            std::ranges::sort(order, std::greater<>{});
            auto size = order.size();
            bool end = false;
            size_t count = 0;
            size_t reserve_bit = fragsize;
            size_t expand = 1;
            for (auto i = 0; i < head_bit + 1; i++) {
                auto maxi = std::uint32_t(1) << (reserve_bit);
                for (size_t k = 0; k < maxi; k++) {
                    Vec v;
                    for (auto u = 0; u < head_bit - i; u++) {
                        v.push_back(0);
                    }
                    if (i != 0) {
                        v.push_back(1);
                    }
                    auto val = Vec(reserve_bit, k);
                    for (auto c = 0; c < val.size(); c++) {
                        v.push_back(val[c]);
                    }
                    auto c = order[count];
                    c.result = v;
                    cb(c);
                    count++;
                    if (count == size) {
                        end = true;
                        break;
                    }
                }
                if (end) {
                    break;
                }
                reserve_bit += expand;
                if (expand < 2) {
                    expand++;
                }
            }
            if (count != size) {
                return false;
            }
            return true;
        }

        inline std::shared_ptr<Tree> make_tree(const ByteTable& table, byte head_bit, byte fragsize) {
            std::shared_ptr<Tree> tree;
            if (!convert_table(table, head_bit, fragsize, [&](const Byte& b) {
                    append(b, tree);
                })) {
                return nullptr;
            }
            return tree;
        }

        struct HuffmanTable {
            ByteTable table;
            void sample(std::uint8_t s) {
                table[s]++;
            }

            void reset() {
                for (auto i = 0; i < sizeof(table); i++) {
                    table[i] = 0;
                }
            }

            std::shared_ptr<Tree> tree(byte head_bit, byte fragment_size) const {
                return make_tree(table, head_bit, fragment_size);
            }

            bool vec(auto&& v, byte head_bit, byte fragment_size) const {
                return convert_table(table, head_bit, fragment_size, [&](Byte b) {
                    v.push_back(b);
                });
            }

            bool fixedvec(auto&& v, byte head_bit, byte fragment_size) const {
                return convert_table(table, head_bit, fragment_size, [&](Byte b) {
                    v[b.label] = b;
                });
            }
        };
    }  // namespace huffman
}  // namespace utils
