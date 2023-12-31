/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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
#include "../../view/iovec.h"
#include "../../helper/defer.h"

namespace utils {
    namespace file::gzip::huffman {

        struct Code {
            std::uint16_t literal = 0;
            byte bits = 0;
            std::uint32_t code = 0;

            template <class Bool = bool>
            constexpr bool write(auto&& out, Bool t = Bool(1), Bool f = Bool(0)) const {
                if (bits == 0 || bits > 64) {
                    return false;
                }
                auto b = std::uint64_t(1) << (bits - 1);
                for (auto i = 0; i < bits; i++) {
                    out.push_back(code & b ? t : f);
                    if (b == 1) {
                        break;
                    }
                    b >>= 1;
                }
                return true;
            }

            constexpr bool index(byte i) const {
                if (i >= bits) {
                    return false;
                }
                return code & (std::uint64_t(1) << (bits - 1 - i));
            }
        };

        struct CodeMap {
            Code code;
            std::uint64_t appear = 0;
        };

        struct Direct {
            constexpr auto& operator()(auto& m) {
                return m;
            }
        };

        struct UnwrapCodeMap {
            constexpr auto& operator()(auto& m) {
                return m.code;
            }
        };

        template <size_t size, class Fn = Direct>
        constexpr bool make_canonical_code(auto* base, std::uint16_t N, Fn map = Direct{}) {
            if (!base || N <= 0 || N > size) {
                return false;
            }
            std::uint32_t next_codes[size] = {};
            std::uint16_t counts[size] = {};
            std::uint16_t n = 0;
            auto mp = [&](auto i) -> Code& {
                return map(base[i]);
            };
            for (auto i = 0; i < N; i++) {
                counts[mp(i).bits]++;
                if (mp(i).bits > n) {
                    n = mp(i).bits;
                }
            }
            std::uint32_t code = 0;
            for (auto len = 1; len <= n; len++) {
                code = (code + counts[len - 1]) << 1;
                next_codes[len] = code;
            }
            for (auto i = 0; i < N; i++) {
                const auto length = mp(i).bits;
                if (length != 0) {
                    mp(i).code = next_codes[length];
                    next_codes[length]++;
                }
            }
            return true;
        }

        template <size_t size, class Fn = Direct>
        constexpr bool make_canonical_code(auto (&base)[size], std::uint16_t N, Fn map = Direct{}) {
            return make_canonical_code<size>(base + 0, N, map);
        }

        namespace test {
            constexpr bool check_huffman() {
                Code syms[] = {{'A', 3}, {'B', 3}, {'C', 3}, {'D', 3}, {'E', 3}, {'F', 2}, {'G', 4}, {'H', 4}};
                make_canonical_code(syms, 8);
                return syms[0].code == 0b010 &&
                       syms[1].code == 0b011 &&
                       syms[2].code == 0b100 &&
                       syms[3].code == 0b101 &&
                       syms[4].code == 0b110 &&
                       syms[5].code == 0b00 &&
                       syms[6].code == 0b1110 &&
                       syms[7].code == 0b1111;
            }

            static_assert(check_huffman());

        }  // namespace test

        struct DecodeTree {
           private:
            static constexpr std::uint32_t mask_zero = 0x00007fff;
            static constexpr std::uint32_t mask_one = 0x3fff8000;
            static constexpr std::uint32_t flag_value = 0x80000000;
            static constexpr std::uint32_t mask_shift = 15;
            std::uint32_t data = 0;

            static_assert((mask_one >> mask_shift) == mask_zero);

           public:
            constexpr std::uint16_t one() const {
                return std::uint16_t((data & mask_one) >> mask_shift);
            }

            constexpr std::uint16_t zero() const {
                return data & mask_zero;
            }

            constexpr bool value_set() const {
                return data != 0;
            }

            constexpr bool set_zero(std::uint16_t p) {
                if (p > mask_zero) {
                    return false;
                }
                data &= ~mask_zero;
                data |= p & mask_zero;
                return true;
            }

            constexpr bool set_one(std::uint16_t p) {
                if (p > mask_zero) {
                    return false;
                }
                data &= ~mask_one;
                data |= (p << mask_shift) & mask_one;
                return true;
            }

