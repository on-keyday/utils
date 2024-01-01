/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "c_lex.h"
#include <comb2/tree/error.h>
#include <escape/escape.h>
#include <number/prefix.h>
#include <map>

namespace futils::langc {
    using Errors = comb2::tree::node::Errors;
    using Error = comb2::tree::node::Error;
    namespace ast {

        enum class ObjectType {
            program,
            group_bits = 0xF0000,
            expr = 0x10000,
            int_literal,
            str_literal,
            ident,
            unary,
            binary,
            cond_op,
            paren,
            call,
            index,
            decl = 0x20000,
            func,
            field,
            type = 0x40000,
            int_type,
            float_type,
            ident_type,
            void_type,
            pointer_type,
            func_type,
            array_type,
            control = 0x80000,
            return_,
            if_,
            while_,
            block,
            for_,
        };

        constexpr bool is_typeof(ObjectType type, ObjectType match) {
            return (int(ObjectType::group_bits) & int(type)) == int(match);
        }

        struct Object {
            Pos pos;
            const ObjectType type;

            virtual ~Object() {}

           protected:
            constexpr Object(ObjectType t)
                : type(t) {
            }
        };

        struct Type;

        struct Expr : Object {
            std::shared_ptr<Type> expr_type;

           protected:
            constexpr Expr(ObjectType t)
                : Object(t) {}
        };

        struct Idents;
        struct Callee;

        struct Ident : Expr {
            std::string ident;
            Idents* belong = nullptr;
            Callee* callee = nullptr;

            constexpr Ident()
                : Expr(ObjectType::ident) {}
        };

        constexpr Ident* as_ident(Expr* e) {
            if (e && e->type == ObjectType::ident) {
                return static_cast<Ident*>(e);
            }
            return nullptr;
        }

        struct Unary : Expr {
            std::unique_ptr<Expr> target;
            ast::UnaryOp op;

            constexpr Unary()
                : Expr(ObjectType::unary) {}
        };

        struct Binary : Expr {
            std::unique_ptr<Expr> left;
            std::unique_ptr<Expr> right;
            ast::BinaryOp op;

            constexpr Binary()
                : Expr(ObjectType::binary) {}
        };

        struct CondOp : Expr {
            std::unique_ptr<Expr> cond;
            std::unique_ptr<Expr> then_;
            Pos els_pos;
            std::unique_ptr<Expr> else_;
            constexpr CondOp()
                : Expr(ObjectType::cond_op) {}
        };

        struct Paren : Expr {
            Pos end_pos;
            std::unique_ptr<Expr> expr;

            constexpr Paren()
                : Expr(ObjectType::paren) {}
        };

        struct Call : Expr {
            Pos end_pos;
            std::unique_ptr<Expr> callee;
            std::unique_ptr<Expr> arguments;

            constexpr Call()
                : Expr(ObjectType::call) {}
        };

        struct Index : Expr {
            Pos end_pos;
            std::unique_ptr<Expr> object;
            std::unique_ptr<Expr> index;

            constexpr Index()
                : Expr(ObjectType::index) {}
        };

        struct Literal : Expr {
           protected:
            constexpr Literal(ObjectType t)
                : Expr(t) {}
        };

        struct IntLiteral : Literal {
            std::string raw;

            template <class T>
            std::optional<T> parse_as() const {
                T t = 0;
                if (!futils::number::prefix_integer(raw, t)) {
                    return std::nullopt;
                }
                return t;
            }

            IntLiteral()
                : Literal(ObjectType::int_literal) {}
        };

        std::optional<std::pair<std::string, size_t>> clang_escape(std::string_view str_lit) {
            std::string mid, out;
            if (!escape::unescape_str(str_lit.substr(1, str_lit.size() - 2), mid)) {
                return std::nullopt;
            }
            if (!escape::escape_str(mid, out, escape::EscapeFlag::hex)) {
                return std::nullopt;
            }
            return std::make_pair("\"" + out + "\"", mid.size());
        }

        struct StrLiteral : Literal {
            std::string str;
            std::string label;

            StrLiteral()
                : Literal(ObjectType::str_literal) {}
        };

        struct Decl : Object {
           protected:
            constexpr Decl(ObjectType t)
                : Object(t) {}
        };

        struct Type;

        struct Type : Object {
            size_t size = 0;
            size_t align = 0;
            std::optional<Pos> is_const;

           protected:
            constexpr Type(ObjectType t)
                : Object(t) {}
        };

        enum class IntTypeKind {
            int_,
            char_,
            size,
            intptr,
        };

        struct IntType : Type {
            IntTypeKind ikind;

            IntType()
                : Type(ObjectType::int_type) {}
        };

        IntType* as_int(Type* t) {
            if (!t || t->type != ObjectType::int_type) {
                return nullptr;
            }
            return static_cast<IntType*>(t);
        }

        struct FloatType : Type {
            std::string ident;
            FloatType()
                : Type(ObjectType::float_type) {}
        };

        struct PointerType : Type {
            std::shared_ptr<Type> base;
            PointerType()
                : Type(ObjectType::pointer_type) {}
        };

        std::shared_ptr<Type> deref(Type* t) {
            if (!t || t->type != ObjectType::pointer_type) {
                return nullptr;
            }
            return static_cast<PointerType*>(t)->base;
        }

        struct ArrayType : Type {
            std::shared_ptr<Type> base;
            std::unique_ptr<Expr> array_size_expr;
            size_t array_size = 0;

            ArrayType()
                : Type(ObjectType::array_type) {}
        };

        inline std::shared_ptr<PointerType> array_to_ptr(const std::shared_ptr<Type>& ty) {
            if (!ty || ty->type != ObjectType::array_type) {
                return nullptr;
            }
            auto arr = static_cast<ArrayType*>(ty.get());
            auto ptr = std::make_shared<PointerType>();
            ptr->base = arr->base;
            ptr->size = 8;
            ptr->align = 8;
            ptr->pos = ty->pos;
            return ptr;
        }

        struct VoidType : Type {
            constexpr VoidType()
                : Type(ObjectType::void_type) {}
        };

        struct Comment : Object {
            std::string comment;
        };

        struct Stmt : Object {
           protected:
            constexpr Stmt(ObjectType t)
                : Object(t) {}
        };

        struct Control : Stmt {
           protected:
            constexpr Control(ObjectType t)
                : Stmt(t) {}
        };

        struct Return : Control {
            std::unique_ptr<Expr> value;

            constexpr Return()
                : Control(ObjectType::return_) {
            }
        };

        struct IfStmt : Control {
            std::unique_ptr<Expr> cond;
            std::unique_ptr<Object> then;
            std::unique_ptr<Object> els_;
            constexpr IfStmt()
                : Control(ObjectType::if_) {}
        };

        struct WhileStmt : Control {
            std::unique_ptr<Expr> cond;
            std::unique_ptr<Object> body;

            constexpr WhileStmt()
                : Control(ObjectType::while_) {}
        };

        struct ForStmt : Control {
            std::unique_ptr<Object> init;
            std::unique_ptr<Expr> cond;
            std::unique_ptr<Expr> cont;
            std::unique_ptr<Object> body;

            constexpr ForStmt()
                : Control(ObjectType::for_) {}
        };

        struct Block : Control {
            std::vector<std::unique_ptr<Object>> objects;
            Pos end_pos;

            constexpr Block()
                : Control(ObjectType::block) {}
        };

