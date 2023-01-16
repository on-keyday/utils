/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlutil - minilang utility
#pragma once
#include "minnode.h"
#include "cast.h"
#include <helper/equal.h>

namespace utils {
    namespace minilang {
        template <class Vec>
        bool BinaryToVec(Vec&& vec, const std::shared_ptr<MinNode>& node, const char* comma = ",") {
            if (!node) {
                return false;
            }
            if (node->str == comma) {
                auto bin = is_Binary(node);
                if (!bin) {
                    return false;
                }
                return BinaryToVec(vec, bin->left, comma) && BinaryToVec(vec, bin->right, comma);
            }
            vec.push_back(node);
            return true;
        }

        // walk walks among each node
        // callback is called twice per node
        // bool callback(const std::shared_ptr<Node>& node,bool after)
        std::shared_ptr<MinNode> walk(const std::shared_ptr<MinNode>& root, auto&& callback) {
            if (!root) {
                return nullptr;
            }
            auto visit = [&](auto&& node, bool after) {
                if (node) {
                    if (!callback(std::as_const(node), after)) {
                        return false;
                    }
                }
                return true;
            };
            auto select = [&](auto&& root_node, auto&&... nodes) -> std::shared_ptr<MinNode> {
                if (!visit(root_node, false)) {
                    return root_node;
                }
                std::shared_ptr<MinNode> except;
                auto fold = [&](auto&& node) {
                    except = walk(node, callback);
                    return except == nullptr;
                };
                if (!(... && fold(nodes))) {
                    return except;
                }
                if (!visit(root_node, true)) {
                    return root_node;
                }
                return nullptr;
            };
            if (auto node = is_ForStat(root)) {
                return select(node, node->right, node->center, node->left, node->block);
            }
            if (auto node = is_IfStat(root)) {
                return select(node, node->right, node->left, node->block, node->els);
            }
            if (auto node = is_CondStat(root)) {
                return select(node, node->right, node->left, node->block);
            }
            if (auto node = is_Binary(root)) {
                return select(node, node->left, node->right);
            }
            if (auto node = is_TypeDef(root)) {
                return select(node, node->ident, node->type, node->next);
            }
            if (auto node = is_Let(root)) {
                return select(node, node->ident, node->type, node->init, node->next);
            }
            if (auto node = is_StructField(root)) {
                return select(node, node->ident, node->type, node->init, node->next);
            }
            if (auto node = is_Interface(root)) {
                return select(node, node->method, node->type, node->next);
            }
            if (auto node = is_Func(root)) {
                return select(node, node->ident, node->type, node->return_, node->next, node->block);
            }
            if (auto node = is_FuncParam(root)) {
                return select(node, node->ident, node->type, node->next);
            }
            if (auto node = is_ArrayType(root)) {
                return select(node, node->expr, node->type);
            }
            if (auto node = is_GenericType(root)) {
                return select(node, node->type);
            }
            if (auto node = is_Type(root)) {
                return select(node, node->type);
            }
            if (auto node = is_SwitchStat(root)) {
                return select(node, node->expr, node->element, node->next, node->next_case);
            }
            if (auto node = is_Block(root)) {
                return select(node, node->element, node->next);
            }
            if (auto node = is_Comment(root)) {
                return select(node, node->next);
            }
            if (auto node = is_Import(root)) {
                return select(node, node->from, node->next);
            }
            if (auto node = is_WordStat(root)) {
                return select(node, node->expr, node->block);
            }
            if (auto node = is_String(root)) {
                return select(node);
            }
            if (auto node = is_Number(root)) {
                return select(node);
            }
            return select(root);
        }

        template <class NewType, class Alias>
        void TydefToVec(NewType& new_type, Alias& alias, const std::shared_ptr<TypeDefNode>& typdef) {
            if (!typdef) {
                return;
            }
            if (typdef->str == type_group_str_) {
                TydefToVec(new_type, alias, typdef->next);
                return;
            }
            if (typdef->str == new_type_str_) {
                new_type.push_back(typdef);
            }
            if (typdef->str == type_alias_str_) {
                alias.push_back(typdef);
            }
            TydefToVec(new_type, alias, typdef->next);
        }

        template <class Vec>
        void LetToVec(Vec& vec, const std::shared_ptr<LetNode>& node) {
            if (!node) {
                return;
            }
            if (node->str == let_def_group_str_ || node->str == const_def_group_str_) {
                LetToVec(vec, node->next);
                return;
            }
            vec.push_back(node);
            LetToVec(vec, node->next);
        }

