/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "iface.h"
#include <comb2/tree/simple_node.h>
#include "interface_list.h"

namespace ifacegen {
    namespace node = futils::comb2::tree::node;

    bool is_tok(const std::shared_ptr<node::Node>& node, const char* tok) {
        auto t = node::as_tok(node);
        if (t && t->token == tok) {
            return true;
        }
        return false;
    }

    bool traverse_typeparam(std::vector<TypeName>& params, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        auto g = static_cast<node::Group*>(node.get());
        auto it = g->children.begin();
        if (it->get()->tag != k_token) {
            return false;
        }
        it++;
        while (it != g->children.end()) {
            TypeName param;
            if (it->get()->tag == k_token) {
                if (is_tok(*it, "...")) {
                    param.vararg = true;
                }
                else if (is_tok(*it, "^")) {
                    param.template_param = true;
                }
                it++;
            }
            if (it->get()->tag != k_ident) {
                return false;
            }
            param.name = node::as_tok(it->get())->token;
            it++;
            if (it == g->children.end()) {
                params.push_back(std::move(param));
                break;
            }
            if (is_tok(*it, "=")) {
                it++;
                if (it->get()->tag != k_ident) {
                    return false;
                }
                param.defvalue = node::as_tok(it->get())->token;
                it++;
                if (it == g->children.end()) {
                    break;
                }
            }
            params.push_back(std::move(param));
            if (is_tok(*it, ",")) {
                it++;
                continue;
            }
        }
        return true;
    }

    bool traverse_type(Type& type, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        if (node->tag != k_type) {
            return false;
        }
        auto g = node::as_group(node);
        auto it = g->children.begin();
        if (is_tok(*it, "...")) {
            type.vararg = true;
            it++;
        }
        if (is_tok(*it, "&&")) {
            type.ref = RefKind::rval;
            it++;
        }
        else if (is_tok(*it, "&")) {
            type.ref = RefKind::lval;
            it++;
        }
        if (it->get()->tag == k_pointer) {
            type.pointer = node::as_tok(*it)->token.size();
            it++;
        }
        if (is_tok(*it, "const")) {
            type.is_const = true;
            it++;
        }
        if (it->get()->tag != k_ident) {
            return false;
        }
        type.prim = node::as_tok(*it)->token;
        return true;
    }

    bool traverse_func(IfaceList& iface, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        Interface func;
        auto g = node::as_group(node);
        auto it = g->children.begin();
        if (is_tok(*it, "const")) {
            func.is_const = true;
            it++;
        }
        if (is_tok(*it, "noexcept")) {
            func.is_noexcept = true;
            it++;
        }
        if (it->get()->tag != k_ident) {
            return false;
        }
        func.funcname = node::as_tok(*it)->token;
        iface.has_nonnull = iface.has_nonnull || func.funcname == "__nonnull__";
        iface.has_sso = iface.has_sso || func.funcname == "__sso__";
        iface.has_unsafe = iface.has_unsafe || func.funcname == "__unsafe__";
        iface.has_vtable = iface.has_vtable || func.funcname == "__vtable__";
        it++;
        if (!is_tok(*it, "(")) {
            return false;
        }
        it++;
        while (!is_tok(*it, ")")) {
            Arg arg;
            if (it->get()->tag != k_ident) {
                return false;
            }
            arg.name = node::as_tok(*it)->token;
            it++;
            if (!traverse_type(arg.type, *it)) {
                return false;
            }
            func.args.push_back(std::move(arg));
            it++;
            if (it == g->children.end()) {
                return false;
            }
            if (is_tok(*it, ")")) {
                break;
            }
            if (is_tok(*it, ",")) {
                it++;
                continue;
            }
            return false;
        }
        it++;
        if (!traverse_type(func.type, *it)) {
            return false;
        }
        it++;
        if (it == g->children.end()) {
            iface.iface.push_back(std::move(func));
            return true;
        }
        if (!is_tok(*it, "=")) {
            return false;
        }
        it++;
        if (it->get()->tag != k_ident) {
            return false;
        }
        func.default_result = node::as_tok(*it)->token;
        iface.iface.push_back(std::move(func));
        return true;
    }

    bool traverse_interface(FileData& data, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        auto g = node::as_group(node);
        IfaceList iface;
        auto it = g->children.begin();
        if (it->get()->tag == k_typeparam) {
            if (!traverse_typeparam(iface.typeparam, *it)) {
                return false;
            }
            it++;
        }
        if (!is_tok(*it, "interface")) {  // interface
            return false;
        }
        it++;
        if (it->get()->tag != k_ident) {
            return false;
        }
        auto name = node::as_tok(*it)->token;
        it++;
        if (!is_tok(*it, "{")) {  // {
            return false;
        }
        it++;
        while (it != g->children.end()) {
            if (is_tok(*it, "}")) {
                it++;
                break;
            }
            if (it->get()->tag == k_func) {
                if (!traverse_func(iface, *it)) {
                    return false;
                }
                data.has_sso_align = data.has_sso_align || iface.has_sso;
                data.has_ref_ret = data.has_ref_ret || iface.iface.back().default_result == "panic";
            }
            it++;  // skip comment
        }
        auto [_, ok] = data.ifaces.emplace(name, std::move(iface));
        if (!ok) {
            return false;
        }
        data.defvec.push_back(std::move(name));
        return ok;
    }

    bool traverse_alias(FileData& data, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        auto g = node::as_group(node);
        Alias alias;
        alias.is_macro = node->tag == k_macro;
        auto it = g->children.begin();
        if (it->get()->tag == k_typeparam) {
            if (!traverse_typeparam(alias.types, *it)) {
                return false;
            }
            it++;
        }
        if (!is_tok(*it, "alias") &&
            !is_tok(*it, "macro")) {
            return false;
        }
        it++;
        auto name = node::as_tok(*it)->token;
        it++;
        alias.token = node::as_tok(*it)->token;
        auto [_, ok] = data.aliases.emplace(name, std::move(alias));
        if (!ok) {
            return false;
        }
        if (!alias.is_macro) {
            data.defvec.push_back(name);
        }
        return true;
    }

    bool traverse_import(FileData& data, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        auto g = node::as_group(node);
        auto it = g->children.begin();
        if (!is_tok(*it, "import")) {
            return false;
        }
        it++;
        data.headernames.push_back(node::as_tok(*it)->token);
        return true;
    }

    bool traverse_package(FileData& data, const std::shared_ptr<node::Node>& node) {
        if (!node->is_group) {
            return false;
        }
        auto g = node::as_group(node);
        auto it = g->children.begin();
        if (!is_tok(*it, "package")) {
            return false;
        }
        it++;
        data.pkgname = node::as_tok(*it)->token;
        return true;
    }

    bool traverse(FileData& data, const std::shared_ptr<node::Node>& node) {
        auto g = node::as_group(node);
        for (auto& child : g->children) {
            if (child->tag == k_comments) {
                continue;  // skip
            }
            if (child->tag == k_package) {
                if (!traverse_package(data, child)) {
                    return false;
                }
            }
            if (child->tag == k_interface) {
                if (!traverse_interface(data, child)) {
                    return false;
                }
            }
            if (child->tag == k_macro || child->tag == k_alias) {
                if (!traverse_alias(data, child)) {
                    return false;
                }
            }
            if (child->tag == k_import) {
                if (!traverse_import(data, child)) {
                    return false;
                }
            }
        }
        return true;
    }
}  // namespace ifacegen