        struct Field : Decl {
            std::string ident;
            // shared by some declation, for example
            // int a,b;
            std::shared_ptr<Type> type_;
            Idents* belong = nullptr;
            Field()
                : Decl(ObjectType::field) {}
        };

        struct Idents {
            Field* def = nullptr;
            std::vector<Ident*> idents;
            size_t offset = 0;
            bool global = false;
        };

        struct Func;
        struct FuncType;

        struct Callee {
            Func* body = nullptr;
            std::vector<Func*> decls;
            std::vector<Ident*> idents;
            std::shared_ptr<FuncType> type;
        };

        void error_defined_identifier(Errors& errs, const std::string& ident, Pos ref, Pos def) {
            errs.push(Error{"identifier " + ident + " is already defined", ref});
            errs.push(Error{"identifier " + ident + " is defined here", def});
        }

        struct Variables {
            std::map<std::string, Idents> idents;
            size_t max_offset = 0;
            bool global = false;

            std::optional<Pos> is_defined(const std::string& ident) {
                auto found = idents.find(ident);
                if (found == idents.end()) {
                    return std::nullopt;
                }
                return found->second.def->pos;
            }

            bool add_defvar(Field* f, Errors& errs) {
                auto& id = idents[f->ident];
                if (id.def != nullptr) {
                    error_defined_identifier(errs, f->ident, f->pos, id.def->pos);
                    return false;
                }
                f->belong = &id;
                id.def = f;
                id.global = global;
                return true;
            }

            bool add_ident(Ident* ident, bool unevaled) {
                auto& id = idents[ident->ident];
                if (id.def == nullptr) {
                    return false;
                }
                if (!unevaled && id.offset == 0) {
                    auto next = max_offset + id.def->type_->size;
                    if (next % id.def->type_->align) {
                        next += id.def->type_->align - next % id.def->type_->align;
                    }
                    id.offset = next;
                    max_offset = next;
                }
                ident->belong = &id;
                ident->expr_type = id.def->type_;
                id.idents.push_back(ident);
                return true;
            }
        };

        struct Func : Decl {
            std::shared_ptr<Type> return_;
            std::string ident;
            std::vector<std::unique_ptr<Field>> params;
            std::optional<Pos> va_arg;
            std::shared_ptr<Block> body;
            Variables variables;
            Callee* callee = nullptr;
            Func()
                : Decl(ObjectType::func) {}
        };

        struct FuncType : Type {
            std::shared_ptr<Type> return_;
            std::vector<std::shared_ptr<Type>> params;
            std::optional<Pos> va_arg;

            FuncType()
                : Type(ObjectType::func_type) {}
        };

        std::shared_ptr<FuncType> make_func_type(ast::Func* f) {
            auto ty = std::make_shared<FuncType>();
            ty->return_ = f->return_;
            for (auto& f : f->params) {
                ty->params.push_back(f->type_);
            }
            ty->va_arg = f->va_arg;
            return ty;
        }

        struct Funcs {
            std::map<std::string, Callee> funcs;

            std::optional<Pos> is_defined(const std::string& ident) {
                auto found = funcs.find(ident);
                if (found == funcs.end()) {
                    return std::nullopt;
                }
                if (found->second.body) {
                    return found->second.body->pos;
                }
                return found->second.decls.front()->pos;
            }

            bool add_func(Func* f, Errors& errs, bool has_body) {
                auto& fs = funcs[f->ident];
                if (has_body && fs.body != nullptr) {
                    error_defined_identifier(errs, f->ident, f->pos, f->body->pos);
                    return false;
                }
                f->callee = &fs;
                fs.body = f;
                fs.type = make_func_type(f);
                if (has_body) {
                    fs.body = f;
                }
                else {
                    fs.decls.push_back(f);
                }
                return true;
            }

            bool add_ident(Ident* ident) {
                auto f = funcs.find(ident->ident);
                if (f == funcs.end()) {
                    return false;
                }
                auto& fs = f->second;
                ident->callee = &fs;
                ident->expr_type = fs.type;
                fs.idents.push_back(ident);
                return true;
            }
        };

        struct Global {
            Variables vars;
            Funcs funcs;
            std::vector<StrLiteral*> strs;
            std::map<std::string, std::shared_ptr<Type>> literal_type;

            std::shared_ptr<ast::Type> get_literal_type(const std::string& k) {
                auto& v = literal_type[k];
                if (v) {
                    return v;
                }
                if (k == "intptr") {
                    auto i = std::make_shared<ast::IntType>();
                    i->ikind = ast::IntTypeKind::intptr;
                    i->pos = {0, 0};
                    i->align = 8;
                    i->size = 8;
                    v = i;
                }
                else if (k == "size_t") {
                    auto i = std::make_shared<ast::IntType>();
                    i->ikind = ast::IntTypeKind::size;
                    i->pos = {0, 0};
                    i->align = 8;
                    i->size = 8;
                    v = i;
                }
                else if (k == "bool" || k == "int") {
                    auto i = std::make_shared<ast::IntType>();
                    i->ikind = ast::IntTypeKind::int_;
                    i->pos = {0, 0};
                    i->align = 4;
                    i->size = 4;
                    v = i;
                }
                else if (k == "const char") {
                    auto i = std::make_shared<ast::IntType>();
                    i->ikind = ast::IntTypeKind::char_;
                    i->pos = {0, 0};
                    i->align = 1;
                    i->size = 1;
                    i->is_const = {0, 0};
                    v = i;
                }
                return v;
            }

            void add_str(StrLiteral* s) {
                strs.push_back(s);
            }

            bool add_ident(Ident* ident, bool unevaluted) {
                vars.global = true;
                if (funcs.add_ident(ident)) {
                    return true;
                }
                if (vars.add_ident(ident, unevaluted)) {
                    return true;
                }
                return false;
            }

            bool add_defvar(Field* f, Errors& errs) {
                vars.global = true;
                if (auto found = funcs.is_defined(f->ident)) {
                    error_defined_identifier(errs, f->ident, f->pos, *found);
                    return true;
                }
                return vars.add_defvar(f, errs);
            }

            bool add_func(Func* f, Errors& errs, bool has_body) {
                vars.global = true;
                if (auto found = vars.is_defined(f->ident)) {
                    error_defined_identifier(errs, f->ident, f->pos, *found);
                    return true;
                }
                return funcs.add_func(f, errs, has_body);
            }
        };

        struct Program : Object {
            std::vector<std::unique_ptr<Object>> objects;
            size_t lvar_max_offset = 0;
            Global global;

            Program()
                : Object(ObjectType::program) {}
        };

        struct TokenConsumer {
            std::vector<Token>& tokens;
            size_t i = 0;

            TokenConsumer(std::vector<Token>& t)
                : tokens(t) {}

           private:
            Variables* curvars = nullptr;
            Global* global = nullptr;
            bool unevaled = false;  // inner sizeof
            std::shared_ptr<Type> return_type;

           public:
            void set_global(Global* g) {
                this->global = g;
            }

            auto enter_unevaled() {
                auto prev = unevaled;
                unevaled = true;
                return helper::defer([&, prev] {
                    unevaled = prev;
                });
            }

            std::shared_ptr<Type> get_return_type() {
                return return_type;
            }

            std::shared_ptr<Type> get_literal_type(const std::string& s) {
                return this->global->get_literal_type(s);
            }