        template <class Vec>
        void FuncParamToVec(Vec& vec, const std::shared_ptr<FuncParamNode>& node) {
            if (!node) {
                return;
            }
            if (is_Func(node)) {
                FuncParamToVec(vec, node->next);
                return;
            }
            vec.push_back(node);
            FuncParamToVec(vec, node->next);
        }

        template <class Vec>
        void StructFieldToVec(Vec& vec, const std::shared_ptr<StructFieldNode>& node) {
            if (!node) {
                return;
            }
            if (node->str == struct_str_ || node->str == union_str_) {
                return StructFieldToVec(vec, node->next);
            }
            vec.push_back(node);
            StructFieldToVec(vec, node->next);
        }

        inline bool equal_Type(const std::shared_ptr<MinNode>& left, const std::shared_ptr<MinNode>& right);

        inline bool equal_FuncParams(const std::shared_ptr<FuncParamNode>& left, const std::shared_ptr<FuncParamNode>& right) {
            auto count_param = [](auto& node) -> size_t {
                if (!node) {
                    return 0;
                }
                size_t count = 1;
                if (node->ident) {
                    count += std::count(node->ident->str.begin(), node->ident->str.end(), ',');
                }
                return count;
            };
            auto left_count = count_param(left);
            auto right_count = count_param(right);
            auto curleft = left, curright = right;
            while (curleft && curright) {
                if (!equal_Type(curleft->type, curright->type)) {
                    return false;
                }
                left_count--;
                right_count--;
                if (left_count == right_count) {
                    curleft = curleft->next;
                    curright = curright->next;
                    left_count = count_param(curleft);
                    right_count = count_param(curright);
                    continue;
                }
                if (left_count == 0) {
                    curleft = curleft->next;
                    left_count = count_param(curleft);
                }
                if (right_count == 0) {
                    curright = curright->next;
                    right_count = count_param(curright);
                }
            }
            if (curleft || curright) {
                return false;
            }
            return true;
        }

        inline bool equal_StructField(const std::shared_ptr<StructFieldNode>& left, const std::shared_ptr<StructFieldNode>& right) {
            if (!left || !right) {
                return left == right;
            }
            auto curleft = left;
            auto curright = right;
            size_t left_i = 0, right_i = 0;
            while (curleft && curright) {
                auto leftid = idents(curleft->ident->str);
                auto rightid = idents(curright->ident->str);
                while (true) {
                    if (!helper::equal(leftid[left_i], rightid[right_i])) {
                        return false;
                    }
                    if (!equal_Type(curleft->type, curright->type)) {
                        return false;
                    }
                    left_i++;
                    right_i++;
                    if (left_i == leftid.size()) {
                        curleft = curleft->next;
                        left_i = 0;
                    }
                    if (right_i == rightid.size()) {
                        curright = curright->next;
                        right_i = 0;
                    }
                    if (left_i != 0 && right_i != 0) {
                        continue;
                    }
                    break;
                }
            }
            if (curleft || curright) {
                return false;
            }
            return true;
        }

        inline bool equal_Type(const std::shared_ptr<MinNode>& left, const std::shared_ptr<MinNode>& right) {
            if (!left || !right) {
                return left == right;
            }
            auto typleft = is_Type(left);
            auto typright = is_Type(right);
            if (!typleft || !typright) {
                return false;
            }
            if (typleft->node_type != typright->node_type) {
                return false;
            }
            if (auto fnleft = is_Func(left)) {
                auto fnright = is_Func(right);
                auto fnlefparm = is_FuncParam(fnleft->return_);
                auto fnrigparm = is_FuncParam(fnright->return_);
                if ((!fnlefparm && !fnrigparm) || (fnlefparm && fnrigparm)) {
                    if (!equal_Type(fnleft->return_, fnright->return_)) {
                        return false;
                    }
                }
                else if (fnlefparm) {
                    if (fnlefparm->next) {
                        return false;
                    }
                    if (!equal_Type(fnlefparm->type, fnright->return_)) {
                        return false;
                    }
                }
                else {
                    if (fnrigparm->next) {
                        return false;
                    }
                    if (!equal_Type(fnleft->return_, fnrigparm->type)) {
                        return false;
                    }
                }
                return equal_Type(fnleft->next, fnright->next);
            }
            if (auto parleft = is_FuncParam(left)) {
                auto parright = is_FuncParam(right);
                return equal_FuncParams(parleft, parright);
            }
            if (auto fieleft = is_StructField(left)) {
                auto fieright = is_StructField(right);
                return fieleft->str == fieright->str &&
                       equal_StructField(fieleft->next, fieright->next);
            }
            if (!equal_Type(typleft->type, typright->type)) {
                return false;
            }
            return left->str == right->str;
        }
    }  // namespace minilang
}  // namespace utils
