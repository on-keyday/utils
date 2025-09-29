/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <comb2/composite/string.h>
#include <comb2/composite/range.h>
#include <comb2/basic/group.h>
#include <comb2/dynamic/dynamic.h>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include "code/src_location.h"
#include "comb2/basic/literal.h"
#include "comb2/basic/logic.h"
#include "comb2/basic/proxy.h"
#include "comb2/internal/context_fn.h"
#include "comb2/status.h"
#include "comb2/tree/simple_node.h"
#include "core/sequencer.h"
#include "escape/escape.h"
#include "helper/defer.h"
#include "helper/defer_ex.h"
#include <optional>
#include <wrap/cin.h>
#include "grammar.h"
#include "json/stringer.h"

static auto wrap(auto&& x) {
    return futils::comb2::ops::type_erased<futils::Sequencer<std::string_view>&, Table&, TopdownTable&>(std::forward<decltype(x)>(x));
}
using futils::comb2::tree::node::as_group, futils::comb2::tree::node::as_tok;

auto with_auto_rule(auto&& g) {
    return futils::comb2::ops::proxy(
        [g = std::forward<decltype(g)>(g)](auto&& seq, auto&& ctx, TopdownTable& rec) -> futils::comb2::Status {
            auto res = g(seq, ctx, rec);
            if (res == futils::comb2::Status::match) {
                if (rec.inner_atomic_rules) {
                    return res;
                }
                rec.inner_atomic_rules = true;
                auto d = futils::helper::defer([&] {
                    rec.inner_atomic_rules = false;
                });
                for (auto& name : rec.table_desc.auto_rules) {
                    if (auto found = rec.table.find(name); found == rec.table.end()) {
                        futils::comb2::ctxs::context_error(seq, ctx, "undefined reference to auto rule: ", name);
                        return futils::comb2::Status::fatal;
                    }
                    else {
                        auto r = found->second(seq, ctx, rec);
                        if (r != futils::comb2::Status::match) {
                            return r;
                        }
                    }
                }
            }
            return res;
        },
        [](auto&& g, auto&& seq, auto&& ctx, auto&& rec) {
            futils::comb2::ctxs::context_call_must_match_error(seq, ctx, g, rec);
        });
}

// this is heuristic left recursion handling
// I saw https://web.archive.org/web/20110527032954/http://acl.ldc.upenn.edu/W/W07/W07-2215.pdf
// but I could not fully understand it. so I implemented my own version.
// it may not work in some cases.
/*
auto left_recursion(auto&& g) {
    return futils::comb2::ops::proxy(
        [g = std::forward<decltype(g)>(g)](auto&& seq, auto&& ctx, RecursionTable& rec) -> futils::comb2::Status {
            // current copy
            auto prev = rec.left_recursion_detected_calls;
            futils::comb2::ctxs::context_logic_entry(ctx, futils::comb2::CallbackType::branch_entry);
            const auto original_ptr = seq.rptr;
            auto res = g(seq, ctx, rec);
            if (res == futils::comb2::Status::fatal) {
                return res;
            }
            // if left recursion and forward progress, retrying
            auto max_pos = rec.max_pos;
            size_t tries_same = 0;
            while (res == futils::comb2::Status::not_match && !rec.inner_atomic_rules && prev.size() < rec.left_recursion_detected_calls.size() && !seq.eos() && tries_same < rec.tried_threshold && !rec.threshold_reached) {
                futils::comb2::ctxs::context_logic_result(ctx, futils::comb2::CallbackType::branch_other, res);
                futils::comb2::ctxs::context_logic_entry(ctx, futils::comb2::CallbackType::branch_other);
                seq.rptr = original_ptr;
                res = g(seq, ctx, rec);
                if (res == futils::comb2::Status::fatal) {
                    return res;
                }
                if (res == futils::comb2::Status::match) {
                    break;
                }
                if (rec.max_pos > max_pos) {
                    max_pos = rec.max_pos;
                    tries_same = 0;
                }
                else {
                    tries_same++;
                }
            }
            if (tries_same >= rec.tried_threshold) {
                rec.threshold_reached = true;
            }
            rec.left_recursion_detected_calls = std::move(prev);
            futils::comb2::ctxs::context_logic_result(ctx, futils::comb2::CallbackType::branch_result, res);
            return res;
        },
        [](auto&& g, auto&& seq, auto&& ctx, auto&& rec) {
            futils::comb2::ctxs::context_call_must_match_error(seq, ctx, g, rec);
        });
}
*/

