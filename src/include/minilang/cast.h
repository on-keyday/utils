/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "minnode.h"

namespace utils {
    namespace minilang {

        inline std::shared_ptr<BinaryNode> is_Binary(const std::shared_ptr<MinNode>& root, bool strict = false) {
            if (!root) {
                return nullptr;
            }
            bool res = false;
            if (strict) {
                res = root->node_type == nt_binary;
            }
            else {
                res = (root->node_type & nt_min_derive_mask) == nt_binary;
            }
            if (res) {
                return std::static_pointer_cast<BinaryNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<WordStatNode> is_WordStat(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_wordstat) {
                return std::static_pointer_cast<WordStatNode>(root);
            }
            return nullptr;
        }

        // also detect Func
        inline std::shared_ptr<FuncParamNode> is_FuncParam(const std::shared_ptr<MinNode>& root, bool strict = false) {
            if (!root) {
                return nullptr;
            }
            bool res = false;
            if (strict) {
                res = root->node_type == nt_funcparam;
            }
            else {
                res = (root->node_type & nt_second_derive_mask) == nt_funcparam;
            }
            if (res) {
                return std::static_pointer_cast<FuncParamNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<FuncNode> is_Func(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_func) {
                return std::static_pointer_cast<FuncNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<TypeDefNode> is_TypeDef(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_typedef) {
                return std::static_pointer_cast<TypeDefNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<ArrayTypeNode> is_ArrayType(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_arrtype) {
                return std::static_pointer_cast<ArrayTypeNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<InterfaceNode> is_Interface(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_interface) {
                return std::static_pointer_cast<InterfaceNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<GenericTypeNode> is_GenericType(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_gentype) {
                return std::static_pointer_cast<GenericTypeNode>(root);
            }
            return nullptr;
        }

        // also detect StructField, FuncParam, Func,ArrayType,GenericType
        inline std::shared_ptr<TypeNode> is_Type(const std::shared_ptr<MinNode>& root, bool strict = false) {
            if (!root) {
                return nullptr;
            }
            bool res = false;
            if (strict) {
                res = root->node_type == nt_type;
            }
            else {
                res = (root->node_type & nt_min_derive_mask) == nt_type;
            }
            if (res) {
                return std::static_pointer_cast<TypeNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<ImportNode> is_Import(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_import) {
                return std::static_pointer_cast<ImportNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<LetNode> is_Let(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_let) {
                return std::static_pointer_cast<LetNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<BlockNode> is_Block(const std::shared_ptr<MinNode>& root, bool strict = false) {
            if (!root) {
                return nullptr;
            }
            bool res = false;
            if (strict) {
                res = root->node_type == nt_block;
            }
            else {
                res = (root->node_type & nt_min_derive_mask) == nt_block;
            }
            if (res) {
                return std::static_pointer_cast<BlockNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<SwitchStatNode> is_SwitchStat(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_switch) {
                return std::static_pointer_cast<SwitchStatNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<CommentNode> is_Comment(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_comment) {
                return std::static_pointer_cast<CommentNode>(root);
            }
            return nullptr;
        }

        // also detect ForStat,IfStat
        inline std::shared_ptr<CondStatNode> is_CondStat(const std::shared_ptr<MinNode>& root, bool strict = false) {
            if (!root) {
                return nullptr;
            }
            bool res = false;
            if (strict) {
                res = root->node_type == nt_cond;
            }
            else {
                res = (root->node_type & nt_second_derive_mask) == nt_cond;
            }
            if (res) {
                return std::static_pointer_cast<CondStatNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<IfStatNode> is_IfStat(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_if) {
                return std::static_pointer_cast<IfStatNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<ForStatNode> is_ForStat(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_for) {
                return std::static_pointer_cast<ForStatNode>(root);
            }
            return nullptr;
        }

        inline std::shared_ptr<StructFieldNode> is_StructField(const std::shared_ptr<MinNode>& root) {
            if (!root) {
                return nullptr;
            }
            if (root->node_type == nt_struct_field) {
                return std::static_pointer_cast<StructFieldNode>(root);
            }
            return nullptr;
        }

        // detect StringNode. str starting with " or ' or `
        inline std::shared_ptr<StringNode> is_String(const std::shared_ptr<MinNode>& node) {
            if (!node) {
                return nullptr;
            }
            if (node->node_type == nt_string) {
                return std::static_pointer_cast<StringNode>(node);
            }
            return nullptr;
        }

        // detect NumberNode .str starting with digit
        inline std::shared_ptr<NumberNode> is_Number(const std::shared_ptr<MinNode>& node) {
            if (!node) {
                return nullptr;
            }
            if (node->node_type == nt_number) {
                return std::static_pointer_cast<NumberNode>(node);
            }
            return nullptr;
        }

        // detect strict MinNode
        inline bool is_MinNode(const std::shared_ptr<MinNode>& node) {
            return node && node->node_type == nt_min;
        }

        // detect MinNode,BinaryNode (strict),TypeNode (type primitive)
        /// and FuncNode (mode==fe_expr,mode==fe_typedec(type_primitive))
        inline bool is_Expr(const std::shared_ptr<MinNode>& node) {
            if (auto fn = is_Func(node)) {
                return fn->mode == fe_expr;
            }
            else if (is_MinNode(node) || is_Number(node) || is_String(node) || is_Binary(node, true)) {
                return true;
            }
            else if (auto ty = is_Type(node); ty && ty->str == typeprim_str_) {
                return true;
            }
            return false;
        }

        inline bool is_Ident(const std::shared_ptr<MinNode>& node) {
            return is_MinNode(node);
        }
    }  // namespace minilang
}  // namespace utils
