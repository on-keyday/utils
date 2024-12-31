/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include <map>
#include <string>
#include <variant>
#include <memory>
#include <vector>
#include <helper/defer.h>
#include "error.h"
#include <format>
#include <escape/escape.h>

namespace qurl {
    namespace runtime {
        enum class Op {
            UNDEF,
            // stack control
            PUSH,
            POP,
            RESOLVE,

            // function call/return
            CALL,
            RET,
            RET_NONE,

            // operation
            ADD,
            SUB,
            MUL,
            DIV,
            MOD,
            AND,
            OR,
            EQ,
            NEQ,

            // variable scope control
            ENTER,
            LEAVE,
            ALLOC,
            ASSIGN,

            // flow control
            JMPIF,
            LABEL,
        };

        enum class Status {
            ok,
            end,
            illegal,
            stack_underflow,
            undef,
            ref_err,
            div_by_zero,
            scope_underflow,
            suspend,
        };

        constexpr const char* to_string(Op op) {
            switch (op) {
                default:
                    return "UNDEF";
                case Op::ASSIGN:
                    return "ASSIGN";
                case Op::PUSH:
                    return "PUSH";
                case Op::CALL:
                    return "CALL";
                case Op::ALLOC:
                    return "ALLOC";
                case Op::POP:
                    return "POP";
                case Op::RET:
                    return "RET";
                case Op::RET_NONE:
                    return "RET_NONE";
                case Op::ADD:
                    return "ADD";
                case Op::SUB:
                    return "SUB";
                case Op::MUL:
                    return "MUL";
                case Op::DIV:
                    return "DIV";
                case Op::MOD:
                    return "MOD";
                case Op::EQ:
                    return "EQ";
                case Op::NEQ:
                    return "NEQ";
                case Op::ENTER:
                    return "ENTER";
                case Op::LEAVE:
                    return "LEAVE";
                case Op::RESOLVE:
                    return "RESOLVE";
                case Op::JMPIF:
                    return "JMPIF";
                case Op::LABEL:
                    return "LABEL";
            }
        }

        struct Name {
            std::string name;
        };

        struct Ref {
            std::string ref;
        };

        struct Label {
            std::uint64_t label = 0;
            bool fwd = false;
        };

        struct Instruction;

        struct Param {
            std::string param;
            futils::comb2::Pos pos;
        };

        struct Func {
            futils::comb2::Pos pos;
            std::vector<Param> params;
            std::vector<Instruction> istrs;
        };
        struct Value;

        enum class BuiltinAccess {
            set,
            get,
            call,
        };

        struct BuiltinObject {
            std::shared_ptr<void> obj;
            Status (*fn)(std::shared_ptr<void>& obj, BuiltinAccess ba, Value& ret, std::vector<Value>& arg);

            Status call(Value& ret, std::vector<Value>& arg) {
                if (fn) {
                    return fn(obj, BuiltinAccess::call, ret, arg);
                }
                return Status::ok;
            }
        };

        struct Value {
            std::variant<std::monostate, std::string, std::int64_t, std::uint64_t, Name, std::shared_ptr<Func>, BuiltinObject*, Label> value;
            futils::comb2::Pos pos = futils::comb2::npos;

            template <class T>
            T* get() {
                return std::get_if<T>(&value);
            }

            bool is_name() const {
                return std::holds_alternative<Name>(value);
            }

            std::string to_string(bool use_null = false) const {
                if (auto s = std::get_if<std::string>(&value)) {
                    std::string val;
                    futils::escape::escape_str(*s, val);
                    return "\"" + val + "\"";
                }
                if (auto i = std::get_if<std::uint64_t>(&value)) {
                    return "#" + futils::number::to_string<std::string>(*i);
                }
                if (auto n = std::get_if<Name>(&value)) {
                    return "[" + n->name + "]";
                }
                if (auto n = std::get_if<std::shared_ptr<Func>>(&value)) {
                    return "<function arg: " + futils::number::to_string<std::string>((*n)->params.size()) + ">";
                }
                if (auto n = std::get_if<std::monostate>(&value); n && use_null) {
                    return "null";
                }
                if (auto n = std::get_if<Label>(&value)) {
                    return ":" + futils::number::to_string<std::string>(n->label);
                }
                return {};
            }
        };

        struct Instruction {
            Op op = Op::UNDEF;
            Value value;
            futils::comb2::Pos pos = futils::comb2::npos;
        };

        struct InstrScope {
            std::shared_ptr<InstrScope> ret;
            std::uint64_t pc = 0;
            std::vector<Instruction>* istrs;
        };

        struct Var {
            futils::comb2::Pos pos;
            Value value;
        };

        struct Scope {
            std::weak_ptr<Scope> parent;
            std::shared_ptr<Scope> child;
            std::map<std::string, Var> vars;
            std::vector<Value> stack;
        };