auto with_eof_check(auto&& g) {
    return futils::comb2::ops::proxy(
        [g = std::forward<decltype(g)>(g)](auto&& seq, auto&& ctx, TopdownTable& rec) -> futils::comb2::Status {
            auto res = g(seq, ctx, rec);
            // rec.max_pos = std::max(rec.max_pos, size_t(seq.rptr));
            return res;
        },
        [](auto&& g, auto&& seq, auto&& ctx, auto&& rec) {
            futils::comb2::ctxs::context_call_must_match_error(seq, ctx, g, rec);
        });
}

auto with_indexed(grammar::NodeKind kind, size_t index, size_t max_index, auto&& g) {
    return futils::comb2::ops::proxy(
        [g = std::forward<decltype(g)>(g), kind, index, max_index](auto&& seq, auto&& ctx, TopdownTable& rec) -> futils::comb2::Status {
            rec.call_stack.push({seq.rptr, KindFrame{kind, index, max_index}});
            const auto d = futils::helper::defer([&] {
                rec.call_stack.pop();
            });
            return g(seq, ctx, rec);
        },
        [](auto&& g, auto&& seq, auto&& ctx, auto&& rec) {
            futils::comb2::ctxs::context_call_must_match_error(seq, ctx, g, rec);
        });
}

auto handle_ident(const std::string& name) {
    return grammar::proxy(
        [name = name](auto&& seq, auto&& ctx, TopdownTable& rec) -> futils::comb2::Status {
            auto it = rec.table.find(name);
            if (it == rec.table.end()) {
                return futils::comb2::Status::fatal;
            }
            auto recursion_type = rec.call_stack.check_recursion(name, seq.rptr);
            if (recursion_type != RecursionType::none) {
                futils::comb2::ctxs::context_error(seq, ctx, "left recursion detected for rule: ", name);
                return futils::comb2::Status::fatal;
            }
            rec.call_stack.push({seq.rptr, name});
            auto d = futils::helper::defer([&] {
                rec.call_stack.pop();
            });
            auto call = [&] {
                futils::comb2::Status res;
                futils::helper::DynDefer dd;
                if (!rec.table_desc.auto_rules.contains(name) && !rec.inner_atomic_rules) {
                    cout << "cmb2parse: debug: calling rule " << name << " at pos " << seq.rptr << "\n";
                    dd = futils::helper::defer_ex([&] {
                        cout << "cmb2parse: debug: returned from rule " << name << " at pos " << seq.rptr << " with status " << (int)res << "\n";
                    });
                }
                if (rec.table_desc.tokens.contains(name)) {
                    auto old = rec.inner_atomic_rules;
                    rec.inner_atomic_rules = true;
                    auto d = futils::helper::defer([&] {
                        rec.inner_atomic_rules = old;
                    });
                    res = it->second(seq, ctx, rec);
                }
                else {
                    res = it->second(seq, ctx, rec);
                }
                return res;
            };
            return call();
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

std::optional<TopdownGrammar> analyze_definition_body(const std::shared_ptr<Node>& node) {
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
            return wrap(with_eof_check(futils::comb2::ops::lit(std::move(token))));
        }
        case grammar::NodeKind::ident: {
            auto tok = as_tok<grammar::NodeKind>(node);
            if (!tok) {
                cerr << "cmb2parse: error: failed to analyze definition body for identifier\n";
                return std::nullopt;
            }
            return wrap(handle_ident(tok->token));
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
            return analyze_definition_body(group->children[0]);
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
            TopdownGrammar body;
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
            return wrap(with_auto_rule(std::move(body)));
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
            auto body = analyze_definition_body(group->children[0]);
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
            return wrap(with_auto_rule(std::move(*body)));
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
            TopdownGrammar body;
            size_t index = 0;
            for (auto& child : group->children) {
                auto part = analyze_definition_body(child);
                if (!part) {
                    return std::nullopt;
                }
                if (group->children.size() == 1) {
                    return part;  // no need to index if only one child
                }
                auto indexed = with_indexed(grammar::NodeKind::sequence, index, group->children.size() - 1, std::move(*part));
                if (body) {
                    body = wrap(futils::comb2::ops::and_(std::move(body), std::move(indexed)));
                }
                else {
                    body = wrap(std::move(indexed));
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
            TopdownGrammar body;
            size_t index = 0;
            for (auto& child : group->children) {
                auto part = analyze_definition_body(child);
                if (!part) {
                    return std::nullopt;
                }
                if (group->children.size() == 1) {
                    return part;  // no need to index if only one child
                }
                auto indexed = with_indexed(grammar::NodeKind::ordered_choice, index, group->children.size() - 1, std::move(*part));
                if (body) {
                    body = wrap(futils::comb2::ops::or_(std::move(body), std::move(indexed)));
                }
                else {
                    body = wrap(std::move(indexed));
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

bool analyze_definition(TopdownTable& table, const std::shared_ptr<Node>& node) {
    auto def = as_group<grammar::NodeKind>(node);
    if (!def || def->tag != grammar::NodeKind::definition) {
        cerr << "cmb2parse: error: definition node is not definition\n";
        return false;
    }
    if (def->children.size() != 2) {
        cerr << "cmb2parse: error: definition node does not have exactly two children\n";
        return false;
    }
    auto id = as_tok<grammar::NodeKind>(def->children[0]);
    if (!id || id->tag != grammar::NodeKind::ident) {
        cerr << "cmb2parse: error: definition node's first child is not ident\n";
        return false;
    }
    if (table.table.find(id->token) != table.table.end()) {
        cerr << "cmb2parse: error: duplicate definition for rule: " << id->token << "\n";
        return false;
    }
    auto body = as_group<grammar::NodeKind>(def->children[1]);
    if (!body || body->tag != grammar::NodeKind::ordered_choice) {
        cerr << "cmb2parse: error: definition node's second child is not ordered_choice\n";
        return false;
    }
    auto g = analyze_definition_body(def->children[1]);
    if (!g) {
        cerr << "cmb2parse: error: failed to analyze definition body for rule: " << id->token << "\n";
        return false;
    }
    table.table[id->token] = wrap(futils::comb2::ops::and_(with_auto_rule(futils::comb2::ops::null), std::move(*g)));
    return true;
}

auto eof() {
    return futils::comb2::ops::proxy(
        [](auto&& seq, auto&& ctx, TopdownTable& rec) {
            if (seq.eos()) {
                // rec.max_pos = std::max(rec.max_pos, size_t(seq.rptr));
                return futils::comb2::Status::match;
            }
            return futils::comb2::Status::not_match;
        },
        [](auto&&, auto&& seq, auto&& ctx, auto&&) {
            if (!seq.eos()) {
                futils::comb2::ctxs::context_error(seq, ctx, "expected end of file");
            }
        });
}

std::optional<TopdownTable> convert_topdown(Description&& desc) {
    TopdownTable table;
    table.table_desc = std::move(desc);
    table.table.clear();
    table.table["eof"] = wrap(eof());  // predefined rule
    for (auto& [name, child] : desc.definitions) {
        auto body = analyze_definition_body(child);
        if (!body) {
            return std::nullopt;
        }
        table.table[name] = std::move(*body);
    }
    return table;
}

int do_topdown_parse(TopdownTable& rtable, std::string_view input, std::string_view input_file, bool as_json) {
    auto seq = futils::make_cpy_seq(input);
    Table ctx_table;
    auto res = handle_ident(rtable.table_desc.root_name)(seq, ctx_table, rtable);
    rtable.reset_parse_state();
    if (res != futils::comb2::Status::match) {
        std::string out;
        auto src_loc = futils::code::write_src_loc(out, seq);
        cerr << "cmb2parse: error: failed to parse input\n";
        if (!ctx_table.buffer.empty()) {
            cerr << ctx_table.buffer << "\n";
        }
        cerr << input_file << ":" << src_loc.line + 1 << ":" << src_loc.pos + 1 << "\n";
        cerr << out << "\n";
        return -1;
    }
    auto root = futils::comb2::tree::node::collect<std::string>(ctx_table.root_branch);
    if (!root) {
        cerr << "cmb2parse: error: failed to collect parse tree\n";
        return -1;
    }
    if (as_json) {
        futils::json::Stringer<> w;
        w.set_indent("  ");
        print_json_tree(rtable.table_desc, w, root);
        cout << w.out() << "\n";
        return 0;
    }
    CodeWriter w;
    print_tree(rtable.table_desc, w, root);
    cout << w.out();
    return 0;
}