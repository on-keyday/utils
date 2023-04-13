/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "binp.h"
#include <minilang/comb/json_traverse.h>

namespace binp {

    using utils::minilang::comb::branch::expect;
    using utils::minilang::comb::branch::expect_token;
    using utils::minilang::comb::branch::expect_with_pos;
    using utils::minilang::comb::branch::skip_space;

    bool traverse_attrs(Attributes& type, auto&& it, const json::JSON& node) {
        skip_space(it, node);
        if (auto obj = expect_with_pos(it, node, "default_object", &type.default_pos)) {
            type.default_value = obj->template force_as_string<std::string>();
            type.default_value.erase(0, 1);
            type.default_value.pop_back();
            skip_space(it, node);
        }
        if (auto obj = expect_with_pos(it, node, "encode_method", &type.encode_pos)) {
            type.encode_method = obj->template force_as_string<std::string>();
            type.encode_method.erase(0, 2);
            type.encode_method.pop_back();
            type.encode_method.pop_back();
            skip_space(it, node);
        }
        if (auto obj = expect_with_pos(it, node, "decode_method", &type.decode_pos)) {
            type.decode_method = obj->template force_as_string<std::string>();
            type.decode_method.erase(0, 2);
            type.decode_method.pop_back();
            type.decode_method.pop_back();
        }
        return true;
    }

    bool traverse_struct(Struct& scope, const json::JSON& node);

    bool traverse_type(Type& type, const json::JSON& node) {
        auto it = node.abegin();
        Pos pos;
        if (auto ident = expect_with_pos(it, node, "ident", &pos)) {
            type.type = IdentTy{pos, ident->force_as_string<std::string>()};
        }
        else if (auto st = expect(it, node, "struct")) {
            Struct s;
            if (!traverse_struct(s, *st)) {
                return false;
            }
            type.type = std::move(s);
        }
        return traverse_attrs(type.attrs, it, node);
    }

    bool traverse_field(Field& field, const json::JSON& node) {
        auto it = node.abegin();
        auto ident = expect_with_pos(it, node, "ident", &field.name_pos);
        if (!ident) {
            return false;
        }
        field.name = ident->force_as_string<std::string>();
        skip_space(it, node);
        auto type = expect(it, node, "type");
        if (!type) {
            return false;
        }
        return traverse_type(field.type, *type);
    }

    bool traverse_struct(Struct& scope, const json::JSON& node) {
        auto it = node.abegin();
        if (!expect_token(it, node, "struct", &scope.struct_pos)) {
            return false;
        }
        skip_space(it, node);
        if (!expect_token(it, node, "{", &scope.begin_pos)) {
            return false;
        }
        skip_space(it, node);
        while (!expect_token(it, node, "}", &scope.end_pos)) {
            const auto field_n = expect(it, node, "field");
            if (!field_n) {
                return false;
            }
            Field field;
            if (!traverse_field(field, *field_n)) {
                return false;
            }
            scope.fields.push_back(std::move(field));
            skip_space(it, node);
        }
        return true;
    }

    bool traverse_typedef(TypeDef& typdef, const json::JSON& node) {
        auto it = node.abegin();
        if (!expect_token(it, node, "type", &typdef.type_pos)) {
            return false;
        }
        skip_space(it, node);
        auto name = expect_with_pos(it, node, "type_name", &typdef.name_pos);
        if (!name) {
            return false;
        }
        typdef.name = name->force_as_string<std::string>();
        skip_space(it, node);
        auto s = expect(it, node, "type");
        if (!s) {
            return false;
        }
        return traverse_type(typdef.base, *s);
    }

    bool traverse_namespace(Program& scope, const json::JSON& node) {
        auto it = node.abegin();
        if (!expect_token(it, node, "namespace")) {
            return false;
        }
        skip_space(it, node);
        auto cpp = expect(it, node, "cpp_namespace");
        if (!cpp) {
            return false;
        }
        scope.namespace_ = cpp->force_as_string<std::string>();
        return true;
    }

    bool traverse_include(Program& scope, const json::JSON& node) {
        auto it = node.abegin();
        if (!expect_token(it, node, "include")) {
            return false;
        }
        skip_space(it, node);
        auto cpp = expect(it, node, "path");
        if (!cpp) {
            return false;
        }
        scope.includes.push_back(cpp->force_as_string<std::string>());
        return true;
    }

    bool traverse(Program& scope, const json::JSON& node) {
        auto prg = node.at("program");
        if (!prg) {
            return false;
        }
        auto it = prg->abegin();
        while (it != prg->aend()) {
            skip_space(it, *prg);
            if (it == prg->aend()) {
                break;
            }
            if (auto n = expect(it, *prg, "namespace")) {
                if (!traverse_namespace(scope, *n)) {
                    return false;
                }
            }
            else if (auto t = expect(it, *prg, "type")) {
                TypeDef def;
                if (!traverse_typedef(def, *t)) {
                    return false;
                }
                scope.typedefs.push_back(std::move(def));
            }
            else if (auto t = expect(it, *prg, "include")) {
                if (!traverse_include(scope, *t)) {
                    return false;
                }
            }
            else {
                return false;
            }
        }
        return true;
    }
}  // namespace binp
