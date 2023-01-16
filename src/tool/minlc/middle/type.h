/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/old/minlsymbols.h>

namespace minlc {
    namespace mi = utils::minilang;
    namespace middle {
        struct M;
        struct Expr;
    }  // namespace middle
    namespace types {
        enum Kind {
            bool_,
            int8_,
            int16_,
            int32_,
            int64_,
            uint8_,
            uint16_,
            uint32_,
            uint64_,
            intptr_,
            uintptr_,
            pointer_,
            vector_,
            array_,
            struct_,
            function_,
            typeof_,
            typed_va_arg_,
            untyped_va_arg_,
            ident_,
        };

        enum TypeAttr {
            ta_const = 0x1,
        };

        struct Type {
            const Kind kind;
            constexpr Type(Kind k)
                : kind(k) {}
            std::shared_ptr<mi::TypeNode> node;
            unsigned int attr = 0;
        };

        struct BuiltinType : Type {
            constexpr BuiltinType(Kind k)
                : Type(k) {}
        };

        struct DefinedType : Type {
            constexpr DefinedType(Kind k)
                : Type(k) {}
            std::shared_ptr<Type> base;
        };

        struct StructField {
            std::string name;
            std::shared_ptr<Type> type;
            std::shared_ptr<mi::StructFieldNode> node;
        };

        struct StructType : Type {
            MINL_Constexpr StructType(Kind k)
                : Type(k) {}
            std::vector<StructField> fields;
        };

        struct FuncParam {
            std::string name;
            std::shared_ptr<Type> type;
            std::shared_ptr<mi::FuncParamNode> node;
        };

        struct FunctionType : DefinedType {
            MINL_Constexpr FunctionType(Kind kind)
                : DefinedType(kind) {}
            std::vector<FuncParam> params, return_;
        };

        struct ArrayType : DefinedType {
            MINL_Constexpr ArrayType(Kind kind)
                : DefinedType(kind) {}
            std::shared_ptr<middle::Expr> expr;
        };

        struct IdentType : Type {
            std::vector<mi::LookUpResult> lookedup;
            std::vector<std::string> types;
            MINL_Constexpr IdentType(Kind kind)
                : Type(kind) {}
        };

        struct Types {
            std::map<std::string, std::shared_ptr<Type>> builtins;
            std::shared_ptr<Type> get_builtin(middle::M& m, std::string_view view);
            std::shared_ptr<Type> get_integer_builtin(middle::M& m, size_t num);
        };

        std::shared_ptr<Type> to_type(middle::M& types, const std::shared_ptr<mi::TypeNode>& node, bool no_va_arg);

        template <class T, class N>
        std::shared_ptr<T> make_type(Types& types, Kind kind, const std::shared_ptr<N>& node) {
            auto res = std::make_shared<T>(kind);
            res->node = node;
            return res;
        }
    }  // namespace types
}  // namespace minlc
