/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "grammar.h"
struct TerminalSymbol {};

struct BottomUpTable;

using BottomUpGrammar = futils::comb2::types::TypeErased<futils::Sequencer<std::string_view>&, Table&, BottomUpTable&>;

static auto wrap(auto&& x) {
    return futils::comb2::ops::type_erased<futils::Sequencer<std::string_view>&, Table&, BottomUpTable&>(std::forward<decltype(x)>(x));
}

struct BottomUpTable {
    Description table_desc;
    size_t id = 0;
    std::map<std::string, size_t> name_to_id;
    std::map<size_t, std::string> id_to_name;
    std::map<size_t, BottomUpGrammar> terminal_symbols;

    void add_terminal(const std::string& name, BottomUpGrammar&& g) {
        if (name_to_id.contains(name)) {
            return;
        }
        name_to_id[name] = id;
        id_to_name[id] = name;
        terminal_symbols[id] = std::move(g);
        id++;
    }

    std::string add_anonymous_terminal(BottomUpGrammar&& g) {
        std::string name = "<anon_term_" + std::to_string(id) + ">";
        add_terminal(name, std::move(g));
        return name;
    }

    BottomUpGrammar* find_terminal(const std::string& name) {
        if (!name_to_id.contains(name)) {
            return nullptr;
        }
        size_t id = name_to_id[name];
        if (!terminal_symbols.contains(id)) {
            return nullptr;
        }
        return &terminal_symbols[id];
    }
};

auto handle_ident_bottom(const std::string& name) {
    return grammar::proxy(
        [name = name](auto&& seq, auto&& ctx, BottomUpTable& rec) -> futils::comb2::Status {
            auto found = rec.find_terminal(name);
            if (!found) {
                futils::comb2::ctxs::context_error(seq, ctx, "undefined reference to terminal symbol: ", name);
                return futils::comb2::Status::fatal;
            }
            return (*found)(seq, ctx, rec);
        },
        [name = name](auto&&, auto&& seq, auto&& ctx, TopdownTable& rec) {
            if (auto found = rec.table.find(name); found == rec.table.end()) {
                futils::comb2::ctxs::context_error(seq, ctx, "undefined reference to rule: ", name);
                return;
            }
            else {
                futils::comb2::ctxs::context_call_must_match_error(seq, ctx, found->second, rec);
            }
        });
}