            auto enter_func(Variables* inner, const std::shared_ptr<Type>& r) {
                auto old = curvars;
                auto ret = return_type;
                curvars = inner;
                return_type = r;
                return helper::defer([&, old, ret] {
                    curvars = old;
                    return_type = ret;
                });
            }

            bool add_ident(Ident* ident, Errors& errs) {
                if (curvars->add_ident(ident, unevaled)) {
                    return true;
                }
                if (global->add_ident(ident, unevaled)) {
                    return true;
                }
                errs.push(Error{"identifier " + ident->ident + " is not defined", ident->pos});
                return false;
            }

            bool add_defvar(Field* f, Errors& errs) {
                if (!curvars) {
                    return global->add_defvar(f, errs);
                }
                return curvars->add_defvar(f, errs);
            }

            bool add_func(Func* f, Errors& errs, bool has_body) {
                return global->add_func(f, errs, has_body);
            }

            void add_str(StrLiteral* s) {
                global->add_str(s);
            }

            auto fallback() {
                auto fb = i;
                return helper::defer([&, fb] {
                    i = fb;
                });
            }

            bool eof() {
                return tokens.size() == i;
            }

            bool expect_tag(const char* tag) {
                if (eof()) {
                    return false;
                }
                return tokens[i].tag == tag;
            }

            bool consume_token(const char* token) {
                if (eof()) {
                    return false;
                }
                if (tokens[i].token == token) {
                    consume();
                    return true;
                }
                return false;
            }

            bool expect_token(const char* token) {
                if (eof()) {
                    return false;
                }
                if (tokens[i].token == token) {
                    return true;
                }
                return false;
            }

            std::optional<size_t> expect_oneof(const char* const* list) {
                for (size_t i = 0; list[i]; i++) {
                    if (expect_token(list[i])) {
                        return i;
                    }
                }
                return std::nullopt;
            }

            // skip_white skip space,line and comment
            void skip_white() {
                while (expect_tag(k_space) || expect_tag(k_comment) || expect_tag(k_line)) {
                    consume();
                }
            }

            void consume_and_skip() {
                consume();
                skip_white();
            }

            void consume() {
                if (eof()) {
                    return;
                }
                i++;
            }

            std::string token() {
                if (eof()) {
                    return "<EOF>";
                }
                return tokens[i].token;
            }

            Pos pos() {
                if (eof()) {
                    return {tokens.back().pos.end, tokens.back().pos.end};
                }
                return tokens[i].pos;
            }
        };

        inline bool equal_type(Type* left, Type* right) {
            if (left == right) {
                return true;
            }
            if (left->type != right->type) {
                return false;
            }
            if (left->type == ObjectType::int_type) {
                return static_cast<ast::IntType*>(left)->ikind == static_cast<ast::IntType*>(right)->ikind;
            }
            if (left->type == ObjectType::pointer_type) {
                return equal_type(static_cast<ast::PointerType*>(left)->base.get(), static_cast<ast::PointerType*>(right)->base.get());
            }
            return false;
        }

        inline bool convertible_type(Type* to, Type* from) {
            if (to->type == ObjectType::pointer_type && from->type == ObjectType::pointer_type) {
                return equal_type(deref(to).get(), deref(from).get());
            }
            return to->type == from->type;
        }

        inline bool is_boolean_convertible(Type* t) {
            if (t->type == ObjectType::int_type ||
                t->type == ObjectType::pointer_type) {
                return true;
            }
            return false;
        }

        inline bool check_bool(Errors& errs, Type* t, Pos p) {
            if (!is_boolean_convertible(t)) {
                errs.push(Error{
                    "not boolean convertible expr",
                    p,
                });
                return false;
            }
            return true;
        }

        inline std::shared_ptr<ast::Type> decide_unary_type(TokenConsumer& consumer, Errors& errs, ast::Unary* u) {
            auto replace_type = [&] {
                if (auto ptr = array_to_ptr(u->target->expr_type)) {
                    u->target->expr_type = ptr;
                }
            };
            switch (u->op) {
                case ast::UnaryOp::plus_sign:
                case ast::UnaryOp::minus_sign: {
                    replace_type();
                    return u->target->expr_type;
                }
                case ast::UnaryOp::address: {
                    auto ptr = std::make_shared<ast::PointerType>();
                    ptr->base = u->target->expr_type;
                    ptr->pos = {0, 0};
                    ptr->align = 8;
                    ptr->size = 8;
                    return ptr;
                }
                case ast::UnaryOp::deref: {
                    replace_type();
                    auto d = ast::deref(u->target->expr_type.get());
                    if (!d) {
                        errs.push(Error{"not derefable expr", u->pos});
                        return nullptr;
                    }
                    return d;
                }
                case ast::UnaryOp::size: {
                    return consumer.get_literal_type("size_t");
                }
                case ast::UnaryOp::logical_not: {
                    replace_type();
                    if (!check_bool(errs, u->target->expr_type.get(), u->target->pos)) {
                        return nullptr;
                    }
                    return consumer.get_literal_type("bool");
                }
                case ast::UnaryOp::increment:
                case ast::UnaryOp::decrement:
                case ast::UnaryOp::post_increment:
                case ast::UnaryOp::post_decrement: {
                    if (u->target->expr_type->type != ast::ObjectType::int_type &&
                        u->target->expr_type->type != ast::ObjectType::pointer_type) {
                        errs.push(Error{"not integer or pointer", u->pos});
                        return nullptr;
                    }
                    return u->target->expr_type;
                }
                default: {
                    errs.push(Error{"not supported expr ", u->pos});
                    return nullptr;
                }
            }
        }

        inline bool check_arguments(Errors& errs, std::vector<std::shared_ptr<ast::Type>>& params, size_t& count, ast::Expr* args, bool va_arg) {
            if (args) {
                if (args->type == ast::ObjectType::binary &&
                    static_cast<ast::Binary*>(args)->op == ast::BinaryOp::comma) {
                    auto b = static_cast<ast::Binary*>(args);
                    // eval right to left
                    // call(x1,x2,x3)
                    // then eval x3 -> x2 -> x1
                    if (!check_arguments(errs, params, count, b->left.get(), va_arg) ||
                        !check_arguments(errs, params, count, b->right.get(), va_arg)) {
                        return false;
                    }
                }
                else {
                    bool on_va_arg = false;
                    if (count >= params.size()) {
                        if (!va_arg) {
                            errs.push(Error{
                                "too many arguments for callee",
                                args->pos,
                            });
                            return false;
                        }
                        on_va_arg = true;
                    }
                    if (auto ptr = array_to_ptr(args->expr_type)) {
                        args->expr_type = std::move(ptr);
                    }
                    if (!on_va_arg) {
                        if (!ast::convertible_type(params[count].get(), args->expr_type.get())) {
                            errs.push(Error{
                                "type mismatch",
                                args->pos,
                            });
                            errs.push(Error{
                                "should match to this",
                                params[count]->pos,
                            });
                            return false;
                        }
                    }
                    count++;
                }
            }
            return true;
        }

