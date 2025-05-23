/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <deprecated/parser/expr/command_expr.h>
#include <deprecated/parser/expr/jsoncvt.h>
#include <wrap/light/string.h>
#include <wrap/light/vector.h>
#include <wrap/light/hash_map.h>
#include <variant>
#include <deprecated/stream/stream.h>

namespace minilang {
    using namespace futils;
    namespace expr = futils::parser::expr;

    enum class TypeKind {
        primitive,
        ptr,
        array,
    };

    struct TypeExpr : expr::Expr {
        TypeExpr(TypeKind k, size_t pos)
            : kind(k), expr::Expr("type", pos) {}
        wrap::string val;
        TypeKind kind;
        expr::Expr* next = nullptr;
        expr::Expr* expr = nullptr;

        expr::Expr* index(size_t i) const override {
            if (i == 0) return next;
            if (i == 1) return expr;
            return nullptr;
        }

        bool stringify(expr::PushBacker pb) const override {
            strutil::append(pb, val);
            return true;
        }
    };

    auto define_type(auto exp) {
        auto fn = [=]<class T>(auto& self, Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) -> bool {
            auto pos = expr::save_and_space(seq);
            auto space = expr::bind_space(seq);
            auto recall = [&]() -> bool {
                return self(self, seq, expr, stack);
            };
            TypeExpr* texpr = nullptr;
            auto make_type = [&](TypeKind kind) {
                texpr = new TypeExpr{kind, pos.pos};
                texpr->next = expr;
            };
            if (seq.seek_if("*")) {
                if (!recall()) {
                    return false;
                }
                make_type(TypeKind::ptr);
                texpr->val = "*";
            }
            else if (seq.seek_if("[")) {
                space();
                if (!seq.match("]")) {
                    if (!exp(seq, expr, stack)) {
                        PUSH_ERROR(stack, "type", "expect expr but error occurred", pos.pos, pos.pos)
                        return false;
                    }
                }
                expr::Expr* rec = expr;
                expr = nullptr;
                if (!seq.seek_if("]")) {
                    delete rec;
                    PUSH_ERROR(stack, "type", "expect `]` but not", pos.pos, seq.rptr);
                    return false;
                }
                if (!recall()) {
                    delete rec;
                    return false;
                }
                make_type(TypeKind::array);
                texpr->expr = rec;
                texpr->val = "[]";
            }
            else {
                wrap::string name;
                if (!expr::variable(seq, name, pos.pos)) {
                    PUSH_ERROR(stack, "type", "expect identifier but not", pos.pos, pos.pos)
                    return false;
                }
                make_type(TypeKind::primitive);
                texpr->val = std::move(name);
            }
            expr = texpr;
            pos.ok();
            return true;
        };
        return [fn]<class T>(Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            return fn(fn, seq, expr, stack);
        };
    }

    struct FuncExpr : expr::Expr {
        expr::Expr* fsig = nullptr;
        expr::Expr* block = nullptr;
        expr::Expr* rettype = nullptr;

        using Expr::Expr;

        Expr* index(size_t i) const override {
            if (i == 0) return fsig;
            if (i == 1) return block;
            if (i == 2) return rettype;
            return nullptr;
        }
    };

    auto define_funcsig(auto arg, auto block, auto tysig) {
        auto csig = expr::define_callexpr<wrap::string, wrap::vector>(arg, "fdef", true);
        return [=]<class T>(Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack& stack) {
            auto pos = expr::save_and_space(seq);
            if (!seq.seek_if("func")) {
                return false;
            }
            auto space = expr::bind_space(seq);
            if (!space()) {
                return false;
            }
            pos.ok();
            if (!csig(seq, expr, stack)) {
                return false;
            }
            space();
            auto fexpr = new FuncExpr{"func", pos.pos};
            fexpr->fsig = expr;
            expr = nullptr;
            if (seq.seek_if("->")) {
                if (!tysig(seq, expr, stack)) {
                    delete fexpr;
                    return false;
                }
                fexpr->rettype = expr;
                expr = nullptr;
                space();
            }
            if (seq.current() == '{') {
                if (!block(seq, expr, stack)) {
                    delete fexpr;
                    return false;
                }
                fexpr->block = expr;
            }
            expr = fexpr;
            return true;
        };
    }

