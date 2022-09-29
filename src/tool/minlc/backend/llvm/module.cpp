/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "llvm.h"

namespace minlc {
    namespace llvm {

        bool convert_type(L& l, std::string& target, sptr<types::Type>& typ) {
            auto kind = typ->kind;
            if (kind == types::bool_) {
                target = "i1";
            }
            else if (kind == types::int8_ || kind == types::uint8_) {
                target = "i8";
            }
            else if (kind == types::int16_ || kind == types::uint16_) {
                target = "i16";
            }
            else if (kind == types::int32_ || kind == types::uint32_) {
                target = "i32";
            }
            else if (kind == types::int64_ || kind == types::uint64_) {
                target = "i64";
            }
            else if (kind == types::pointer_) {
                auto ptrty = std::static_pointer_cast<types::DefinedType>(typ);
                if (!convert_type(l, target, ptrty->base)) {
                    return false;
                }
                target += "*";
            }
            else {
                l.m.errc.say("now non-builtin type is not supported");
                l.m.errc.node(typ->node);
                return false;
            }
            return true;
        }

        enum class Fndef {
            def,
            decl,
            typ,
        };

        struct FunParam {
            std::string type;
            std::string name;
        };

        struct FunSig {
            std::string ret;
            std::string name;
            std::vector<FunParam> arg;
        };

        bool fetch_funcsig(L& l, sptr<middle::Func>& func, FunSig& sig) {
            auto& ret = func->type->return_;
            if (!ret.size()) {
                sig.ret = "void";
            }
            else if (ret.size() == 1) {
                auto typ = ret[0];
                if (!convert_type(l, sig.ret, typ.type)) {
                    return false;
                }
            }
            else {
                l.m.errc.say("now multiple return is not supported");
                return false;
            }
            sig.name = func->node->ident->str;
            for (auto& param : func->type->params) {
                std::string ty;
                if (!convert_type(l, ty, param.type)) {
                    return false;
                }
                sig.arg.push_back({std::move(ty), param.name});
            }
            return true;
        }

        bool write_funcsig(L& l, const FunSig& sig, Fndef def) {
            if (def == Fndef::typ) {
                l.write(sig.ret, " ");
            }
            else {
                l.write(def == Fndef::def ? "define " : "declare ", sig.ret, " @", sig.name);
            }
            l.write("(");
            for (auto& arg : sig.arg) {
                if (l.back() != '(') {
                    l.write(", ");
                }
                l.write(arg.type);
                if (def == Fndef::def && arg.name.size()) {
                    l.write(" %", arg.name);
                }
            }
            l.write(")");
            return true;
        }

        sptr<middle::Func> get_callee(L& l, std::string& result, sptr<middle::Expr>& expr) {
            if (expr->obj_type == middle::mk_ident) {
                auto ident = std::static_pointer_cast<middle::IdentExpr>(expr);
                auto& recent = ident->lookedup[0];
                auto found = recent.list->func_defs.find(ident->node->str);
                if (found == recent.list->func_defs.end()) {
                    l.m.errc.say("function definition of `", ident->node->str, "` not found");
                    l.m.errc.node(expr->node);
                    return nullptr;
                }
                auto fn = l.m.object_mapping.find(found->second[0].node);
                if (fn == l.m.object_mapping.end()) {
                    l.m.errc.say("mapped object of `", ident->node->str, "` not found. library bug!!");
                    l.m.errc.node(expr->node);
                    return nullptr;
                }
                if (fn->second->obj_type != middle::mk_func) {
                    l.m.errc.say("mapped object is not function type. library bug!!");
                    l.m.errc.node(expr->node);
                    return nullptr;
                }
                result = "@" + ident->node->str;
                return std::static_pointer_cast<middle::Func>(fn->second);
            }
            l.m.errc.say("now not identifier callee not supported.");
            l.m.errc.node(expr->node);
            return nullptr;
        }