        inline std::shared_ptr<ast::Type> decide_call_type(TokenConsumer& consumer, Errors& errs, ast::Call* c) {
            if (c->callee->expr_type->type != ObjectType::func_type) {
                errs.push(Error{"callee is not callable", c->pos});
                return nullptr;
            }

            auto f = static_cast<FuncType*>(c->callee->expr_type.get());
            size_t count = 0;
            if (!check_arguments(errs, f->params, count, c->arguments.get(), (bool)f->va_arg)) {
                return nullptr;
            }
            if (f->va_arg) {
                if (f->params.size() > count) {
                    errs.push(Error{
                        "expect more than " +
                            futils::number::to_string<std::string>(f->params.size()) + " arguments but got " +
                            futils::number::to_string<std::string>(count) + " arguments",
                        c->pos});
                    return nullptr;
                }
            }
            else {
                if (f->params.size() != count) {
                    errs.push(Error{
                        "expect " +
                            futils::number::to_string<std::string>(f->params.size()) + " arguments but got " +
                            futils::number::to_string<std::string>(count) + " arguments",
                        c->pos});
                    return nullptr;
                }
            }
            return f->return_;
        }

        inline std::shared_ptr<ast::Type> decide_index_type(TokenConsumer& consumer, Errors& errs, ast::Index* index) {
            if (auto ptr = array_to_ptr(index->object->expr_type)) {
                index->object->expr_type = ptr;  // replace; this may holds original information so needless to care of
            }
            if (auto ptr = array_to_ptr(index->index->expr_type)) {
                index->index->expr_type = ptr;
            }
            auto lt = index->object->expr_type->type;
            auto rt = index->index->expr_type->type;
            if (lt == ast::ObjectType::pointer_type &&
                rt == ast::ObjectType::int_type) {
                return deref(index->object->expr_type.get());
            }
            if (rt == ast::ObjectType::pointer_type &&
                lt == ast::ObjectType::int_type) {
                return deref(index->index->expr_type.get());
            }
            errs.push(Error{"no pointer exists", index->pos});
            return nullptr;
        }

        inline std::shared_ptr<ast::Type> decide_binary_type(TokenConsumer& consumer, Errors& errs, ast::Binary* b) {
            if (auto ptr = array_to_ptr(b->left->expr_type)) {
                b->left->expr_type = ptr;  // replace; this may holds original information so needless to care of
            }
            if (auto ptr = array_to_ptr(b->right->expr_type)) {
                b->right->expr_type = ptr;
            }
            if (b->op == ast::BinaryOp::logical_and ||
                b->op == ast::BinaryOp::logical_or) {
                if (!check_bool(errs, b->left->expr_type.get(), b->left->pos)) {
                    return nullptr;
                }
                if (!check_bool(errs, b->right->expr_type.get(), b->right->pos)) {
                    return nullptr;
                }
                return consumer.get_literal_type("bool");
            }
            auto lt = b->left->expr_type->type;
            auto rt = b->right->expr_type->type;
            auto both_int = [&] {
                return rt == ast::ObjectType::int_type && lt == ast::ObjectType::int_type;
            };
            if (b->op == ast::BinaryOp::add) {
                if (lt == ast::ObjectType::pointer_type &&
                    rt == ast::ObjectType::int_type) {
                    return b->left->expr_type;
                }
                if (rt == ast::ObjectType::pointer_type &&
                    lt == ast::ObjectType::int_type) {
                    return b->right->expr_type;
                }
                if (both_int()) {
                    auto lv = ast::as_int(b->left->expr_type.get());
                    auto rv = ast::as_int(b->right->expr_type.get());
                    return b->left->expr_type;  // currently only int is available
                }
                return nullptr;
            }
            if (b->op == ast::BinaryOp::sub) {
                if (lt == ast::ObjectType::pointer_type &&
                    rt == ast::ObjectType::int_type) {
                    return b->left->expr_type;
                }
                if (lt == ast::ObjectType::pointer_type &&
                    rt == ast::ObjectType::pointer_type) {
                    if (!ast::equal_type(b->left->expr_type.get(), b->right->expr_type.get())) {
                        errs.push(Error{"not equal type", b->pos});
                        return nullptr;
                    }
                    return consumer.get_literal_type("intptr");
                }
                if (both_int()) {
                    auto lv = ast::as_int(b->left->expr_type.get());
                    auto rv = ast::as_int(b->right->expr_type.get());
                    return b->left->expr_type;  // currently only int is available
                }
                return nullptr;
            }
            if (b->op == ast::BinaryOp::mul ||
                b->op == ast::BinaryOp::div ||
                b->op == ast::BinaryOp::mod) {
                if (both_int()) {
                    auto lv = ast::as_int(b->left->expr_type.get());
                    auto rv = ast::as_int(b->right->expr_type.get());
                    return b->left->expr_type;  // currently only int is available
                }
                return nullptr;
            }
            if (b->op == ast::BinaryOp::equal ||
                b->op == ast::BinaryOp::not_equal ||
                b->op == ast::BinaryOp::less ||
                b->op == ast::BinaryOp::less_or_eq) {
                if (!ast::equal_type(b->left->expr_type.get(), b->right->expr_type.get())) {
                    errs.push(Error{"not equal type", b->pos});
                    return nullptr;
                }
                return consumer.get_literal_type("bool");
            }
            if (b->op == ast::BinaryOp::assign) {
                if (!ast::convertible_type(b->left->expr_type.get(), b->right->expr_type.get())) {
                    errs.push(Error{"not equal type; cannot assign", b->pos});
                    return nullptr;
                }
                return b->left->expr_type;
            }
            if (b->op == ast::BinaryOp::add_assign || b->op == ast::BinaryOp::sub_assign) {
                if (lt == ast::ObjectType::pointer_type &&
                    rt == ast::ObjectType::int_type) {
                    return b->left->expr_type;  // special case
                }
                if (!ast::convertible_type(b->left->expr_type.get(), b->right->expr_type.get())) {
                    errs.push(Error{"not equal type; cannot assign", b->pos});
                    return nullptr;
                }
                return b->left->expr_type;
            }
            if (b->op == ast::BinaryOp::comma) {
                return b->right->expr_type;
            }
            return nullptr;
        }

        inline std::optional<std::unique_ptr<Literal>> parse_literal(TokenConsumer& consumer, Errors& errs) {
            if (consumer.expect_tag(k_int_lit)) {
                auto int_lit = std::make_unique<IntLiteral>();
                int_lit->raw = consumer.token();
                int_lit->pos = consumer.pos();
                int_lit->expr_type = consumer.get_literal_type("int");
                consumer.consume();
                return int_lit;
            }
            if (consumer.expect_tag(k_str_lit)) {
                auto str_lit = std::make_unique<StrLiteral>();
                auto lit = clang_escape(consumer.token());
                if (!lit) {
                    errs.push(Error{"escape failed " + consumer.token(), consumer.pos()});
                    return nullptr;
                }
                str_lit->str = std::move(lit->first);
                str_lit->pos = consumer.pos();
                auto arrty = std::make_shared<ArrayType>();
                arrty->base = consumer.get_literal_type("const char");
                arrty->array_size = lit->second + 1;  // with '\0'
                arrty->align = arrty->base->align;
                arrty->size = arrty->base->size * arrty->array_size;
                arrty->pos = str_lit->pos;
                str_lit->expr_type = std::move(arrty);
                consumer.add_str(str_lit.get());
                consumer.consume();
                return str_lit;
            }
            return std::nullopt;
        }

        inline std::optional<std::unique_ptr<Ident>> parse_ident(TokenConsumer& consumer, Errors& errs) {
            if (consumer.expect_tag(k_ident)) {
                auto ident = std::make_unique<Ident>();
                ident->ident = consumer.token();
                ident->pos = consumer.pos();
                if (!consumer.add_ident(ident.get(), errs)) {
                    return nullptr;
                }
                consumer.consume();
                return ident;
            }
            return std::nullopt;
        }