            constexpr bool set_value(std::uint32_t code) {
                if (code >= flag_value) {
                    return false;
                }
                data = code | flag_value;
                return true;
            }

            constexpr bool has_value() const {
                return data & flag_value;
            }

            constexpr std::uint32_t get_value() const {
                return data & ~flag_value;
            }
        };

        struct DecodeTable {
           private:
            DecodeTree* table = nullptr;
            std::uint16_t size = 0;
            std::uint16_t index = 0;
            bool borrow = false;

            constexpr void remove() {
                if (!borrow) {
                    delete[] table;
                }
                table = nullptr;
                size = 0;
                index = 0;
                borrow = false;
            }

           public:
            constexpr DecodeTable() = default;

            constexpr DecodeTable(DecodeTable&& i)
                : table(std::exchange(i.table, nullptr)),
                  size(std::exchange(i.size, 0)),
                  index(std::exchange(i.index, 0)),
                  borrow(std::exchange(i.borrow, false)) {}

            constexpr DecodeTable& operator=(DecodeTable&& i) {
                if (this == &i) {
                    return *this;
                }
                remove();
                table = std::exchange(i.table, nullptr);
                size = std::exchange(i.size, 0);
                index = std::exchange(i.index, 0);
                borrow = std::exchange(i.borrow, false);
                return *this;
            }

            constexpr ~DecodeTable() {
                remove();
            }

            constexpr bool has_table() const {
                return table != nullptr;
            }

            void set_leaf_count(std::uint16_t s) {
                remove();
                size = (s << 1) - 1;
                table = new DecodeTree[size];
                index = 1;
            }

            constexpr void set_table_place(DecodeTree* place, std::uint16_t count) {
                remove();
                size = count;
                table = place;
                index = 1;
                borrow = true;
            }

            constexpr bool set_code(const Code& code) {
                if (!table) {
                    return false;
                }
                if (code.bits == 0) {
                    return true;  // ignore
                }
                byte i = 0;
                size_t cur = 0;
                for (;; i++) {
                    auto& elm = table[cur];
                    if (code.bits == i) {
                        if (elm.value_set()) {
                            return false;
                        }
                        return elm.set_value(code.literal);
                    }
                    if (code.index(i)) {
                        cur = elm.one();
                        if (!cur) {
                            if (index >= size) {
                                return false;
                            }
                            if (!elm.set_one(index)) {
                                return false;
                            }
                            cur = index;
                            index++;
                        }
                    }
                    else {
                        cur = elm.zero();
                        if (!cur) {
                            if (index >= size) {
                                return false;
                            }
                            if (!elm.set_zero(index)) {
                                return false;
                            }
                            cur = index;
                            index++;
                        }
                    }
                }
            }

            constexpr const DecodeTree* get_root() const {
                if (!table) {
                    return nullptr;
                }
                return &table[0];
            }

            constexpr const DecodeTree* get_next(const DecodeTree* c, bool next) const {
                if (!table || c->has_value()) {
                    return nullptr;
                }
                if (!std::is_constant_evaluated()) {
                    if (c < table || table + size <= c) {
                        return nullptr;
                    }
                }
                auto expect = next ? c->one() : c->zero();
                if (!expect || expect >= index) {
                    return nullptr;
                }
                return &table[expect];
            }
        };

        struct EncodeTree : DecodeTree {
            std::uint64_t count = 0;
        };

        struct EncodeTable {
           private:
            EncodeTree* table = nullptr;
            std::uint16_t size = 0;
            std::uint16_t lit_index = 0;
            std::uint16_t full_index = 0;
            bool borrow = false;

            constexpr void remove() {
                if (!borrow) {
                    delete[] table;
                }
                table = nullptr;
                size = 0;
                lit_index = 0;
                full_index = 0;
                borrow = false;
            }

           public:
            constexpr EncodeTable() = default;