        struct SuspendEnv {
            BuiltinObject* builtin;
            std::vector<Value> args;
            Value res;
            bool suspended = false;
        };

        struct Env {
            std::shared_ptr<Scope> root;
            std::shared_ptr<Scope> scope;
            std::uint64_t pc = 0;
            std::shared_ptr<InstrScope> istrs;
            futils::comb2::Pos src_pos;
            std::vector<Error> errors;
            std::map<std::string, BuiltinObject> builtins;
            SuspendEnv suspend;

           private:
            Var* find(const std::string& name) {
                maybe_init_scope();
                auto cur = scope;
                while (cur) {
                    auto found = cur->vars.find(name);
                    if (found != cur->vars.end()) {
                        return &found->second;
                    }
                    cur = cur->parent.lock();
                }
                return nullptr;
            }

            void maybe_init_scope() {
                if (!root) {
                    root = std::make_shared<Scope>();
                    scope = root;
                }
            }
            void enter_scope() {
                maybe_init_scope();
                auto new_scope = std::make_shared<Scope>();
                scope->child = new_scope;
                new_scope->parent = scope;
                scope = std::move(new_scope);
            }

            bool leave_scope() {
                maybe_init_scope();
                auto l = scope->parent.lock();
                if (!l) {
                    return false;  // cannot leave
                }
                l->child = nullptr;
                scope = std::move(l);
                return true;
            }

            Status call(std::shared_ptr<Func>& f, std::vector<Value>& args) {
                if (f->params.size() != args.size()) {
                    errors.push_back(Error{
                        .msg = std::format("expect {} argument, got {}",
                                           f->params.size(), args.size()),
                        .pos = src_pos,
                    });
                    errors.push_back(Error{
                        .msg = std::format("function is defined here"),
                        .pos = f->pos,
                    });
                    return Status::ref_err;
                }
                enter_scope();
                for (size_t i = 0; i < args.size(); i++) {
                    auto& param = f->params[i];
                    auto& var = scope->vars[param.param];
                    var.value = std::move(args[i]);
                    var.pos = param.pos;
                }
                auto new_istrs = std::make_shared<InstrScope>();
                new_istrs->pc = 0;
                new_istrs->ret = std::move(istrs);
                new_istrs->istrs = &f->istrs;
                istrs = std::move(new_istrs);
                return Status::ok;
            }

            Status ret(Value& val) {
                if (!istrs->ret) {
                    return Status::end;
                }
                auto back_to = std::move(istrs->ret);
                if (!leave_scope()) {
                    errors.push_back(Error{
                        .msg = std::format("cannot leave scope here"),
                        .pos = src_pos,
                    });
                    return Status::illegal;
                }
                scope->stack.push_back(std::move(val));
                istrs = std::move(back_to);
                return Status::ok;
            }

            bool resolve_name(Value& resolved) {
                while (resolved.is_name()) {
                    auto& name = *resolved.get<Name>();
                    auto val = find(name.name);
                    if (!val) {
                        auto found = builtins.find(name.name);
                        if (found != builtins.end()) {
                            resolved.value = &found->second;
                            return true;
                        }
                        errors.push_back(Error{
                            .msg = std::format("identifier `{}` is not defined", name.name),
                            .pos = resolved.pos,
                        });
                        return false;
                    }
                    resolved = val->value;
                }
                return true;
            }