        inline std::unique_ptr<Expr> parse_expr(TokenConsumer& consumer, Errors& errs);

        inline std::optional<std::unique_ptr<Paren>> parse_paren(TokenConsumer& consumer, Errors& errs, bool force) {
            if (!consumer.expect_token("(")) {
                if (force) {
                    errs.push(Error{"expect token ( but found " + consumer.token(), consumer.pos()});
                    return nullptr;
                }
                return std::nullopt;
            }
            auto pos = consumer.pos();
            consumer.consume();
            auto expr = parse_expr(consumer, errs);
            if (!expr) {
                return nullptr;
            }
            consumer.skip_white();
            if (!consumer.expect_token(")")) {
                errs.push(Error{"expect token ) but found " + consumer.token(), consumer.pos()});
                return nullptr;
            }
            auto p = std::make_unique<Paren>();
            p->expr = std::move(expr);
            p->expr_type = p->expr->expr_type;
            p->end_pos = consumer.pos();
            p->pos = pos;
            consumer.consume();
            return p;
        }

        inline std::unique_ptr<Expr> parse_prim(TokenConsumer& consumer, Errors& errs) {
            std::unique_ptr<Expr> expr;
            if (auto paren = parse_paren(consumer, errs, false)) {
                if (!*paren) {
                    return nullptr;  // error
                }
                expr = std::move(*paren);
            }
            if (auto lit = parse_literal(consumer, errs)) {
                if (!*lit) {
                    return nullptr;  // error
                }
                expr = std::move(*lit);
            }
            if (!expr) {
                if (auto id = parse_ident(consumer, errs)) {
                    if (!*id) {
                        return nullptr;  // error
                    }
                    expr = std::move(*id);
                }
            }
            if (!expr) {
                errs.push({
                    "expect identifier or literal but found " + consumer.token(),
                    consumer.pos(),
                });
                return nullptr;
            }
            return expr;
        }

        inline std::optional<std::unique_ptr<Call>> parse_call(std::unique_ptr<Expr>& callee, TokenConsumer& consumer, Errors& errs) {
            consumer.skip_white();
            if (!consumer.expect_token("(")) {
                return std::nullopt;
            }
            auto pos = consumer.pos();
            consumer.consume_and_skip();
            std::unique_ptr<Expr> args;
            if (!consumer.expect_token(")")) {
                args = parse_expr(consumer, errs);
                if (!args) {
                    return nullptr;
                }
                consumer.skip_white();
            }
            if (!consumer.expect_token(")")) {
                errs.push(Error{"expect token ) but found " + consumer.token(), consumer.pos()});
                return nullptr;
            }
            auto c = std::make_unique<Call>();
            c->arguments = std::move(args);
            c->pos = pos;
            c->end_pos = consumer.pos();
            c->callee = std::move(callee);
            c->expr_type = decide_call_type(consumer, errs, c.get());
            if (!c->expr_type) {
                return nullptr;
            }
            consumer.consume();
            return c;
        }

        inline std::optional<std::unique_ptr<Index>> parse_index(std::unique_ptr<Expr>& object, TokenConsumer& consumer, Errors& errs) {
            consumer.skip_white();
            if (!consumer.expect_token("[")) {
                return std::nullopt;
            }
            auto pos = consumer.pos();
            consumer.consume_and_skip();
            std::unique_ptr<Expr> index;
            index = parse_expr(consumer, errs);
            if (!index) {
                return nullptr;
            }
            consumer.skip_white();
            if (!consumer.expect_token("]")) {
                errs.push(Error{"expect token ] but found " + consumer.token(), consumer.pos()});
                return nullptr;
            }
            auto c = std::make_unique<Index>();
            c->index = std::move(index);
            c->object = std::move(object);
            c->pos = pos;
            c->end_pos = consumer.pos();
            c->expr_type = decide_index_type(consumer, errs, c.get());
            if (!c->expr_type) {
                return nullptr;
            }
            consumer.consume();
            return c;
        }

        inline std::unique_ptr<Expr> parse_post(TokenConsumer& consumer, Errors& errs) {
            auto expr = parse_prim(consumer, errs);
            if (!expr) {
                return nullptr;
            }
            for (;;) {
                if (auto c = parse_call(expr, consumer, errs)) {
                    if (!*c) {
                        return nullptr;  // error
                    }
                    expr = std::move(*c);
                    continue;
                }
                else if (auto c = parse_index(expr, consumer, errs)) {
                    if (!*c) {
                        return nullptr;  // error
                    }
                    expr = std::move(*c);
                    continue;
                }
                else if (consumer.expect_token("++")) {
                    auto ua = std::make_unique<Unary>();
                    ua->op = UnaryOp::post_increment;
                    ua->target = std::move(expr);
                    ua->pos = consumer.pos();
                    consumer.consume();
                    ua->expr_type = decide_unary_type(consumer, errs, ua.get());
                    if (!ua->expr_type) {
                        return nullptr;  // error
                    }
                    expr = std::move(ua);
                    continue;
                }
                else if (consumer.expect_token("--")) {
                    auto ua = std::make_unique<Unary>();
                    ua->op = UnaryOp::post_decrement;
                    ua->target = std::move(expr);
                    ua->pos = consumer.pos();
                    consumer.consume();
                    ua->expr_type = decide_unary_type(consumer, errs, ua.get());
                    if (!ua->expr_type) {
                        return nullptr;  // error
                    }
                    expr = std::move(ua);
                    continue;
                }
                break;
            }
            return expr;
        }

        inline std::unique_ptr<Expr> parse_unary(TokenConsumer& consumer, Errors& errs) {
            consumer.skip_white();
            if (auto m = consumer.expect_oneof(ast::unary_op)) {
                auto unary = std::make_unique<Unary>();
                unary->op = ast::UnaryOp(*m);
                unary->pos = consumer.pos();
                consumer.consume_and_skip();
                if (unary->op == ast::UnaryOp::size) {
                    // sizeof
                    auto e = consumer.enter_unevaled();
                    unary->target = parse_unary(consumer, errs);
                }
                else {
                    unary->target = parse_unary(consumer, errs);
                }
                if (!unary->target) {
                    return nullptr;
                }
                unary->expr_type = decide_unary_type(consumer, errs, unary.get());
                if (!unary->expr_type) {
                    return nullptr;
                }
                return unary;
            }
            return parse_post(consumer, errs);
        }

        inline std::unique_ptr<Expr> parse_cast(TokenConsumer& consumer, Errors& errs) {
            return parse_unary(consumer, errs);
        }

        struct BinOpStack {
            size_t depth = 0;
            std::unique_ptr<Expr> expr;
        };

