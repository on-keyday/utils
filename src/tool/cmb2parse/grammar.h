/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <comb2/composite/range.h>
#include <comb2/basic/group.h>
#include <comb2/composite/string.h>
#include <comb2/tree/branch_table.h>
#include <comb2/dynamic/dynamic.h>
#include <comb2/tree/simple_node.h>
#include "code/code_writer.h"
#include "json/stringer.h"
#include "number/to_string.h"
#include <memory>
#include <optional>
#include <variant>
#include <binary/writer.h>
#include <binary/number.h>
#include <map>
#include <set>
#include <wrap/cout.h>

namespace grammar {
    using namespace futils::comb2::ops;
    namespace cps = futils::comb2::composite;
    constexpr auto space = cps::space | cps::tab;
    constexpr auto spaces = *space;
    constexpr auto new_line = cps::eol;
    constexpr auto space_lines = *(space | new_line);

    enum class NodeKind {
        root,
        ident,
        definition,
        literal,
        group,
        token,
        primary,
        sequence,
        ordered_choice,
        range,
        range_group,

        token_definition,
        group_definition,
        root_definition,
        omit_if_one_definition,
        auto_definition,
    };

    constexpr const char* to_string(NodeKind kind) {
        switch (kind) {
            case NodeKind::root:
                return "root";
            case NodeKind::ident:
                return "ident";
            case NodeKind::definition:
                return "definition";
            case NodeKind::literal:
                return "literal";
            case NodeKind::group:
                return "group";
            case NodeKind::token:
                return "token";
            case NodeKind::primary:
                return "primary";
            case NodeKind::sequence:
                return "sequence";
            case NodeKind::ordered_choice:
                return "ordered_choice";
            case NodeKind::range:
                return "range";
            case NodeKind::range_group:
                return "range_group";
            case NodeKind::token_definition:
                return "token_definition";
            case NodeKind::group_definition:
                return "group_definition";
            case NodeKind::root_definition:
                return "root_definition";
            case NodeKind::omit_if_one_definition:
                return "omit_if_one_definition";
            case NodeKind::auto_definition:
                return "auto_definition";
            default:
                return "unknown";
        }
    }

    // C++ comb2 expression for the grammar:
    // + means force match
    // * means repeat 0 or more (syntax sugar for -~(x))
    // | means ordered choice
    // & means sequence
    // ~ means repeat 1 or more
    // - means optional
    // uany means any single character
    // not_ means negative lookahead
    constexpr auto ident = str(NodeKind::ident, cps::c_ident);
    constexpr auto literal = str(NodeKind::literal, cps::c_str | cps::char_str);
    constexpr auto repeat_token = lit("+");
    constexpr auto optional_repeat_token = lit("*");
    constexpr auto optional_token = lit("?");
    constexpr auto force_error_token = lit("!");
    constexpr auto peek_token = lit("^");
    constexpr auto not_token = lit("~");
    constexpr auto definition_body = method_proxy(body);
    constexpr auto range = str(NodeKind::range, uany & -('-'_l & +uany));
    constexpr auto range_group = group(NodeKind::range_group, '['_l & range & *(not_(']'_l) & range) & +']'_l);
    constexpr auto definition = group(NodeKind::definition, ident& spaces & +'='_l & spaces & definition_body & spaces & +(new_line | eos));
    constexpr auto group_ = group(NodeKind::group, '('_l & spaces & definition_body & spaces & +')'_l);
    constexpr auto primary = literal | ident | range_group | group_;
    constexpr auto postfix_token = str(NodeKind::token, (force_error_token | repeat_token & -(force_error_token) | peek_token | not_token | optional_repeat_token | optional_token));
    constexpr auto postfix = group(NodeKind::primary, primary & -(spaces & postfix_token));
    constexpr auto sequence = group(NodeKind::sequence, postfix&*(spaces& postfix));
    constexpr auto ordered_choice = group(NodeKind::ordered_choice, sequence&*(spaces & '/'_l & spaces & sequence));
    constexpr auto some_ident_and_line = ~(spaces & ident) & spaces & +(new_line | eos);
    constexpr auto single_ident_and_line = spaces & +ident & spaces & +(new_line | eos);
    constexpr auto token_definition = group(NodeKind::token_definition, lit("token!") & some_ident_and_line);
    constexpr auto group_definition = group(NodeKind::group_definition, lit("group!") & some_ident_and_line);
    constexpr auto omit_if_one = group(NodeKind::omit_if_one_definition, lit("omit_one!") & some_ident_and_line);
    constexpr auto auto_definition = group(NodeKind::auto_definition, lit("auto!") & some_ident_and_line);
    constexpr auto root_definition = group(NodeKind::root_definition, lit("root!") & single_ident_and_line);
    constexpr auto body = token_definition | group_definition | root_definition | omit_if_one | auto_definition | definition;
    constexpr auto root = space_lines & *(body & space_lines) & +eos;