            Status binary_op(Op op) {
                if (scope->stack.size() < 2) {
                    errors.push_back(Error{
                        .msg = std::format("stack underflow"),
                        .pos = src_pos,
                    });
                    return Status::stack_underflow;
                }
                auto right = std::move(scope->stack.back());
                scope->stack.pop_back();
                auto left = std::move(scope->stack.back());
                scope->stack.pop_back();
                if (!resolve_name(left) || !resolve_name(right)) {
                    return Status::ref_err;
                }
                auto invalid_op = [&] {
                    errors.push_back(Error{
                        .msg = std::format("operation `{}` is undefined for value {} and {}", to_string(op), left.to_string(true), right.to_string(true)),
                        .pos = src_pos,
                    });
                    return Status::undef;
                };
                auto push = [&](auto&& val) {
                    scope->stack.push_back(
                        Value{
                            .value = val,
                            .pos = src_pos,
                        });
                    return Status::ok;
                };
                auto div_by_0 = [&] {
                    errors.push_back(Error{
                        .msg = "div by 0",
                        .pos = src_pos,
                    });
                    errors.push_back(Error{
                        .msg = "0 is generated here",
                        .pos = right.pos,
                    });
                    return Status::div_by_zero;
                };
                auto int_ops = [&](auto&& op) {
                    auto ul = left.get<std::uint64_t>();
                    auto ur = right.get<std::uint64_t>();
                    auto sl = left.get<std::int64_t>();
                    auto sr = right.get<std::int64_t>();
                    if (ul && ur) {
                        return op(*ul, *ur);
                    }
                    else if (ul && sr) {
                        return op(*ul, *sr);
                    }
                    else if (sl && ur) {
                        return op(*sl, *ur);
                    }
                    else if (sl && sr) {
                        return op(*sl, *sr);
                    }
                    else {
                        return invalid_op();
                    }
                };
                switch (op) {
                    default: {
                        return invalid_op();
                    }
                    case Op::ADD: {
                        return int_ops([&](auto&& a, auto&& b) {
                            if (a < 0 || b < 0) {
                                return push(std::int64_t(a) + std::int64_t(b));
                            }
                            return push(std::uint64_t(a) + std::uint64_t(b));
                        });
                    }
                    case Op::SUB: {
                        return int_ops([&](auto&& a, auto&& b) {
                            if (a >= 0 && a < b) {
                                return push(std::int64_t(a - b));
                            }
                            if (a < 0 && a > b) {
                                return push(std::uint64_t(a - b));
                            }
                            return push(a - b);
                        });
                    }
                    case Op::MUL: {
                        return int_ops([&](auto&& a, auto&& b) {
                            switch ((a >= 0) + (b >= 0)) {
                                default:
                                    return push(std::uint64_t(a * b));
                                case 1:
                                    return push(std::int64_t(a * b));
                            }
                        });
                    }
                    case Op::DIV: {
                        return int_ops([&](auto&& a, auto&& b) {
                            if (b == 0) {
                                return div_by_0();
                            }
                            switch ((a >= 0) + (b >= 0)) {
                                default:
                                    return push(std::uint64_t(a / b));
                                case 1:
                                    return push(std::int64_t(a / b));
                            }
                        });
                    }
                    case Op::MOD: {
                        return int_ops([&](auto&& a, auto&& b) {
                            if (b == 0) {
                                return div_by_0();
                            }
                            switch ((a >= 0) + (b >= 0)) {
                                default:
                                    return push(std::uint64_t(a % b));
                                case 1:
                                    return push(std::int64_t(a % b));
                            }
                        });
                    }
                    case Op::EQ: {
                        return int_ops([&](auto&& a, auto&& b) {
                            return push(std::int64_t(a == b));
                        });
                    }
                    case Op::NEQ: {
                        return int_ops([&](auto&& a, auto&& b) {
                            return push(std::int64_t(a != b));
                        });
                    }
                }
            }