            constexpr EncodeTable(EncodeTable&& i)
                : table(std::exchange(i.table, nullptr)),
                  size(std::exchange(i.size, 0)),
                  lit_index(std::exchange(i.lit_index, 0)),
                  full_index(std::exchange(i.full_index, 0)),
                  borrow(std::exchange(i.borrow, false)) {}

            constexpr EncodeTable& operator=(EncodeTable&& i) {
                if (this == &i) {
                    return *this;
                }
                remove();
                table = std::exchange(i.table, nullptr);
                size = std::exchange(i.size, 0);
                lit_index = std::exchange(i.lit_index, 0);
                full_index = std::exchange(i.full_index, 0);
                borrow = std::exchange(i.borrow, false);
                return *this;
            }

            constexpr bool has_table() const {
                return table != nullptr;
            }

            void set_max_leaf(std::uint16_t nleaf) {
                remove();
                table = new EncodeTree[(nleaf << 1) - 1];
                size = nleaf;
                lit_index = 1;
                full_index = 1;
                borrow = false;
            }

            constexpr void set_space(EncodeTree* space, std::uint16_t spsize) {
                remove();
                table = space;
                size = spsize;
                lit_index = 1;
                full_index = 1;
                borrow = true;
            }

            constexpr auto literals() const {
                return lit_index - 1;
            }

            constexpr bool add_bytes(view::rvec data) {
                for (auto c : data) {
                    if (!add_count(c)) {
                        return false;
                    }
                }
                return true;
            }

            constexpr bool add_count(std::uint32_t literal) {
                for (auto i = 1; i < lit_index; i++) {
                    if (table[i].has_value() && table[i].get_value() == literal) {
                        table[i].count++;
                        return true;
                    }
                }
                if (lit_index >= size) {
                    return false;
                }
                if (!table[lit_index].set_value(literal)) {
                    return false;
                }
                table[lit_index].count++;
                lit_index++;
                full_index++;
                return true;
            }

            bool merge() {
                auto tmp = new std::uint16_t[lit_index - 1];
                const auto d = helper::defer([&] {
                    delete[] tmp;
                });
                return merge_in_space(tmp, lit_index - 1);
            }

            constexpr bool merge_in_space(std::uint16_t* space, size_t spsize) {
                if (!space || spsize < lit_index - 1) {
                    return false;
                }
                for (auto i = 0; i < lit_index - 1; i++) {
                    space[i] = i;
                }
                auto cmp = [&](std::uint16_t a, std::uint16_t b) {
                    return table[a].count > table[b].count;
                };
                full_index = lit_index;
                auto l = lit_index - 1;
                std::make_heap(space, space + l, cmp);
                auto pop = [&] {
                    std::pop_heap(space, space + l, cmp);
                    l--;
                    auto r = space[l];
                    space[l] = 0;
                    return r;
                };
                auto push = [&](std::uint16_t a) {
                    space[l] = a;
                    l++;
                    std::push_heap(space, space + l, cmp);
                };
                auto alloc = [&](std::uint16_t a, std::uint16_t b) {
                    if (full_index > size) {
                        return false;
                    }
                    auto i = full_index;
                    if (l == 0) {
                        i = 0;
                    }
                    if (full_index == size && i != 0) {
                        return false;
                    }
                    if (!table[i].set_one(a) ||
                        !table[i].set_zero(b)) {
                        return false;
                    }
                    table[i].count = table[a].count + table[b].count;
                    if (i != 0) {
                        full_index++;
                    }
                    return true;
                };
                while (l > 1) {
                    auto a = pop();
                    auto b = pop();
                    auto c = full_index;
                    if (!alloc(a, b)) {
                        return false;
                    }
                    push(c);
                }
                return true;
            }

            constexpr const EncodeTree* get_root() const {
                if (!table) {
                    return nullptr;
                }
                return &table[0];
            }

            constexpr const EncodeTree* get_next(const EncodeTree* c, bool next) const {
                if (!table || c->has_value()) {
                    return nullptr;
                }
                if (!std::is_constant_evaluated()) {
                    if (c < table || table + size <= c) {
                        return nullptr;
                    }
                }
                auto expect = next ? c->one() : c->zero();
                if (!expect || expect >= full_index) {
                    return nullptr;
                }
                return &table[expect];
            }
        };