    template <class T>
    auto define_minilang(Sequencer<T>& seq, expr::PlaceHolder*& ph, expr::PlaceHolder*& ph2) {
        auto rp = expr::define_replacement(ph);
        auto rp2 = expr::define_replacement(ph2);
        auto comment = expr::define_comment(true, rp, expr::Comment{.begin = "//"}, expr::Comment{.begin = "/*", .end = "*/"});
        auto mul = expr::define_binary(
            comment,
            expr::Ops{"*", expr::Op::mul},
            expr::OpsCheck{"/", expr::Op::div, [](auto& seq) {
                               return seq.current() != '/' & seq.current() != '*';
                           }},
            expr::Ops{"%", expr::Op::mod});
        auto add = expr::define_binary(
            mul,
            expr::Ops{"+", expr::Op::add},
            expr::Ops{"-", expr::Op::sub});
        auto eq = expr::define_binary(
            add,
            expr::Ops{"==", expr::Op::equal},
            expr::Ops{"!=", expr::Op::not_equal});
        auto and_ = expr::define_binary(
            eq,
            expr::Ops{"&&", expr::Op::and_});
        auto or_ = expr::define_binary(
            and_,
            expr::Ops{"||", expr::Op::or_});
        auto exp = expr::define_assignment(
            or_,
            expr::Ops{"=", expr::Op::assign});
        auto call = expr::define_callop<wrap::vector>("fcall", exp);
        auto prim = expr::define_primitive<wrap::string>();
        auto single = expr::define_after(prim, call);
        auto brackets = expr::define_brackets(single, exp, "brackets");
        auto block = expr::define_block<wrap::string, wrap::vector>(rp2, false, "block");
        auto type_ = define_type(exp);
        auto let = expr::define_vardef<wrap::string>("let", "let", exp, type_);
        auto arg = expr::define_vardef<wrap::string>("arg", nullptr, exp, type_);
        auto typedef_ = expr::define_vardef<wrap::string>("typedef", "type", type_, type_, "");
        auto exprstat = expr::define_wrapexpr("expr_stat", exp);
        auto funcsig = define_funcsig(arg, block, type_);
        auto initstat = expr::define_statements(let, exprstat);
        auto return_ = expr::define_return("return", "return", exp);
        auto for_ = expr::define_statement("for", 3, initstat, exp, block);
        auto if_ = expr::define_statement("if", 2, initstat, exp, block);
        auto stat = expr::define_statements(for_, if_, typedef_, let, funcsig, return_, exprstat);
        auto cmts = expr::define_comment(true, stat, expr::Comment{.begin = "//"}, expr::Comment{.begin = "/*", .end = "*/"});
        ph = expr::make_replacement(seq, brackets);
        ph2 = expr::make_replacement(seq, cmts);
        return expr::define_block<wrap::string, wrap::vector>(cmts, false, "program", 0);
    }

    template <class T>
    bool parse(Sequencer<T>& seq, expr::Expr*& expr, expr::ErrorStack stack) {
        expr::PlaceHolder *ph1, *ph2;
        auto parser = define_minilang(seq, ph1, ph2);
        auto res = parser(seq, expr, stack);
        delete ph1;
        delete ph2;
        return res;
    }

    struct Node;

    struct Symbols {
        wrap::vector<Node*> symbol_source;
        wrap::hash_map<wrap::string, wrap::hash_map<wrap::string, wrap::vector<Node*>>> classfied;
    };

    enum class ScopeKind {
        func_local,
        local,
        global,
    };

    struct Scope {
        Symbols* symbols = nullptr;
        Scope* parent = nullptr;
        ScopeKind kind = ScopeKind::local;
    };

    inline Scope* child_scope(Scope* parent, ScopeKind kind) {
        auto ret = new Scope{};
        ret->parent = parent;
        ret->kind = kind;
        return ret;
    }

    struct NodeChildren {
        wrap::vector<Node*> node;
        size_t len() const {
            return node.size();
        }
    };

    struct Error {
        wrap::string msg;
        Node* node;
    };

    namespace assembly {
        struct Type {
        };

        struct Instance {
            Node* base = nullptr;
            size_t instance = 0;
            Type* type = nullptr;
            size_t offset = 0;
        };

        struct AsmData {
            Instance* instance = nullptr;
            size_t llvmindex = 0;
        };

        enum class Location {
            global,
            func,
        };

        struct Context {
            size_t offset_sum = 0;
            Location loc = Location::global;
            size_t tmpindex = 0;

            wrap::string buffer;
            wrap::vector<Error> stack;