        inline std::unique_ptr<Expr> parse_expr(TokenConsumer& consumer, Errors& errs) {
            auto expr = parse_cast(consumer, errs);
            if (!expr) {
                return nullptr;
            }
            size_t depth = 0;
            std::vector<BinOpStack> stacks;
            for (; depth < ast::bin_layer_len;) {
                if (stacks.size() && stacks.back().depth == depth) {
                    auto op = std::move(stacks.back());
                    stacks.pop_back();
                    if (op.expr->type == ObjectType::binary) {
                        if (depth == ast::bin_assign_layer) {
                            stacks.push_back(std::move(op));
                        }
                        else {
                            auto b = static_cast<Binary*>(op.expr.get());
                            b->right = std::move(expr);
                            b->expr_type = decide_binary_type(consumer, errs, b);
                            if (!b->expr_type) {
                                return nullptr;
                            }
                            expr = std::move(op.expr);
                        }
                    }
                    else if (op.expr->type == ObjectType::cond_op) {
                        auto cop = static_cast<CondOp*>(op.expr.get());
                        if (!cop->then_) {
                            cop->then_ = std::move(expr);
                            if (!consumer.expect_token(":")) {
                                errs.push(Error{.err = "expect : but found " + consumer.token(), .pos = consumer.pos()});
                                return nullptr;
                            }
                            cop->els_pos = consumer.pos();
                            stacks.push_back(std::move(op));
                            consumer.consume_and_skip();
                            depth = 0;
                            expr = parse_cast(consumer, errs);
                            continue;
                        }
                        else {
                            stacks.push_back(std::move(op));
                        }
                    }
                }
                consumer.skip_white();
                if (depth == ast::bin_cond_layer) {
                    if (consumer.expect_token("?")) {
                        auto cop = std::make_unique<CondOp>();
                        cop->cond = std::move(expr);
                        cop->pos = consumer.pos();
                        stacks.push_back(BinOpStack{.depth = depth, .expr = std::move(cop)});
                        consumer.consume_and_skip();
                        depth = 0;
                        expr = parse_cast(consumer, errs);
                        if (!expr) {
                            return nullptr;
                        }
                        continue;
                    }
                }
                else {
                    if (auto m = consumer.expect_oneof(ast::bin_layers[depth])) {
                        Pos pos = consumer.pos();
                        auto fallback = consumer.fallback();
                        consumer.consume_and_skip();
                        if (depth == ast::bin_comma_layer && consumer.expect_token("}")) {
                            break;  // no more expr exists
                        }
                        fallback.cancel();
                        auto b = std::make_unique<Binary>();
                        b->pos = pos;
                        b->op = *ast::bin_op(ast::bin_layers[depth][*m]);
                        if (depth == 0) {
                            b->left = std::move(expr);
                            b->right = parse_cast(consumer, errs);
                            if (!b->right) {
                                return nullptr;
                            }
                            b->expr_type = decide_binary_type(consumer, errs, b.get());
                            if (!b->expr_type) {
                                return nullptr;
                            }
                            expr = std::move(b);
                        }
                        else {
                            b->left = std::move(expr);
                            stacks.push_back(BinOpStack{.depth = depth, .expr = std::move(b)});
                            depth = 0;
                            expr = parse_cast(consumer, errs);
                            if (!expr) {
                                return nullptr;
                            }
                        }
                        continue;
                    }
                }
                if (depth == ast::bin_cond_layer) {
                    while (stacks.size() &&
                           stacks.back().depth == ast::bin_cond_layer) {
                        auto op = std::move(stacks.back());
                        stacks.pop_back();
                        auto cop = static_cast<CondOp*>(op.expr.get());
                        if (!cop->then_) {
                            errs.push(Error{.err = "unexpected state, parser errpr", .pos = consumer.pos()});
                            return nullptr;
                        }
                        cop->else_ = std::move(expr);
                        expr = std::move(op.expr);
                    }
                }
                else if (depth == ast::bin_assign_layer) {
                    while (stacks.size() &&
                           stacks.back().depth == ast::bin_assign_layer) {
                        auto op = std::move(stacks.back());
                        stacks.pop_back();
                        auto b = static_cast<Binary*>(op.expr.get());
                        b->right = std::move(expr);
                        b->expr_type = decide_binary_type(consumer, errs, b);
                        if (!b->expr_type) {
                            return nullptr;
                        }
                        expr = std::move(op.expr);
                    }
                }
                depth++;
            }
            return expr;
        }

        template <class T>
        inline std::optional<T> eval_const_expr(ast::Expr* expr) {
            if (expr->type == ObjectType::int_literal) {
                return static_cast<IntLiteral*>(expr)->parse_as<T>();
            }
            if (expr->type == ObjectType::binary) {
                auto b = static_cast<Binary*>(expr);
                auto both_eval = [&](auto&& op) -> std::optional<T> {
                    auto l = eval_const_expr<T>(b->left.get());
                    auto r = eval_const_expr<T>(b->right.get());
                    if (!l || !r) {
                        return std::nullopt;
                    }
                    return op(*l, *r);
                };
                switch (b->op) {
                    case BinaryOp::add:
                        return both_eval([](auto& a, auto& b) { return a + b; });
                    case BinaryOp::sub:
                        return both_eval([](auto& a, auto& b) { return a - b; });
                    case BinaryOp::mul:
                        return both_eval([](auto& a, auto& b) { return a + b; });
                    case BinaryOp::div:
                        return both_eval([](auto& a, auto& b) -> std::optional<T> {if(b==0){return std::nullopt;} return a / b; });
                    case BinaryOp::mod:
                        return both_eval([](auto& a, auto& b) -> std::optional<T> {if(b==0){return std::nullopt;} return a % b; });
                    default:
                        break;
                }
            }
            else if (expr->type == ObjectType::unary) {
                auto u = static_cast<Unary*>(expr);
                switch (u->op) {
                    case UnaryOp::plus_sign: {
                        return eval_const_expr<T>(u->target.get());
                    }
                    case UnaryOp::minus_sign: {
                        if (auto v = eval_const_expr<T>(u->target.get())) {
                            return -*v;
                        }
                        return std::nullopt;
                    }
                    default:
                        break;
                }
            }
            return std::nullopt;
        }

        inline std::shared_ptr<Type> parse_type_pre(TokenConsumer& consumer, Errors& errs) {
            std::shared_ptr<Type> ty;
            std::optional<Pos> is_const;
            while (consumer.expect_token("const")) {
                is_const = consumer.pos();
                consumer.consume_and_skip();
            }
            if (consumer.expect_token("int")) {
                auto v = std::make_shared<IntType>();
                v->pos = consumer.pos();
                v->ikind = IntTypeKind::int_;
                v->size = 4;
                v->align = 4;
                ty = std::move(v);
            }
            else if (consumer.expect_token("char")) {
                auto v = std::make_shared<IntType>();
                v->pos = consumer.pos();
                v->ikind = IntTypeKind::char_;
                v->size = 1;
                v->align = 1;
                ty = std::move(v);
            }
            else if (consumer.expect_token("void")) {
                auto v = std::make_shared<VoidType>();
                v->pos = consumer.pos();
                v->size = 0;
                v->align = 0;
                ty = std::move(v);
            }
            else {
                return nullptr;
            }
            ty->is_const = std::move(is_const);
            consumer.consume();
            std::shared_ptr<Type> b = std::move(ty);
            for (;;) {
                consumer.skip_white();
                if (consumer.expect_token("*")) {
                    auto ptr = std::make_shared<PointerType>();
                    ptr->pos = consumer.pos();
                    ptr->base = std::move(b);
                    ptr->size = 8;
                    ptr->align = 8;
                    b = std::move(ptr);
                    consumer.consume();
                    continue;
                }
                if (consumer.expect_token("const")) {
                    ty->is_const = consumer.pos();
                    consumer.consume();
                    continue;
                }
                break;
            }
            return b;
        }