    struct Recurse {
        decltype(ordered_choice) body;
    };
    constexpr Recurse recurse{ordered_choice};
    constexpr auto parse = [](auto&& seq, auto&& ctx) {
        return root(seq, ctx, recurse);
    };

    constexpr bool test() {
        auto seq = futils::make_ref_seq(R"(
            expr = term (('+' / '-') term)*
            term = factor (('*' / '/') factor)*
            factor = ident / number / '(' expr ')'
            ident = [a-zA-Z_][a-zA-Z0-9_]*
            number = [0-9]+
            force_match = ident!
            force_repeat = ident+!
            repeat = ident+
            optional_repeat = ident*
            optional = ident?
            token! ident 
            root! expr
            group! paren
            omit_one! single
            peek = ident^
            not = ident~
            auto! auto_rule
        )");
        return parse(seq, futils::comb2::test::TestContext{}) == futils::comb2::Status::match;
    }

    static_assert(test());

}  // namespace grammar

struct Table : futils::comb2::tree::BranchTable {
    std::string buffer;
    void error(auto&&... err) {
        auto each_arg = [&](auto&& arg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, char>) {
                char buf[2] = {arg, 0};
                futils::strutil::appends(buffer, buf);
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, const char*>) {
                futils::strutil::appends(buffer, arg);
            }
            else if constexpr (std::is_integral_v<std::decay_t<decltype(arg)>>) {
                futils::number::to_string(buffer, arg);
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, grammar::NodeKind>) {
                futils::strutil::appends(buffer, grammar::to_string(arg));
            }
            else if constexpr (std::is_convertible_v<std::decay_t<decltype(arg)>, std::string_view>) {
                futils::strutil::appends(buffer, arg);
            }
            else {
                static_assert(std::is_void_v<decltype(arg)>, "unsupported type in error");
            }
        };
        (each_arg(err), ...);
    }
};

using Node = futils::comb2::tree::node::GenericNode<grammar::NodeKind>;
struct Description {
    std::vector<std::string> definition_order;
    std::map<std::string, std::shared_ptr<Node>> definitions;
    std::string root_name;
    std::set<std::string> tokens;
    std::set<std::string> groups;
    std::set<std::string> omit_if_one;
    std::set<std::string> auto_rules;
};

bool analyze_description(Description& table, const std::shared_ptr<Node>& node);

struct TopdownTable;
using TopdownGrammar = futils::comb2::types::TypeErased<futils::Sequencer<std::string_view>&, Table&, TopdownTable&>;

struct KindFrame {
    grammar::NodeKind kind;
    size_t index;
    size_t max_index;
};

struct StackFrame {
    size_t pos;
    std::variant<std::string, KindFrame> location;

    bool is_named() const {
        return std::holds_alternative<std::string>(location);
    }

    bool is_named(const std::string& name) const {
        if (auto p = std::get_if<std::string>(&location); p) {
            return *p == name;
        }
        return false;
    }

    bool is_kind(grammar::NodeKind kind) const {
        if (auto p = std::get_if<KindFrame>(&location); p) {
            return p->kind == kind;
        }
        return false;
    }