            void write(auto&&... v) {
                strutil::appends(buffer, v...);
            }

            void error(auto&& v, Node* node) {
                stack.push_back({.msg = v, .node = node});
            }

            wrap::string make_tmp(auto prefix = "", auto suffix = "") {
                wrap::string ret = prefix;
                number::to_string(ret, tmpindex);
                tmpindex++;
                strutil::append(ret, suffix);
                return ret;
            }
        };

        enum class LLVMBasicType {
            integer,
        };

        struct LLVMType {
            LLVMBasicType type = LLVMBasicType::integer;
            size_t size;

            void as_int(size_t bit) {
                type = LLVMBasicType::integer;
                size = bit;
            }
        };

        struct LLVMValue {
            wrap::string val;
            LLVMType type;
            Node* node = nullptr;
        };
    }  // namespace assembly

    struct Node {
        expr::Expr* expr = nullptr;
        Scope* belongs = nullptr;
        Scope* owns = nullptr;
        Node* parent = nullptr;
        NodeChildren* children = nullptr;
        Scope* symbol = nullptr;
        assembly::AsmData* data = nullptr;
        int resolved_at = 0;
        bool root;

        Node* child(size_t i) {
            if (!children) return nullptr;
            if (i >= children->len()) {
                return nullptr;
            }
            return children->node[i];
        }
    };

    inline size_t length(NodeChildren* ptr) {
        if (!ptr) return 0;
        return ptr->len();
    }

    inline void append_child(NodeChildren*& nch, Node* child) {
        if (!nch) {
            nch = new NodeChildren{};
        }
        nch->node.push_back(child);
    }

    inline void append_symbol(Scope* scope, Node* node, const char* type, wrap::string& name) {
        if (!scope) return;
        if (!scope->symbols) {
            scope->symbols = new Symbols{};
        }
        scope->symbols->symbol_source.push_back(node);
        node->symbol = scope;
        auto& symbols = scope->symbols->classfied[type][name];
        symbols.push_back(node);
    }

    inline Scope* resolve_symbol(Scope* scope, const char* type, wrap::string& name) {
        if (scope->symbols) {
            auto& table = scope->symbols->classfied[type];
            if (table.contains(name)) {
                return scope;
            }
        }
        if (scope->parent) {
            return resolve_symbol(scope->parent, type, name);
        }
        return nullptr;
    }

    Node* convert_to_node(expr::Expr* expr, Scope* scope, bool root = false);

    namespace runtime {

        struct Boolean {
            Node* node;
            bool value;
        };

        struct Integer {
            Node* node;
            std::int64_t value;
        };

        struct String {
            Node* node;
            wrap::string value;
        };

        struct RuntimeEnv;

        using BuiltInProc = void (*)(void* obj, const char* key, RuntimeEnv* args);

        struct BuiltIn {
            Node* node;
            wrap::string name;
            void* object;
            BuiltInProc proc;
        };

        struct Function {
            Node* node;
        };

        enum class OpFilter {
            none = 0,
            add = 0x1,
            sub = 0x2,
            mul = 0x4,
            div = 0x8,
            mod = 0x10,
            and_ = 0x20,
            or_ = 0x40,
        };

        DEFINE_ENUM_FLAGOP(OpFilter)

        struct RuntimeVar;

        struct RuntimeValue {
            RuntimeVar* relvar = nullptr;
            std::variant<std::monostate, Boolean, Integer, String, BuiltIn, Function> value;

            template <class T>
            auto emplace(T&& val) {
                return value.emplace<T>(std::move(val));
            }

            bool* as_bool() {
                if (auto ptr = std::get_if<1>(&value)) {
                    return &ptr->value;
                }
                return nullptr;
            }

            std::int64_t* as_int() {
                if (auto ptr = std::get_if<2>(&value)) {
                    return &ptr->value;
                }
                return nullptr;
            }

            wrap::string* as_str() {
                if (auto ptr = std::get_if<3>(&value)) {
                    return &ptr->value;
                }
                return nullptr;
            }

            BuiltIn* as_builtin() {
                if (auto ptr = std::get_if<4>(&value)) {
                    return ptr;
                }
                return nullptr;
            }

            Node* as_function() {
                if (auto ptr = std::get_if<5>(&value)) {
                    return ptr->node;
                }
                return nullptr;
            }

