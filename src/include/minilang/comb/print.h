/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "nullctx.h"
#include "branch_table.h"
#include "comb.h"
#include "../../escape/escape.h"

namespace utils::minilang::comb {
    struct PrintSet {
        bool token = true;
        bool ident = true;
        bool group = true;
        bool branch = true;
    };

    constexpr auto print_none = PrintSet{false, false, false, false};

    template <class Cout>
    struct PrintCtx : CounterCtx {
        Cout cout;
        branch::BranchTable br;
        PrintSet set;

        PrintCtx(Cout c)
            : cout(c) {}

        void branch(CombMode mode, std::uint64_t* id) {
            CounterCtx::branch(mode, id);
            if (set.branch) {
                cout << to_string(mode) << ": id=" << *id << "\n";
            }
            br.branch(mode, *id);
        }

        void commit(CombMode mode, std::uint64_t id, CombState st) {
            if (set.branch) {
                cout << to_string(mode) << ": " << to_string(st) << " id=" << id << "\n";
            }
            br.commit(mode, id, st);
        }

        void group(auto tag, std::uint64_t* id) {
            CounterCtx::group(tag, id);
            if (set.group) {
                cout << "group: " << convert_tag_to_string(tag) << " id=" << *id << "\n";
            }
            br.group(tag, *id);
        }

        void group_end(auto tag, std::uint64_t id, CombState st) {
            if (set.group) {
                cout << "group: " << to_string(st) << " " << convert_tag_to_string(tag) << " id=" << id << "\n";
            }
            br.group_end(tag, id, st);
        }

        void ctoken(const char* tok, Pos pos) {
            std::string str;
            escape::escape_str(tok, str, escape::EscapeFlag::utf | escape::EscapeFlag::hex);
            if (set.token) {
                cout << "token: \"" << str << "\"\n";
            }
            br.ctoken(tok, pos);
        }

        template <class T>
        constexpr void crange(auto tag, Pos pos, utils::Sequencer<T>& seq) {
            std::string str, esc;
            for (auto i = pos.begin; i < pos.end; i++) {
                str.push_back(seq.buf.buffer[i]);
            }
            escape::escape_str(str, esc, escape::EscapeFlag::utf | escape::EscapeFlag::hex);
            if (set.ident) {
                cout << "ident: " << convert_tag_to_string(tag) << " \"" << str << "\"\n";
            }
            br.crange(tag, str, pos);
        }
    };

}  // namespace utils::minilang::comb
