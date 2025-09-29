#include "grammar.h"
#include <json/stringer.h>

bool analyze_description(Description& table, const std::shared_ptr<Node>& node) {
    auto group = as_group<grammar::NodeKind>(node);
    if (!group || group->tag != grammar::NodeKind::root) {
        cerr << "cmb2parse: error: root node is not root\n";
        return false;
    }
    std::set<std::string> token_candidates;
    std::set<std::string> group_candidates;
    std::string root_name;
    std::set<std::string> omit_if_one_candidates;
    std::set<std::string> auto_rules;
    for (auto& child : group->children) {
        if (child->tag == grammar::NodeKind::definition) {
            auto child_def = as_group<grammar::NodeKind>(child);
            if (!child_def || child_def->children.size() < 2) {
                cerr << "cmb2parse: error: failed to analyze definition node\n";
                return false;
            }
            auto id = as_tok<grammar::NodeKind>(child_def->children[0]);
            if (!id || id->tag != grammar::NodeKind::ident) {
                cerr << "cmb2parse: error: definition node's first child is not ident\n";
                return false;
            }
            if (table.definitions.contains(id->token)) {
                cerr << "cmb2parse: error: duplicate definition for rule: " << id->token << "\n";
                return false;
            }
            table.definition_order.push_back(id->token);
            table.definitions[id->token] = child;
        }
        else if (
            child->tag == grammar::NodeKind::omit_if_one_definition ||
            child->tag == grammar::NodeKind::token_definition ||
            child->tag == grammar::NodeKind::group_definition ||
            child->tag == grammar::NodeKind::root_definition ||
            child->tag == grammar::NodeKind::auto_definition) {
            auto def = as_group<grammar::NodeKind>(child);
            if (!def) {
                cerr << "cmb2parse: error: failed to analyze definition body for group\n";
                return false;
            }
            if (def->tag == grammar::NodeKind::root_definition) {
                if (def->children.size() != 1) {
                    cerr << "cmb2parse: error: root_definition node does not have exactly one child\n";
                    return false;
                }
            }
            for (auto& child2 : def->children) {
                auto id = as_tok<grammar::NodeKind>(child2);
                if (!id || id->tag != grammar::NodeKind::ident) {
                    cerr << "cmb2parse: error: token_definition node's child is not ident\n";
                    return false;
                }
                if (child->tag == grammar::NodeKind::omit_if_one_definition) {
                    if (!omit_if_one_candidates.insert(id->token).second) {
                        cerr << "cmb2parse: error: duplicate omit_if_one definition for rule: " << id->token << "\n";
                        return false;
                    }
                }
                else if (child->tag == grammar::NodeKind::auto_definition) {
                    if (!auto_rules.insert(id->token).second) {
                        cerr << "cmb2parse: error: duplicate auto definition for rule: " << id->token << "\n";
                        return false;
                    }
                }
                else if (child->tag == grammar::NodeKind::root_definition) {
                    if (!root_name.empty()) {
                        cerr << "cmb2parse: error: multiple root definitions\n";
                        return false;
                    }
                    root_name = id->token;
                }
                else if (child->tag == grammar::NodeKind::token_definition) {
                    if (group_candidates.find(id->token) != group_candidates.end()) {
                        cerr << "cmb2parse: error: token definition conflicts with group definition for rule: " << id->token << "\n";
                        return false;
                    }
                    if (!token_candidates.insert(id->token).second) {
                        cerr << "cmb2parse: error: duplicate token definition for rule: " << id->token << "\n";
                        return false;
                    }
                }
                else {
                    if (token_candidates.find(id->token) != token_candidates.end()) {
                        cerr << "cmb2parse: error: group definition conflicts with token definition for rule: " << id->token << "\n";
                        return false;
                    }
                    if (!group_candidates.insert(id->token).second) {
                        cerr << "cmb2parse: error: duplicate group definition for rule: " << id->token << "\n";
                        return false;
                    }
                }
            }
        }
        else {
            cerr << "cmb2parse: error: unsupported node kind in root\n";
            return false;
        }
    }
    for (auto& name : token_candidates) {
        if (!table.definitions.contains(name)) {
            cerr << "cmb2parse: error: token definition refers to undefined rule: " << name << "\n";
            return false;
        }
    }
    for (auto& name : group_candidates) {
        if (!table.definitions.contains(name)) {
            cerr << "cmb2parse: error: group definition refers to undefined rule: " << name << "\n";
            return false;
        }
    }
    for (auto& name : omit_if_one_candidates) {
        if (!group_candidates.contains(name)) {
            cerr << "cmb2parse: error: omit_if_one definition refers to undefined group rule: " << name << "\n";
            return false;
        }
    }
    for (auto& name : table.auto_rules) {
        if (!table.definitions.contains(name)) {
            cerr << "cmb2parse: error: auto definition refers to undefined rule: " << name << "\n";
            return false;
        }
    }
    table.tokens = std::move(token_candidates);
    table.auto_rules = std::move(auto_rules);
    table.omit_if_one = std::move(omit_if_one_candidates);
    if (root_name.empty()) {
        cerr << "cmb2parse: error: no root definition\n";
        return false;
    }
    if (!table.definitions.contains(root_name)) {
        cerr << "cmb2parse: error: root definition refers to undefined rule: " << root_name << "\n";
        return false;
    }
    table.root_name = root_name;
    return true;
}

void print_tree(Description& table, CodeWriter& w, const std::shared_ptr<futils::comb2::tree::node::GenericNode<std::string>>& node) {
    if (auto group = as_group<std::string>(node); group) {
        if (table.omit_if_one.contains(group->tag) && group->children.size() == 1) {
            print_tree(table, w, group->children[0]);
            return;
        }
    }
    if (node->tag.empty()) {  // this is root
        w.writeln("<root>");
    }
    else {
        w.writeln(node->tag);
    }
    auto indent = w.indent_scope();
    if (auto tok = as_tok<std::string>(node); tok) {
        w.writeln("token: ", tok->token);
    }
    else if (auto group = as_group<std::string>(node); group) {
        for (auto& child : group->children) {
            print_tree(table, w, child);
        }
    }
}

void print_json_tree(Description& table, futils::json::Stringer<>& w, const std::shared_ptr<futils::comb2::tree::node::GenericNode<std::string>>& node) {
    if (auto group = as_group<std::string>(node); group) {
        if (table.omit_if_one.contains(group->tag) && group->children.size() == 1) {
            print_json_tree(table, w, group->children[0]);
            return;
        }
    }
    auto obj = w.object();
    if (node->tag.empty()) {  // this is root
        obj("tag", "<root>");
    }
    else {
        obj("tag", node->tag);
    }
    if (auto tok = as_tok<std::string>(node); tok) {
        obj("token", tok->token);
    }
    else if (auto group = as_group<std::string>(node); group) {
        obj("children", [&]() {
            auto element = w.array();
            for (auto& child : group->children) {
                element([&] { print_json_tree(table, w, child); });
            }
        });
    }
}