std::optional<BottomUpGrammar> analyze_token_body(BottomUpTable& table, const std::shared_ptr<Node>& node) {
    switch (node->tag) {
        case grammar::NodeKind::literal: {
            auto tok = as_tok<grammar::NodeKind>(node);
            if (!tok) {
                cerr << "cmb2parse: error: failed to analyze definition body for literal\n";
                return std::nullopt;
            }
            auto token = tok->token;
            token.erase(0, 1);
            token.pop_back();
            token = futils::escape::unescape_str<std::string>(token);
            auto t = table.add_anonymous_terminal(wrap(futils::comb2::ops::lit(std::move(token))));
            return wrap(handle_ident_bottom(t));
        }
        case grammar::NodeKind::ident: {
            auto tok = as_tok<grammar::NodeKind>(node);
            if (!tok) {
                cerr << "cmb2parse: error: failed to analyze definition body for identifier\n";
                return std::nullopt;
            }
            return wrap(handle_ident_bottom(tok->token));
        }
        case grammar::NodeKind::group: {
            auto group = as_group<grammar::NodeKind>(node);
            if (!group) {
                cerr << "cmb2parse: error: failed to analyze definition body for group\n";
                return std::nullopt;
            }
            if (group->children.size() != 1) {
                cerr << "cmb2parse: error: group must have exactly one child\n";
                return std::nullopt;
            }
            return analyze_token_body(table, group->children[0]);
        }
        case grammar::NodeKind::range_group: {
            auto group = as_group<grammar::NodeKind>(node);
            if (!group) {
                return std::nullopt;
            }
            if (group->children.size() == 0) {
                cerr << "cmb2parse: error: range group must have at least one child\n";
                return std::nullopt;
            }
            BottomUpGrammar body;
            for (auto& child : group->children) {
                auto r = as_tok<grammar::NodeKind>(child);
                if (!r || r->tag != grammar::NodeKind::range) {
                    cerr << "cmb2parse: error: range group's child is not range\n";
                    return std::nullopt;
                }
                if (r->token.size() == 1) {
                    auto c = r->token[0];
                    auto lit = futils::comb2::ops::lit(c);
                    if (body) {
                        body = wrap(futils::comb2::ops::or_(std::move(body), lit));
                    }
                    else {
                        body = wrap(lit);
                    }
                }
                else if (r->token.size() == 3 && r->token[1] == '-') {
                    auto start = r->token[0];
                    auto end = r->token[2];
                    if (start > end) {
                        cerr << "cmb2parse: error: invalid range: " << r->token << "\n";
                        return std::nullopt;
                    }
                    auto range = futils::comb2::ops::range(start, end);
                    if (body) {
                        body = wrap(futils::comb2::ops::or_(std::move(body), std::move(range)));
                    }
                    else {
                        body = wrap(range);
                    }
                }
                else {
                    cerr << "cmb2parse: error: invalid range: " << r->token << "\n";
                    return std::nullopt;
                }
            }
            auto t = table.add_anonymous_terminal(std::move(body));
            return wrap(handle_ident_bottom(t));
        }
        case grammar::NodeKind::primary: {
            auto group = as_group<grammar::NodeKind>(node);
            if (!group) {
                return std::nullopt;
            }
            if (group->children.size() != 1 && group->children.size() != 2) {
                cerr << "cmb2parse: error: primary must have one or two children\n";
                return std::nullopt;
            }
            auto body = analyze_token_body(table, group->children[0]);
            if (!body) {
                return std::nullopt;
            }
            if (group->children.size() == 2) {
                auto tok = as_tok<grammar::NodeKind>(group->children[1]);
                if (!tok || tok->tag != grammar::NodeKind::token) {
                    cerr << "cmb2parse: error: primary's second child is not token\n";
                    return std::nullopt;
                }
                if (tok->token == "+") {
                    body = wrap(futils::comb2::ops::repeat(std::move(*body)));
                }
                else if (tok->token == "*") {
                    body = wrap(futils::comb2::ops::optional_repeat(std::move(*body)));
                }
                else if (tok->token == "?") {
                    body = wrap(futils::comb2::ops::optional(std::move(*body)));
                }
                else if (tok->token == "!") {
                    body = wrap(futils::comb2::ops::must_match(std::move(*body)));
                }
                else if (tok->token == "+!") {
                    body = wrap(futils::comb2::ops::must_match(futils::comb2::ops::repeat(std::move(*body))));
                }
                else if (tok->token == "^") {
                    body = wrap(futils::comb2::ops::peek(std::move(*body)));
                }
                else if (tok->token == "~") {
                    body = wrap(futils::comb2::ops::not_(std::move(*body)));
                }
                else {
                    cerr << "cmb2parse: error: unknown token: " << tok->token << "\n";
                    return std::nullopt;
                }
            }
            return wrap(*body);
        }
        case grammar::NodeKind::sequence: {
            auto group = as_group<grammar::NodeKind>(node);
            if (!group) {
                return std::nullopt;
            }
            if (group->children.size() == 0) {
                cerr << "cmb2parse: error: sequence must have at least one child\n";
                return std::nullopt;
            }
            BottomUpGrammar body;
            size_t index = 0;
            for (auto& child : group->children) {
                auto part = analyze_token_body(table, child);
                if (!part) {
                    return std::nullopt;
                }
                if (body) {
                    body = wrap(futils::comb2::ops::and_(std::move(body), std::move(*part)));
                }
                else {
                    body = wrap(std::move(*part));
                }
                index++;
            }
            return body;
        }
        case grammar::NodeKind::ordered_choice: {
            auto group = as_group<grammar::NodeKind>(node);
            if (!group) {
                return std::nullopt;
            }
            if (group->children.size() == 0) {
                cerr << "cmb2parse: error: ordered_choice must have at least one child\n";
                return std::nullopt;
            }
            BottomUpGrammar body;
            size_t index = 0;
            for (auto& child : group->children) {
                auto part = analyze_token_body(table, child);
                if (!part) {
                    return std::nullopt;
                }
                if (body) {
                    body = wrap(futils::comb2::ops::or_(std::move(body), std::move(*part)));
                }
                else {
                    body = wrap(std::move(*part));
                }
                index++;
            }
            return body;
        }
        default:
            cerr << "cmb2parse: error: unsupported node kind in definition body\n";
            return std::nullopt;
    }
}