        inline bool parse_type_post(std::shared_ptr<Type>& b, TokenConsumer& consumer, Errors& errs) {
            for (;;) {
                consumer.skip_white();
                if (consumer.expect_token("[")) {
                    auto arr = std::make_shared<ArrayType>();
                    arr->pos = consumer.pos();
                    arr->base = std::move(b);
                    arr->align = arr->base->align;
                    consumer.consume_and_skip();
                    arr->array_size_expr = parse_expr(consumer, errs);
                    if (!arr->array_size_expr) {
                        return false;
                    }
                    auto size = eval_const_expr<size_t>(arr->array_size_expr.get());
                    if (!size) {
                        errs.push(Error{"not constant evaluted", arr->array_size_expr->pos});
                        return false;
                    }
                    arr->array_size = *size;
                    arr->size = arr->base->size * arr->array_size;
                    consumer.skip_white();
                    if (!consumer.consume_token("]")) {
                        errs.push(Error{"expect ] but found " + consumer.token(), consumer.pos()});
                        return false;
                    }
                    b = std::move(arr);
                    continue;
                }
                break;
            }
            return true;
        }

        struct DeclPre {
            std::shared_ptr<Type> type;
            std::string ident;
            Pos ident_pos;
        };

        inline std::optional<DeclPre> parse_decl_pre(TokenConsumer& consumer, Errors& errs) {
            auto type = parse_type_pre(consumer, errs);
            if (!type) {
                return std::nullopt;
            }
            consumer.skip_white();
            if (!consumer.expect_tag(k_ident)) {
                return std::nullopt;
            }
            DeclPre pre;
            pre.type = std::move(type);
            pre.ident = consumer.token();
            pre.ident_pos = consumer.pos();
            consumer.consume();
            return pre;
        }

        inline std::unique_ptr<Field> parse_field_post(DeclPre& pre, TokenConsumer& consumer, Errors& errs) {
            auto field = std::make_unique<Field>();
            field->type_ = std::move(pre.type);
            field->ident = std::move(pre.ident);
            field->pos = pre.ident_pos;
            if (!parse_type_post(field->type_, consumer, errs)) {
                return nullptr;
            }
            return field;
        }

        inline std::optional<std::unique_ptr<Field>> parse_field(TokenConsumer& consumer, Errors& errs) {
            auto pre = parse_decl_pre(consumer, errs);
            if (!pre) {
                return std::nullopt;
            }
            return parse_field_post(*pre, consumer, errs);
        }

        inline std::optional<std::unique_ptr<Field>> parse_defvar(TokenConsumer& consumer, Errors& errs) {
            auto f = parse_field(consumer, errs);
            if (!f) {
                return std::nullopt;
            }
            if (!*f) {
                return nullptr;
            }
            if (!consumer.add_defvar(f->get(), errs)) {
                return nullptr;
            }
            return f;
        }

        inline std::unique_ptr<Object> parse_block_inner(TokenConsumer& consumer, Errors& errs);
        inline std::optional<std::unique_ptr<Block>> parse_block(TokenConsumer& consumer, Errors& errs, bool force);

        inline std::optional<std::unique_ptr<Func>> parse_func(DeclPre& pre, TokenConsumer& consumer, Errors& errs) {
            consumer.skip_white();
            if (!consumer.consume_token("(")) {
                return std::nullopt;
            }
            consumer.skip_white();
            auto fn = std::make_unique<Func>();
            fn->return_ = std::move(pre.type);
            fn->pos = pre.ident_pos;
            fn->ident = std::move(pre.ident);
            bool next = false;
            while (!consumer.consume_token(")")) {
                if (next) {
                    if (!consumer.consume_token(",")) {
                        errs.push(Error{"expect token , but found " + consumer.token(), consumer.pos()});
                        return nullptr;
                    }
                }
                consumer.skip_white();
                if (consumer.expect_token("...")) {
                    fn->va_arg = consumer.pos();
                    consumer.consume_and_skip();
                    if (!consumer.consume_token(")")) {
                        errs.push(Error{"expect token ) but found " + consumer.token(), consumer.pos()});
                        return nullptr;
                    }
                    break;
                }
                auto f = parse_field(consumer, errs);
                if (!f) {
                    errs.push(Error{"expect field but found " + consumer.token(), consumer.pos()});
                    return nullptr;
                }
                if (!*f) {
                    return nullptr;
                }
                fn->params.push_back(std::move(*f));
                consumer.skip_white();
                next = true;
            }
            consumer.skip_white();
            auto has_body = consumer.expect_token("{");
            if (!consumer.add_func(fn.get(), errs, has_body)) {
                return nullptr;
            }
            if (has_body) {
                auto b = consumer.enter_func(&fn->variables, fn->return_);
                for (auto& param : fn->params) {
                    if (!consumer.add_defvar(param.get(), errs)) {
                        return nullptr;
                    }
                }
                fn->body = *parse_block(consumer, errs, true);
                if (!fn->body) {
                    return nullptr;
                }
            }
            return fn;
        }

        inline std::optional<std::unique_ptr<Decl>> parse_global_decl(TokenConsumer& consumer, Errors& errs) {
            auto pre = parse_decl_pre(consumer, errs);
            if (!pre) {
                return std::nullopt;
            }
            if (auto fn = parse_func(*pre, consumer, errs)) {
                return std::move(*fn);
            }
            auto f = parse_field_post(*pre, consumer, errs);
            if (!f) {
                return nullptr;
            }
            if (!consumer.add_defvar(f.get(), errs)) {
                return nullptr;
            }
            return f;
        }

        inline bool check_return_type(TokenConsumer& consumer, Errors& errs, ast::Expr* expr) {
            auto ret = consumer.get_return_type();
            if (!expr) {
                if (ret->type != ast::ObjectType::void_type) {
                    errs.push(Error{"not expect expr but found", expr->pos});
                    return false;
                }
            }
            if (!ast::convertible_type(ret.get(), expr->expr_type.get())) {
                errs.push(Error{"expr type not matched to return type", expr->pos});
                errs.push(Error{"should match to this", ret->pos});
                return false;
            }
            return true;
        }

        inline std::optional<std::unique_ptr<Return>> parse_return(TokenConsumer& consumer, Errors& errs) {
            if (consumer.expect_token("return")) {
                consumer.consume_and_skip();
                auto r = std::make_unique<Return>();
                if (!consumer.expect_token(";")) {
                    r->value = parse_expr(consumer, errs);
                    if (!r->value) {
                        return nullptr;
                    }
                }
                if (!check_return_type(consumer, errs, r->value.get())) {
                    return nullptr;
                }
                return r;
            }
            return std::nullopt;
        }

        inline std::unique_ptr<Object> parse_ctrl_body(TokenConsumer& consumer, Errors& errs) {
            return parse_block_inner(consumer, errs);
        }

        inline std::optional<std::unique_ptr<WhileStmt>> parse_while(TokenConsumer& consumer, Errors& errs) {
            if (!consumer.expect_token("while")) {
                return std::nullopt;
            }
            auto root = std::make_unique<WhileStmt>();
            root->pos = consumer.pos();
            consumer.consume_and_skip();
            root->cond = *parse_paren(consumer, errs, true);  // on force mode, non-nullopt is returned
            if (!root->cond) {
                return nullptr;
            }
            if (!check_bool(errs, root->cond->expr_type.get(), root->pos)) {
                return nullptr;
            }
            consumer.skip_white();
            root->body = parse_ctrl_body(consumer, errs);
            if (!root->body) {
                return nullptr;
            }
            return root;
        }

