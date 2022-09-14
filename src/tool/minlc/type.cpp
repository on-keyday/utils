/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "type.h"
#include "middle.h"
#include <helper/splits.h>

namespace minlc {
    namespace types {
        std::shared_ptr<StructType> to_struct_type(middle::M& m, const std::shared_ptr<mi::StructFieldNode>& node) {
            std::vector<std::shared_ptr<mi::StructFieldNode>> fields;
            mi::StructFieldToVec(fields, node);
            auto st = make_type<StructType>(m.types, struct_, node);
            for (auto& field : fields) {
                auto typ = to_type(m, field->type, true);
                if (!typ) {
                    return nullptr;
                }
                auto idents = utils::helper::split(field->ident->str, ",");
                for (auto& ident : idents) {
                    st->fields.push_back(StructField{
                        .name = ident,
                        .type = typ,
                        .node = field,
                    });
                }
            }
            return st;
        }

        std::shared_ptr<FunctionType> to_function_type(middle::M& m, const std::shared_ptr<mi::FuncNode>& node) {
            auto fn = make_type<FunctionType>(m.types, function_, node);
            auto append_param = [&](auto& node, auto& params) {
                std::vector<std::shared_ptr<mi::FuncParamNode>> nodes;
                mi::FuncParamToVec(nodes, node);
                for (auto& param : nodes) {
                    auto typ = to_type(m, param->type, false);
                    if (!typ) {
                        return false;
                    }
                    if (param->ident) {
                        for (auto&& ident : utils::helper::split(param->ident->str, ",")) {
                            params.push_back(FuncParam{.name = ident, .type = typ, .node = param});
                        }
                    }
                    else {
                        params.push_back(FuncParam{.type = typ, .node = param});
                    }
                }
                return true;
            };
            if (!append_param(node, fn->params)) {
                return nullptr;
            }
            if (auto ret = mi::is_FuncParam(node->return_)) {
                if (!append_param(ret, fn->return_)) {
                    return nullptr;
                }
            }
            else if (auto ty = mi::is_Type(node->return_)) {
                auto typ = to_type(m, ty, true);
                if (!typ) {
                    return nullptr;
                }
                fn->return_.push_back(FuncParam{.type = typ, .node = node});
            }
            return fn;
        }

        std::shared_ptr<Type> to_type(middle::M& m, const std::shared_ptr<mi::TypeNode>& node, bool no_va_arg) {
            if (!node) {
                return nullptr;
            }
            if (auto st = mi::is_StructField(node)) {
                return to_struct_type(m, st);
            }
            if (auto fn = mi::is_Func(node)) {
                return to_function_type(m, fn);
            }
            if (node->str == mi::array_str_) {
                auto arr = mi::is_ArrayType(node);
                auto expr = middle::minl_to_expr(m, arr->expr);
                if (!expr) {
                    return nullptr;
                }
                auto typ = to_type(m, arr->type, true);
                if (!typ) {
                    return nullptr;
                }
                auto array = make_type<ArrayType>(m.types, array_, node);
                array->expr = std::move(expr);
                array->base = std::move(typ);
                return array;
            }
            if (node->str == mi::typeof_str_) {
                auto tof = mi::is_ArrayType(node);
                auto expr = middle::minl_to_expr(m, tof->expr);
                if (!expr) {
                    return nullptr;
                }
                auto typeof_v = make_type<ArrayType>(m.types, typeof_, node);
                typeof_v->expr = std::move(expr);
                return typeof_v;
            }
            if (node->str == mi::const_str_) {
                auto typ = to_type(m, node->type, true);
                if (!typ) {
                    return nullptr;
                }
                typ->attr |= ta_const;
                return typ;
            }
            if (node->str == mi::pointer_str_) {
                auto typ = to_type(m, node->type, true);
                if (!typ) {
                    return nullptr;
                }
                auto ptr = make_type<Type>(m.types, pointer_, node);
                ptr->base = typ;
                return ptr;
            }
            if (node->str == mi::vector_str_) {
                auto typ = to_type(m, node->type, true);
                if (!typ) {
                    return nullptr;
                }
                auto vec = make_type<Type>(m.types, vector_, node);
                vec->base = typ;
                return vec;
            }
            if (node->str == mi::va_arg_str_) {
                if (no_va_arg) {
                    m.errc.say("unexpected variable argument `...`");
                    m.errc.node(node);
                    return nullptr;
                }
                if (node->type) {
                    auto typ = to_type(m, node->type, true);
                    if (!typ) {
                        return nullptr;
                    }
                    auto typed = make_type<Type>(m.types, typed_va_arg_, node);
                    typed->base = typ;
                    return typed;
                }
                return make_type<Type>(m.types, untyped_va_arg_, node);
            }
            if (node->str == mi::mut_str_ || node->str == mi::reference_str_ ||
                node->str == mi::genr_str_) {
                m.errc.say("reserved keyword but currently not available");
                m.errc.node(node);
                return nullptr;
            }
            auto splt = utils::helper::split(node->str, ".");
            auto ident = make_type<IdentType>(m.types, ident_, node);
            ident->types = std::move(splt);
            return ident;
        }
    }  // namespace types
}  // namespace minlc