    void write(futils::binary::writer& w) const {
        futils::number::to_string(w, pos);
        w.write(":");
        if (auto p = std::get_if<std::string>(&location); p) {
            w.write(*p);
        }
        else if (auto p = std::get_if<KindFrame>(&location); p) {
            w.write(grammar::to_string(p->kind));
            w.write(":");
            futils::number::to_string(w, p->index);
            w.write("/");
            futils::number::to_string(w, p->max_index);
        }
    }
};

struct LeftRecursionContext {
    size_t current_depth = 0;
    size_t trying_depth = 0;
};

enum class RecursionType {
    none,
    left,
    infinity,
};

struct CallStack {
    std::vector<StackFrame> call_stack;
    bool empty() const {
        return call_stack.empty();
    }
    void push(StackFrame&& v) {
        call_stack.push_back(std::move(v));
    }

    void pop() {
        if (!call_stack.empty()) {
            call_stack.pop_back();
        }
    }

    RecursionType check_recursion(const std::string& name, size_t pos) const {
        bool has_ordered_choice = false;
        for (auto it = call_stack.rbegin(); it != call_stack.rend(); it++) {
            if (it->is_named(name)) {
                if (pos > it->pos) {
                    return RecursionType::none;
                }
                return has_ordered_choice ? RecursionType::left : RecursionType::infinity;
            }
            if (it->is_kind(grammar::NodeKind::ordered_choice)) {
                has_ordered_choice = true;
            }
        }
        return RecursionType::none;
    }

    std::string hash() const {  // for internal use only
        std::string buf;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buf};
        if (call_stack.empty()) {
            futils::binary::write_num(w, std::uint64_t(0));
        }
        else {
            // first find most recent named function excluding last
            futils::number::to_string(w, std::uint64_t(call_stack.size()));
            size_t start = call_stack.size() - 2;
            for (; start != (size_t)-1; start--) {
                if (call_stack[start].is_named()) {
                    break;
                }
            }
            for (size_t i = start; i < call_stack.size(); i++) {
                if (w.offset() != 0) {
                    w.write(";");
                }
                call_stack[i].write(w);
            }
            futils::binary::write_num(w, std::uint64_t(call_stack.back().pos));
        }
        return buf;
    }

    std::uint64_t pos_from_hash(const std::string& hash) const {  // for internal use only
        if (hash.size() < 8) {
            return 0;
        }
        auto ptr = reinterpret_cast<const std::uint8_t*>(hash.data());
        futils::binary::reader r{futils::view::rvec(ptr, hash.size())};
        r.offset(hash.size() - 8);
        std::uint64_t res = 0;
        if (!futils::binary::read_num(r, res)) {
            return 0;
        }
        return res;
    }
};

struct TopdownTable {
    Description table_desc;
    std::map<std::string, TopdownGrammar> table;
    bool inner_atomic_rules = false;
    CallStack call_stack;

    // std::map<std::string, LeftRecursionContext> left_recursion_contexts;
    // std::set<std::string> left_recursion_detected;

    /*
    std::map<std::string, std::optional<futils::comb2::Status>> left_recursion_detected_calls;
    size_t max_pos = 0;
    size_t tried_threshold = 10;
    bool threshold_reached = false;
    */

    void reset_parse_state() {
        call_stack.call_stack.clear();
        // left_recursion_contexts.clear();
        /*
        left_recursion_detected_calls.clear();
        max_pos = 0;
        threshold_reached = false;
        */
    }
};

inline auto& cout = futils::wrap::cout_wrap();
inline auto& cerr = futils::wrap::cerr_wrap();
std::optional<TopdownTable> convert_topdown(Description&& desc);

using CodeWriter = futils::code::CodeWriter<std::string>;
using ResultNode = futils::comb2::tree::node::GenericNode<std::string>;
void print_tree(Description& table, CodeWriter& w, const std::shared_ptr<ResultNode>& node);
void print_json_tree(Description& table, futils::json::Stringer<>& w, const std::shared_ptr<ResultNode>& node);
int do_topdown_parse(TopdownTable& rtable, std::string_view input, std::string_view input_file, bool as_json);