        bool write_expr(L& l, std::string& result, sptr<middle::Expr>& expr) {
            if (expr->obj_type == middle::mk_expr) {
                if (expr->node->node_type != mi::nt_number) {
                    l.m.errc.say("now allowed literal is only integer");
                    l.m.errc.node(expr->node);
                    return false;
                }
                auto n = mi::is_Number(expr->node);
                result = utils::number::to_string<std::string>(n->integer);  // immediate
                return true;
            }
            if (expr->obj_type == middle::mk_binary) {
                auto bin = std::static_pointer_cast<middle::BinaryExpr>(expr);
                std::string left;
                if (!write_expr(l, left, bin->left)) {
                    return false;
                }
                // TODO(on-keyday): if expr is "&&" or "||", add shortcut
                std::string right;
                if (!write_expr(l, right, bin->right)) {
                    return false;
                }
                auto seq = "%teq" + l.yield_seq();
                l.write(seq, " = ");
                auto& op = bin->node->str;
                if (op == "*") {
                    l.write("mul i32 ", left, ", ", right);
                }
                else if (op == "+") {
                    l.write("add i32 ", left, ", ", right);
                }
                else {
                    goto ERR;
                }
                l.ln();
                result = std::move(seq);
                return true;
            }
            if (expr->obj_type == middle::mk_call) {
                auto call = std::static_pointer_cast<middle::CallExpr>(expr);
                std::vector<std::string> args;
                for (auto& arg : call->arguments) {
                    write_expr(l, result, arg);
                    args.push_back(std::move(result));
                }
                auto seq = "%teq" + l.yield_seq();
                std::string callee_str;
                auto callee = get_callee(l, callee_str, call->callee);
                if (!callee) {
                    return false;
                }
                l.write(seq, " = call ");
                std::string ret_typ;
                FunSig sig;
                if (!fetch_funcsig(l, callee, sig)) {
                    return false;
                }
                if (sig.arg.size() != args.size()) {
                    l.m.errc.say("unexpected function argument count");
                    l.m.errc.node(call->node);
                    l.m.errc.say("function defined at here");
                    l.m.errc.node(callee->node);
                    return false;
                }
                if (!write_funcsig(l, sig, Fndef::typ)) {
                    return false;
                }
                l.write(" ", callee_str, "(");
                for (auto i = 0; i < args.size(); i++) {
                    if (l.output.back() != '(') {
                        l.write(", ");
                    }
                    l.write(sig.arg[i].type, " ", args[i]);
                }
                l.write(")");
                l.ln();
                result = std::move(seq);
                return true;
            }
            if (expr->obj_type == middle::mk_ident) {
                // TODO(on-keyday): check identifier
                result = "%" + expr->node->str;
                return true;
            }
        ERR:
            l.m.errc.say("now this type of node not supported");
            l.m.errc.node(expr->node);
            return false;
        }

        bool write_funcblock(L& l, sptr<middle::Block>& block, const FunSig& sig) {
            l.write("{");
            l.ln();
            l.write("entry:");
            l.ln();
            bool lastret = false;
            for (auto& ist : block->objects) {
                lastret = false;
                if (ist->obj_type == middle::mk_expr || ist->obj_type == middle::mk_binary) {
                    std::string tmp;
                    auto expr = std::static_pointer_cast<middle::Expr>(ist);
                    if (!write_expr(l, tmp, expr)) {
                        return false;
                    }
                }
                if (ist->obj_type == middle::mk_return) {
                    auto ret = std::static_pointer_cast<middle::Return>(ist);
                    std::string tmp;
                    if (ret->expr) {
                        if (!write_expr(l, tmp, ret->expr)) {
                            return false;
                        }
                    }
                    l.write("ret ", sig.ret, " ", tmp);
                    l.ln();
                    lastret = true;
                }
            }
            if (!lastret) {
                if (sig.ret == "void") {
                    l.writeln("ret void");
                }
                else if (sig.ret == "i32" && sig.name == "main") {
                    l.writeln("ret i32 0");
                }
                else {
                    l.writeln("unreachable");
                }
            }
            l.writeln("}");
            l.ln();
            return true;
        }

        bool write_funcdef(L& l, sptr<middle::Func>& func) {
            FunSig sig;
            if (!fetch_funcsig(l, func, sig)) {
                return false;
            }
            if (!write_funcsig(l, sig, Fndef::def)) {
                return false;
            }
            return write_funcblock(l, func->block, sig);
        }

        bool write_program(L& l, const std::string& input, sptr<middle::Program>& prog) {
            l.writeln(R"(source_filename= ")", input, R"(")");
            l.ln();
            for (auto& g : prog->global_funcs) {
                if (!write_funcdef(l, g)) {
                    return false;
                }
            }
            return true;
        }

    }  // namespace llvm
}  // namespace minlc