        template <size_t nleaf, class Tree, class Table>
        struct StaticTableBase {
           private:
            Tree place[(nleaf << 1) - 1];
            Table table_;

           public:
            constexpr StaticTableBase(auto&& fn) {
                fn(table_, place, (nleaf << 1) - 1);
                if (!table_.has_table() && std::is_constant_evaluated()) {
                    throw "error";
                }
            }

            constexpr const Table& table() const {
                return table_;
            }
        };

        template <size_t nleaf>
        using StaticDecodeTable = StaticTableBase<nleaf, DecodeTree, DecodeTable>;
        template <size_t nleaf>
        using StaticEncodeTable = StaticTableBase<nleaf, EncodeTree, EncodeTable>;

        namespace test {
            constexpr StaticEncodeTable<14> check_encode{
                [](EncodeTable& table, EncodeTree* space, size_t size) {
                    table.set_space(space, size);
                    byte lit[] = "<doctype html><html></html>";
                    table.add_bytes(view::rvec(lit, utils::strlen(lit)));
                    std::uint16_t calc[14]{};
                    if (!table.merge_in_space(calc, 14)) {
                        table = {};
                    }
                }};

        }  // namespace test

        template <std::uint16_t size, class CodeTy = Code, class Unwrap = Direct>
        struct CanonicalTable {
            std::uint16_t index = 0;
            CodeTy codes[size]{};
            std::uint16_t indices[288]{};

            constexpr Unwrap get_unwrap() const {
                return {};
            }

            constexpr void reset() {
                *this = {};
            }

            constexpr void canonization() {
                Unwrap uw;
                std::sort(codes, codes + index, [&](const auto& a, const auto& b) {
                    if (uw(a).bits == uw(b).bits) {
                        return uw(a).literal < uw(b).literal;
                    }
                    return uw(a).bits < uw(b).bits;
                });
                for (auto i = 0; i < index; i++) {
                    indices[uw(codes[i]).literal] = i;
                }
                make_canonical_code(codes, index, uw);
            }

            constexpr CodeTy* map(std::uint16_t literal) {
                if (literal > size) {
                    return nullptr;
                }
                return &codes[indices[literal]];
            }

            DecodeTable decode_table() {
                DecodeTable table;
                table.set_leaf_count(index);
                auto uw = get_unwrap();
                for (auto i = 0; i < index; i++) {
                    if (!table.set_code(uw(codes[i]))) {
                        return {};
                    }
                }
                return table;
            }

            constexpr DecodeTable static_decode_table(DecodeTree* place, size_t count) {
                DecodeTable table;
                table.set_table_place(place, count);
                auto uw = get_unwrap();
                for (auto i = 0; i < index; i++) {
                    if (!table.set_code(uw(codes[i]))) {
                        return {};
                    }
                }
                return table;
            }

           private:
            constexpr bool node_to_code(const EncodeTable& table, const EncodeTree* node, std::uint32_t bits, std::uint32_t tcode) {
                if (!node) {
                    return true;  // ignore
                }
                if (node->has_value()) {
                    if (index >= size) {
                        return false;
                    }
                    Code& code = get_unwrap()(codes[index]);
                    code.bits = bits;
                    code.literal = node->get_value();
                    code.code = tcode;
                    index++;
                    return true;
                }
                else {
                    return node_to_code(table, table.get_next(node, true), bits + 1, (tcode << 1) + 1) &&
                           node_to_code(table, table.get_next(node, false), bits + 1, tcode << 1);
                }
            }

           public:
            constexpr bool from_encode_table(const EncodeTable& table) {
                auto root = table.get_root();
                if (!root) {
                    return false;
                }
                return node_to_code(table, root, 0, 0);
            }
        };

        namespace test {
            constexpr auto check_canonnical_table() {
                CanonicalTable<14> table;
                if (!table.from_encode_table(check_encode.table())) {
                    throw "why";
                }
                table.canonization();
                return table;
            }

            constexpr auto canonical_table = check_canonnical_table();
        }  // namespace test

    }  // namespace file::gzip::huffman
}  // namespace utils