            bool get_bool() {
                if (auto b = as_bool()) {
                    return *b;
                }
                else if (auto i = as_int()) {
                    return *i != 0;
                }
                else if (auto s = as_str()) {
                    return s->size() != 0;
                }
                return false;
            }

            OpFilter reflect(const RuntimeValue& other) const {
                if (value.index() != other.value.index() || std::holds_alternative<Boolean>(value)) {
                    return OpFilter::and_ | OpFilter::or_;
                }
                else if (std::holds_alternative<Integer>(value)) {
                    return OpFilter::add | OpFilter::sub | OpFilter::mul | OpFilter::div | OpFilter::mod |
                           OpFilter::and_ | OpFilter::or_;
                }
                else if (std::holds_alternative<String>(value)) {
                    return OpFilter::add;
                }
                return OpFilter::none;
            }
        };

        struct RuntimeEnv {
            wrap::vector<RuntimeValue> args;
            bool abort = false;
        };

        struct RuntimeScope;

        struct RuntimeVar {
            Node* node = nullptr;
            wrap::string name;
            RuntimeValue value;
            RuntimeScope* scope = nullptr;
        };

        struct RuntimeScope {
            RuntimeScope* parent = nullptr;
            wrap::hash_map<wrap::string, RuntimeVar> vars;
            Scope* static_scope = nullptr;

            RuntimeVar* define_var(wrap::string& name, Node* node) {
                auto v = vars.try_emplace(name, RuntimeVar{});
                if (!v.second) {
                    return nullptr;
                }
                auto& ref = v.first->second;
                ref.name = name;
                ref.node = node;
                ref.scope = this;
                return &ref;
            }

            RuntimeVar* find_var(wrap::string& name, Scope* scope) {
                if (static_scope == scope) {
                    if (auto found = vars.find(name); found != vars.end()) {
                        return &found->second;
                    }
                }
                if (parent) {
                    return parent->find_var(name, scope);
                }
                return nullptr;
            }
        };

        enum class BackTo {
            none,
            loop,
            func,
        };

        struct Interpreter {
            wrap::vector<Error> stack;

            bool add_builtin(wrap::string key, void* obj, BuiltInProc proc);

            template <class T>
            bool add_builtin(const char* key, T* obj, void (*proc)(T* obj, const char* key, RuntimeEnv* args)) {
                return add_builtin(std::move(key), obj, reinterpret_cast<BuiltInProc>(proc));
            }

           private:
            RuntimeScope root;
            RuntimeScope* current = nullptr;
            Node* rootnode = nullptr;

            BackTo state = BackTo::none;

            RuntimeVar* resolve(Node* node);
            bool walk_node(Node* node, RuntimeValue& value);
            bool eval_for(Node* node, RuntimeValue& value, bool as_if);
            bool eval_if(Node* node, RuntimeValue& value);

            bool eval_expr(RuntimeValue& value, Node* node);

            bool eval_as_bool(Node* node, bool& err);

            bool call_function(Node* fnode, Node* node, RuntimeValue& val);

            void error(auto msg, Node* node) {
                stack.push_back({msg, node});
            }

            bool eval_binary(RuntimeValue& value, Node* node);

            bool invoke_builtin(BuiltIn* builtin, Node* node);

           public:
            bool eval(Node* node) {
                current = &root;
                current->static_scope = node->belongs;
                rootnode = node;
                RuntimeValue value;
                auto res = walk_node(node, value);
                current = nullptr;
                rootnode = nullptr;
                return res;
            }
        };

    }  // namespace runtime

    namespace assembly {

        enum class asmcode {
            nop,
            ld,
            str,
            push,
            pop,
            call,
            ret,
            add,
            sub,
            mul,
            mod,
            div,
        };

        struct AsmValue {
            bool addr = false;
            bool relative = false;
            size_t value;
        };

        struct Asm {
            asmcode code = asmcode::nop;
            AsmValue op[3];
        };

        struct NodeAnalyzer {
           private:
            bool save_types(Node* node);
            bool save_scope(Node* node);
            bool save_variable(Node* node);
            bool save_function(Node* node);

           public:
            bool collect_symbols(Node* node);
            bool resolve_symbols(Node* node);
            bool dump_pseudo_asm(Node* node);
            bool dump_cpp_code(Node* node);
            bool dump_llvm(Node* node, Context& ctx);
        };

    }  // namespace assembly

    futils::parser::stream::Stream make_stream();

    void test_code();
}  // namespace minilang