           public:
            Status run() {
                if (suspend.suspended) {
                    auto res = suspend.builtin->call(suspend.res, suspend.args);
                    if (res == Status::suspend) {
                        return Status::suspend;
                    }
                    suspend.suspended = false;
                    scope->stack.push_back(std::move(suspend.res));
                    return res;
                }
                if (istrs->pc >= istrs->istrs->size()) {
                    return Status::end;
                }
                maybe_init_scope();
                auto& istr = (*istrs->istrs)[istrs->pc];
                auto& cpc = istrs->pc;
                auto inc = futils::helper::defer([&] {
                    cpc++;
                });
                src_pos = istr.pos;
                switch (istr.op) {
                    default: {
                        inc.cancel();
                        errors.push_back(Error{
                            .msg = std::format("undefined operation: Op{{{}}}", int(istr.op)),
                            .pos = src_pos,
                        });
                        return Status::undef;
                    }
                    case Op::ASSIGN:
                    case Op::ALLOC: {
                        auto var_name = istr.value.get<Name>();
                        if (!var_name) {
                            inc.cancel();
                            errors.push_back(Error{
                                .msg = std::format("expect var name but not. value:{}", istr.value.to_string(true)),
                                .pos = src_pos,
                            });
                            return Status::illegal;
                        }
                        if (scope->stack.size() == 0) {
                            inc.cancel();
                            errors.push_back(Error{
                                .msg = std::format("stack underflow when assigning to `{}`", var_name->name),
                                .pos = src_pos,
                            });
                            return Status::stack_underflow;
                        }
                        Var* found = nullptr;
                        if (istr.op == Op::ASSIGN) {
                            found = find(var_name->name);
                            if (!found) {
                                errors.push_back(Error{
                                    .msg = std::format("identifier `{}` is not defined", var_name->name),
                                    .pos = istr.value.pos,
                                });
                                return Status::ref_err;
                            }
                        }
                        else {
                            auto it = scope->vars.find(var_name->name);
                            if (it != scope->vars.end()) {
                                errors.push_back(Error{
                                    .msg = std::format("identifier `{}` is already defined", var_name->name),
                                    .pos = istr.value.pos,
                                });
                                errors.push_back(Error{
                                    .msg = std::format("identifier `{}` is defined here", var_name->name),
                                    .pos = it->second.pos,
                                });
                                return Status::ref_err;
                            }
                            found = &scope->vars[var_name->name];
                            found->pos = src_pos;
                        }
                        auto val = std::move(scope->stack.back());
                        if (!resolve_name(val)) {
                            return Status::ref_err;
                        }
                        found->value = std::move(val);
                        scope->stack.pop_back();
                        return Status::ok;
                    }
                    case Op::PUSH: {
                        scope->stack.push_back(istr.value);
                        return Status::ok;
                    }
                    case Op::POP: {
                        if (scope->stack.size() == 0) {
                            inc.cancel();
                            errors.push_back(Error{
                                .msg = std::format("stack underflow"),
                                .pos = src_pos,
                            });
                            return Status::stack_underflow;
                        }
                        scope->stack.pop_back();
                        return Status::ok;
                    }
                    case Op::CALL: {
                        auto narg = istr.value.get<std::uint64_t>();
                        if (!narg) {
                            inc.cancel();
                            errors.push_back(Error{
                                .msg = std::format("expect arg count but not. value:{}", istr.value.to_string(true)),
                                .pos = src_pos,
                            });
                            return Status::illegal;
                        }
                        std::vector<Value> args;
                        for (size_t i = 0; i < *narg; i++) {
                            if (scope->stack.size() == 0) {
                                inc.cancel();
                                errors.push_back(Error{
                                    .msg = std::format("stack underflow when calling function"),
                                    .pos = src_pos,
                                });
                                return Status::stack_underflow;
                            }
                            auto arg = std::move(scope->stack.back());
                            if (!resolve_name(arg)) {
                                inc.cancel();
                                return Status::ref_err;
                            }
                            args.push_back(std::move(arg));
                            scope->stack.pop_back();
                        }
                        if (scope->stack.size() == 0) {
                            inc.cancel();
                            errors.push_back(Error{
                                .msg = std::format("stack underflow when calling function"),
                                .pos = src_pos,
                            });
                            return Status::stack_underflow;
                        }
                        auto callee = std::move(scope->stack.back());
                        if (!resolve_name(callee)) {
                            return Status::ref_err;
                        }
                        scope->stack.pop_back();
                        if (auto builtin = callee.get<BuiltinObject*>()) {
                            Value ret;
                            auto res = (**builtin).call(ret, args);
                            if (res == Status::suspend) {
                                suspend.builtin = *builtin;
                                suspend.args = std::move(args);
                                suspend.res = std::move(ret);
                                suspend.suspended = true;
                                return Status::suspend;
                            }
                            scope->stack.push_back(std::move(ret));
                            return res;
                        }
                        auto fn = callee.get<std::shared_ptr<Func>>();
                        if (!fn) {
                            errors.push_back(Error{
                                .msg = std::format("expect func but not. value:{}", callee.to_string(true)),
                                .pos = src_pos,
                            });
                            errors.push_back(Error{
                                .msg = std::format("callee is defined here"),
                                .pos = callee.pos,
                            });
                            return Status::ref_err;
                        }
                        auto& c = *fn;
                        return call(c, args);
                    }
                    case Op::RESOLVE: {
                        if (scope->stack.size() == 0) {
                            errors.push_back(Error{
                                .msg = "stack underflow",
                                .pos = src_pos,
                            });
                            return Status::stack_underflow;
                        }
                        if (!resolve_name(scope->stack.back())) {
                            return Status::ref_err;
                        }
                        return Status::ok;
                    }
                    case Op::RET:
                    case Op::RET_NONE: {
                        inc.cancel();
                        Value val;
                        if (istr.op == Op::RET) {
                            if (scope->stack.size() == 0) {
                                inc.cancel();
                                errors.push_back(Error{
                                    .msg = std::format("stack underflow when returning"),
                                    .pos = src_pos,
                                });
                                return Status::stack_underflow;
                            }
                            val = std::move(scope->stack.back());
                            if (val.is_name()) {
                                errors.push_back(Error{
                                    .msg = std::format("cannnot resolve name on RET instruction. call RESOLVE before"),
                                    .pos = val.pos,
                                });
                                return Status::ref_err;
                            }
                        }
                        return ret(val);
                    }
                    case Op::ADD:
                    case Op::SUB:
                    case Op::MUL:
                    case Op::DIV:
                    case Op::MOD:
                    case Op::EQ:
                    case Op::NEQ: {
                        return binary_op(istr.op);
                    }
                    case Op::ENTER: {
                        enter_scope();
                        return Status::ok;
                    }
                    case Op::LEAVE: {
                        if (!leave_scope()) {
                            errors.push_back(Error{
                                .msg = "scope underflow",
                                .pos = src_pos,
                            });
                            return Status::scope_underflow;
                        }
                        return Status::ok;
                    }
                }
            }
        };
    }  // namespace runtime
}  // namespace qurl