        inline bool parse_for_paren(TokenConsumer& consumer, Errors& errs,
                                    std::unique_ptr<Object>& init,
                                    std::unique_ptr<Expr>& cond,
                                    std::unique_ptr<Expr>& cont) {
            if (!consumer.expect_token("(")) {
                errs.push(Error{"expect token ( but found " + consumer.token(), consumer.pos()});
                return false;
            }
            auto pos = consumer.pos();
            consumer.consume();
            consumer.skip_white();
            if (!consumer.expect_token(";")) {
                init = parse_expr(consumer, errs);
                if (!init) {
                    return false;
                }
                consumer.skip_white();
            }
            if (!consumer.consume_token(";")) {
                errs.push(Error{"expect token ; but found " + consumer.token(), consumer.pos()});
                return false;
            }
            consumer.skip_white();
            if (!consumer.expect_token(";")) {
                cond = parse_expr(consumer, errs);
                if (!cond) {
                    return false;
                }
                if (!check_bool(errs, cond->expr_type.get(), cond->pos)) {
                    return false;
                }
                consumer.skip_white();
            }
            if (!consumer.consume_token(";")) {
                errs.push(Error{"expect token ; but found " + consumer.token(), consumer.pos()});
                return false;
            }
            consumer.skip_white();
            if (!consumer.expect_token(")")) {
                cont = parse_expr(consumer, errs);
                if (!cont) {
                    return false;
                }
                consumer.skip_white();
            }
            if (!consumer.consume_token(")")) {
                errs.push(Error{"expect token ) but found " + consumer.token(), consumer.pos()});
                return false;
            }
            return true;
        }

        inline std::optional<std::unique_ptr<ForStmt>> parse_for(TokenConsumer& consumer, Errors& errs) {
            if (!consumer.expect_token("for")) {
                return std::nullopt;
            }
            auto root = std::make_unique<ForStmt>();
            root->pos = consumer.pos();
            consumer.consume_and_skip();
            if (!parse_for_paren(consumer, errs, root->init, root->cond, root->cont)) {
                return nullptr;
            }
            consumer.skip_white();
            root->body = parse_ctrl_body(consumer, errs);
            if (!root->body) {
                return nullptr;
            }
            return root;
        }

        inline std::optional<std::unique_ptr<IfStmt>> parse_if(TokenConsumer& consumer, Errors& errs) {
            if (!consumer.expect_token("if")) {
                return std::nullopt;
            }
            auto root = std::make_unique<IfStmt>();
            root->pos = consumer.pos();
            consumer.consume_and_skip();
            root->cond = *parse_paren(consumer, errs, true);  // on force mode, non-nullopt is returned
            if (!root->cond) {
                return nullptr;
            }
            if (!check_bool(errs, root->cond->expr_type.get(), root->cond->pos)) {
                return nullptr;
            }
            consumer.skip_white();
            root->then = parse_ctrl_body(consumer, errs);
            if (!root->then) {
                return nullptr;
            }
            consumer.skip_white();
            if (consumer.expect_token("else")) {
                consumer.consume_and_skip();
                root->els_ = parse_ctrl_body(consumer, errs);
                if (!root->els_) {
                    return nullptr;
                }
            }
            return root;
        }

        inline std::unique_ptr<Object> parse_block_inner(TokenConsumer& consumer, Errors& errs) {
            std::unique_ptr<Object> obj;
            if (consumer.expect_tag(k_unspec)) {
                errs.push(Error{.err = "unspecific token: " + escape::escape_str<std::string>(consumer.token()), .pos = consumer.pos()});
                return nullptr;
            }
            bool need_term = true;
            if (auto block = parse_block(consumer, errs, false)) {
                obj = std::move(*block);
                need_term = false;
            }
            else if (auto ret = parse_return(consumer, errs)) {
                obj = std::move(*ret);
            }
            else if (auto if_ = parse_if(consumer, errs)) {
                obj = std::move(*if_);
                need_term = false;
            }
            else if (auto while_ = parse_while(consumer, errs)) {
                obj = std::move(*while_);
                need_term = false;
            }
            else if (auto for_ = parse_for(consumer, errs)) {
                obj = std::move(*for_);
                need_term = false;
            }
            else if (auto def = parse_defvar(consumer, errs)) {
                obj = std::move(*def);
            }
            else {
                auto expr = parse_expr(consumer, errs);
                if (!expr) {
                    return nullptr;
                }
                obj = std::move(expr);
            }
            if (!obj) {
                return nullptr;
            }
            if (need_term) {
                if (!consumer.consume_token(";")) {
                    errs.push(Error{.err = "expect ; but found " + consumer.token(), .pos = consumer.pos()});
                    return nullptr;
                }
            }
            consumer.skip_white();
            return obj;
        }

        inline std::optional<std::unique_ptr<Block>> parse_block(TokenConsumer& consumer, Errors& errs, bool force) {
            if (!consumer.expect_token("{")) {
                if (force) {
                    errs.push(Error{"expect { but found " + consumer.token(), consumer.pos()});
                    return nullptr;
                }
                return std::nullopt;
            }
            auto pos = consumer.pos();
            consumer.consume_and_skip();
            std::vector<std::unique_ptr<Object>> objects;
            while (!consumer.eof() && !consumer.expect_token("}")) {
                auto obj = parse_block_inner(consumer, errs);
                if (!obj) {
                    return nullptr;
                }
                objects.push_back(std::move(obj));
            }
            if (!consumer.expect_token("}")) {
                errs.push(Error{"expect } but found " + consumer.token(), consumer.pos()});
                return nullptr;
            }
            auto end_pos = consumer.pos();
            consumer.consume();
            auto block = std::make_unique<Block>();
            block->pos = pos;
            block->end_pos = end_pos;
            block->objects = std::move(objects);
            return block;
        }

        inline std::unique_ptr<Object> parse_global(TokenConsumer& consumer, Errors& errs) {
            std::unique_ptr<Object> obj;
            bool need_term = false;
            if (auto f = parse_global_decl(consumer, errs)) {
                if (!*f) {
                    return nullptr;
                }
                auto& v = *f;
                need_term = v->type == ast::ObjectType::field ||
                            (v->type == ast::ObjectType::func &&
                             static_cast<Func*>(v.get())->body == nullptr);
                obj = std::move(*f);
            }
            else {
                errs.push(Error{"expect function or global variable but found " + consumer.token(), consumer.pos()});
                return nullptr;
            }
            if (need_term) {
                if (!consumer.consume_token(";")) {
                    errs.push(Error{.err = "expect ; but found " + consumer.token(), .pos = consumer.pos()});
                    return nullptr;
                }
            }
            return obj;
        }

        inline std::unique_ptr<Program> parse_c(std::vector<Token>& tokens, Errors& errs) {
            auto consumer = TokenConsumer{tokens};
            consumer.skip_white();
            auto prog = std::make_unique<Program>();
            consumer.set_global(&prog->global);
            while (!consumer.eof()) {
                auto obj = parse_global(consumer, errs);
                if (!obj) {
                    return nullptr;
                }
                prog->objects.push_back(std::move(obj));
                consumer.skip_white();
            }
            if (tokens.size()) {
                prog->pos = Pos{tokens.front().pos.begin, tokens.back().pos.end};
            }
            return prog;
        }
    }  // namespace ast
}  // namespace futils::langc
