/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "c_ast.h"
#include <code/code_writer.h>
#include <variant>
#include <platform/detect.h>

// see also https://www.sigbus.info/compilerbook
namespace utils::langc::gen {
    using Writer = code::CodeWriter<std::string>;
    namespace x64 {

        enum class OptIstr {
            other,
            push_imm,
            push_rax,
            push_rdi,
            push_rdx,
            pop_rdi,
            pop_rax,
            pop_rdx,
        };

        enum class Platform {
            win_x64,
            linux_x64,
        };

        enum class Register {
            // general purpose
            rax,
            rbx,
            rcx,
            rdx,
            rsi,
            rdi,
            rbp,
            rsp,
            r8,
            r9,
            r10,
            r11,
            r12,
            r13,
            r14,
            r15,

            rip,
        };

        constexpr const char* reg_list[]{
            "rax",
            "rbx",
            "rcx",
            "rdx",
            "rsi",
            "rdi",
            "rbp",
            "rsp",
            "r8",
            "r9",
            "r10",
            "r11",
            "r12",
            "r13",
            "r14",
            "r15",

            "rip",
        };

        constexpr Register regs_win[] = {Register::rcx, Register::rdx, Register::r8, Register::r9};
        constexpr Register regs_linux[] = {Register::rdi, Register::rsi, Register::rdx, Register::rcx, Register::r8, Register::r9};

        enum class Instruction {
            push,
            pop,
            mov,
            movzx,
            movsx,
            lea,

            cmp,
            xor_,
            add,
            sub,
        };

        constexpr const char* istr_list[] = {
            "push",
            "pop",
            "mov",
            "movzx",
            "movsx",
            "lea",

            "cmp",
            "xor",
            "add",
            "sub",
        };

        enum class MemoryPtr {
            byte,
            word,
            dword,
            qword,
            xmmword,
        };

        constexpr const char* ptr_list[] = {"byte ptr", "word ptr", "dword ptr", "qword ptr", "xmmword ptr"};

        // rxx -> xl, xx, exx, rxx
        std::optional<std::string> regs_map(Register r, size_t size) {
            switch (r) {
                case Register::rax:
                case Register::rbx:
                case Register::rcx:
                case Register::rdx: {
                    auto reg = reg_list[int(r)];
                    char buf[4]{0};
                    switch (size) {
                        case 1: {
                            buf[0] = reg[1];
                            buf[1] = 'l';
                            return buf;
                        }
                        case 2: {
                            buf[0] = reg[1];
                            buf[1] = 'x';
                            return buf;
                        }
                        case 4: {
                            buf[0] = 'e';
                            buf[1] = reg[1];
                            buf[2] = 'x';
                            return buf;
                        }
                        case 8: {
                            buf[0] = 'r';
                            buf[1] = reg[1];
                            buf[2] = 'x';
                            return buf;
                        }
                        default: {
                            return std::nullopt;
                        }
                    }
                }
                case Register::r8:
                case Register::r9:
                case Register::r10:
                case Register::r11:
                case Register::r12:
                case Register::r13:
                case Register::r14:
                case Register::r15: {
                    auto reg = reg_list[int(r)];
                    switch (size) {
                        case 1: {
                            return std::string(reg) + "b";
                        }
                        case 2: {
                            return std::string(reg) + "w";
                        }
                        case 4: {
                            return std::string(reg) + "d";
                        }
                        case 8: {
                            return std::string(reg);
                        }
                        default: {
                            return std::nullopt;
                        }
                    }
                }
                case Register::rsi:
                case Register::rdi:
                case Register::rip:
                case Register::rsp:
                case Register::rbp: {
                    auto reg = reg_list[int(r)];
                    char buf[4]{0};
                    switch (size) {
                        case 1: {
                            if (r == Register::rip) {
                                return std::nullopt;
                            }
                            buf[0] = reg[1];
                            buf[1] = reg[2];
                            buf[2] = 'l';
                            return buf;
                        }
                        case 2: {
                            buf[0] = reg[1];
                            buf[1] = reg[2];
                            return buf;
                        }
                        case 4: {
                            buf[0] = 'e';
                            buf[1] = reg[1];
                            buf[2] = reg[2];
                            return buf;
                        }
                        case 8: {
                            buf[0] = 'r';
                            buf[1] = reg[1];
                            buf[2] = reg[2];
                            return buf;
                        }
                        default: {
                            return std::nullopt;
                        }
                    }
                }
                default: {
                    return std::nullopt;
                }
            }
        }

        struct Ptr {
            MemoryPtr ptr;
            std::string addr;
            size_t size = 0;

            std::string to_addr() {
                return ptr_list[int(ptr)] + (" [" + addr + "]");
            }
        };

        std::optional<MemoryPtr> mem_ptr(size_t size) {
            switch (size) {
                case 1:
                    return MemoryPtr::byte;
                case 2:
                    return MemoryPtr::word;
                case 4:
                    return MemoryPtr::dword;
                case 8:
                    return MemoryPtr::qword;
                case 16:
                    return MemoryPtr::xmmword;
                default:
                    return std::nullopt;
            }
        }

        std::optional<Ptr> ptr_access(const std::string& addr, const std::shared_ptr<ast::Type>& type) {
            auto ptr = mem_ptr(type->size);
            if (!ptr) {
                return std::nullopt;
            }
            return Ptr{*ptr, addr, type->size};
        }

        struct SizedRegister {
            Register reg;
            size_t size = 0;
        };

        struct Immediate {
            std::string imm;
        };

        struct Operand {
            std::vector<std::variant<SizedRegister, Ptr, Immediate>> operand;
            std::optional<std::string> to_string() {
                std::string str;
                bool comma = false;
                for (auto& op : operand) {
                    if (comma) {
                        str += ", ";
                    }
                    else {
                        str += " ";
                    }
                    switch (op.index()) {
                        case 0: {
                            auto& r = std::get<0>(op);
                            auto maped = regs_map(r.reg, r.size);
                            if (!maped) {
                                return std::nullopt;
                            }
                            str += *maped;
                        }
                        case 1: {
                            auto& p = std::get<1>(op);
                            str += p.to_addr();
                        }
                        case 2: {
                            auto& imm = std::get<2>(op);
                            str += imm.imm;
                        }
                    }
                    comma = true;
                }
                return str;
            }
        };

        struct Op {
            Instruction ist;
            Operand operand;

            std::optional<std::string> to_string() {
                std::string buf;
                auto op = operand.to_string();
                if (!op) {
                    return std::nullopt;
                }
                strutil::appends(buf, istr_list[int(ist)], *op);
                return buf;
            }
        };

        struct Context {
            Writer& w;
            Errors& errs;
            size_t label = 0;

           private:
            std::string section;

           public:
            void set_section(const std::string& s) {
                if (section != s) {
                    writeln(".section ", s);
                    section = s;
                }
            }

            size_t get_arg_reg_len() const {
                switch (platform) {
                    case Platform::win_x64:
                        return 4;
                    case Platform::linux_x64:
                        return 6;
                }
            }

            size_t get_stack_arg(size_t args) const {
                if (args <= get_arg_reg_len()) {
                    return 0;
                }
                return args - get_arg_reg_len();
            }

            const Register* const get_arg_regs() const {
                switch (platform) {
                    case Platform::win_x64:
                        return regs_win;
                    case Platform::linux_x64:
                        return regs_linux;
                }
            }

            Platform platform =
#ifdef UTILS_PLATFORM_WINDOWS
                Platform::win_x64;
#elif defined(UTILS_PLATFORM_LINUX)
                Platform::linux_x64;
#else
                Platform::linux_x64;
#endif
            Context(Writer& w, Errors& e)
                : w(w), errs(e) {}

            std::string get_local_label(const char* prefix) {
                const auto d = helper::defer([&] {
                    label++;
                });
                return ".L." + (prefix + ("." + utils::number::to_string<std::string>(label)));
            }

           private:
            static constexpr auto n_history = 4;
            OptIstr history[n_history]{OptIstr::other};
            std::optional<std::string> f_imm;
            size_t stack_offset = 0;

            void write_opt_ister(OptIstr h) {
                if (h == OptIstr::pop_rax) {
                    w.writeln("pop rax");
                    stack_offset -= 8;
                }
                else if (h == OptIstr::pop_rdi) {
                    w.writeln("pop rdi");
                    stack_offset -= 8;
                }
                else if (h == OptIstr::pop_rdx) {
                    w.writeln("pop rdx");
                    stack_offset -= 8;
                }
                else if (h == OptIstr::push_imm) {
                    w.writeln("push ", *f_imm);
                    f_imm.reset();
                    stack_offset += 8;
                }
                else if (h == OptIstr::push_rax) {
                    w.writeln("push rax");
                    stack_offset += 8;
                }
                else if (h == OptIstr::push_rdi) {
                    w.writeln("push rdi");
                    stack_offset += 8;
                }
                else if (h == OptIstr::push_rdx) {
                    w.writeln("push rdx");
                    stack_offset += 8;
                }
            }

            void flush_history(bool write) {
                for (auto& h : history) {
                    if (write) {
                        write_opt_ister(h);
                    }
                    h = OptIstr::other;
                }
            }

            void maybe_write_optimized() {
                if (history[n_history - 4] == OptIstr::push_rax &&
                    history[n_history - 3] == OptIstr::push_imm &&
                    history[n_history - 2] == OptIstr::pop_rdi &&
                    history[n_history - 1] == OptIstr::pop_rax) {
                    w.writeln("mov rdi, ", *f_imm);
                    f_imm.reset();
                    flush_history(false);
                }
                else if (history[n_history - 4] == OptIstr::push_rax &&
                         history[n_history - 3] == OptIstr::push_imm &&
                         history[n_history - 2] == OptIstr::pop_rdx &&
                         history[n_history - 1] == OptIstr::pop_rax) {
                    w.writeln("mov rdx, ", *f_imm);
                    f_imm.reset();
                    flush_history(false);
                }
                else if (history[n_history - 2] == OptIstr::push_imm &&
                         history[n_history - 1] == OptIstr::pop_rax) {
                    history[n_history - 2] = OptIstr::other;
                    history[n_history - 1] = OptIstr::other;
                    auto imm = std::move(f_imm);
                    flush_history(true);
                    w.writeln("mov rax, ", *imm);
                }
                else if ((history[n_history - 2] == OptIstr::push_rax ||
                          history[n_history - 2] == OptIstr::push_rdi ||
                          history[n_history - 2] == OptIstr::push_rdx) &&
                         history[n_history - 1] == OptIstr::pop_rax) {
                    history[n_history - 2] = OptIstr::other;
                    history[n_history - 1] = OptIstr::other;
                    flush_history(true);
                }
                else if (history[n_history - 2] == OptIstr::push_rax &&
                         history[n_history - 1] == OptIstr::pop_rdi) {
                    history[n_history - 2] = OptIstr::other;
                    history[n_history - 1] = OptIstr::other;
                    flush_history(true);
                    w.writeln("mov rdi, rax");
                }
                else if (history[n_history - 2] == OptIstr::push_rax &&
                         history[n_history - 1] == OptIstr::pop_rdx) {
                    history[n_history - 2] = OptIstr::other;
                    history[n_history - 1] = OptIstr::other;
                    flush_history(true);
                    w.writeln("mov rdx, rax");
                }
            }

            void add_history(OptIstr s) {
                if (s == OptIstr::other) {
                    flush_history(true);
                }
                else if (f_imm && s == OptIstr::push_imm) {
                    flush_history(true);
                }
                if (history[0] != OptIstr::other) {
                    write_opt_ister(history[0]);
                }
                for (auto i = 0; i < n_history - 1; i++) {
                    history[i] = history[i + 1];
                }
                history[n_history - 1] = s;
                maybe_write_optimized();
            }

           public:
            void push(Register reg) {
                writeln("push ", reg_list[int(reg)]);
                stack_offset += 8;
            }

            void pop(Register reg) {
                writeln("pop ", reg_list[int(reg)]);
                stack_offset -= 8;
            }

           private:
            void as(size_t a, size_t b) {
                assert(a == b);
            }

           public:
            auto align_stack(size_t to_add) {
                flush_history(true);  // for stack algiment
                auto prev = stack_offset;
                auto align_target = to_add + prev;
                if (align_target % 16 == 0) {
                    write_comment("for alignment");
                    writeln("sub rsp, 8");
                    stack_offset += 8;
                }
                return helper::defer([&, prev, align_target] {
                    if (align_target % 16 == 0) {
                        writeln("add rsp, 8");
                        stack_offset -= 8;
                        as(stack_offset, prev);
                    }
                });
            }

            void reset_offset() {
                stack_offset = 0;
            }

            void reserve_stack(size_t offset) {
                writeln("sub rsp, ", utils::number::to_string<std::string>(offset));
                stack_offset += offset;
            }

            void discard_stack(size_t offset) {
                writeln("add rsp, ", utils::number::to_string<std::string>(offset));
                stack_offset -= offset;
            }

            // xor reg, reg
            void clear(Register reg) {
                writeln("xor ", reg_list[int(reg)], ", ", reg_list[int(reg)]);
            }

            // mov register, ptr [addr]
            bool load(Register to, Register from_addr, const std::shared_ptr<ast::Type>& ty) {
                auto ptr = ptr_access(reg_list[int(from_addr)], ty);
                if (!ptr) {
                    return false;
                }
                std::optional<std::string> reg;
                if (ptr->size == 4) {
                    reg = regs_map(to, 4);
                }
                else {
                    reg = regs_map(to, 8);
                }
                if (!reg) {
                    return false;
                }
                Instruction istr = Instruction::mov;
                if (ptr->size == 1 || ptr->size == 2) {
                    istr = Instruction::movzx;
                }
                writeln(istr_list[int(istr)], " ", *reg, ", ", ptr->to_addr());
                return true;
            }

            // mov ptr [addr], register
            bool store(Register to_addr, Register from, const std::shared_ptr<ast::Type>& ty) {
                auto ptr = ptr_access(reg_list[int(to_addr)], ty);
                if (!ptr) {
                    return false;
                }
                auto reg = regs_map(from, ty->size);
                if (!reg) {
                    return false;
                }
                writeln(istr_list[int(Instruction::mov)], " ", ptr->to_addr(), ", ", *reg);
                return true;
            }

            // mov register, register
            void transfer(Register to, Register from) {
                writeln(istr_list[int(Instruction::mov)], " ", reg_list[int(to)], ", ", reg_list[int(from)]);
            }

            // mov register, immediate
            void immediate(Register to, const std::string& imm) {
                writeln(istr_list[int(Instruction::mov)], " ", reg_list[int(to)], ", ", imm);
            }

            void immediate(Register to, size_t imm) {
                immediate(to, number::to_string<std::string>(imm));
            }

            // op ptr [addr], reg
            bool compound_assign(Instruction op, Register to_addr, Register from, std::shared_ptr<ast::Type>& ty) {
                auto ptr = ptr_access(reg_list[int(to_addr)], ty);
                if (!ptr) {
                    return false;
                }
                auto reg = regs_map(from, ty->size);
                if (!reg) {
                    return false;
                }
                writeln(istr_list[int(op)], " ", ptr->to_addr(), ", ", *reg);
                return true;
            }

            // function epiloge
            void cleanup() {
                transfer(Register::rsp, Register::rbp);
                writeln("pop rbp");
                writeln("ret");
            }

            void push_imm(const std::string& imm) {
                add_history(OptIstr::push_imm);
                f_imm = imm;
            }

            void push_rax() {
                add_history(OptIstr::push_rax);
            }

            void push_rdi() {
                add_history(OptIstr::push_rdi);
            }

            void pop_rdi() {
                add_history(OptIstr::pop_rdi);
            }

            void pop_rax() {
                add_history(OptIstr::pop_rax);
            }

            void push_rdx() {
                add_history(OptIstr::push_rdx);
            }

            void pop_rdx() {
                add_history(OptIstr::pop_rdx);
            }

            void write_label(auto&& a) {
                add_history(OptIstr::other);
                w.should_write_indent(false);
                w.writeln(a, ":");
            }

            void writeln(auto&&... s) {
                add_history(OptIstr::other);
                w.writeln(s...);
            }

            void write_comment(auto&&... s) {
                add_history(OptIstr::other);
                w.writeln("# ", s...);
            }

            void maybe_pop_rax(const std::shared_ptr<ast::Type>& ty) {
                if (ty->type != ast::ObjectType::void_type) {
                    pop_rax();
                }
            }

            void maybe_push_rax(const std::shared_ptr<ast::Type>& ty) {
                if (ty->type != ast::ObjectType::void_type) {
                    push_rax();
                }
            }
        };

        inline bool gen_expr(Context& ctx, ast::Expr* expr);

        inline void ptr_add(Context& ctx, ast::BinaryOp op, Register left, Register right, const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            if (op == ast::BinaryOp::add || op == ast::BinaryOp::sub) {
                // this is, ptr + int or int + ptr
                if (lty->type == ast::ObjectType::pointer_type &&
                    rty->type == ast::ObjectType::int_type) {
                    auto pointee = ast::deref(lty.get());
                    // multiply right operand
                    ctx.writeln("imul ", reg_list[int(right)], ", ", utils::number::to_string<std::string>(pointee->size));
                }
                else if (op != ast::BinaryOp::sub &&
                         rty->type == ast::ObjectType::pointer_type &&
                         lty->type == ast::ObjectType::int_type) {
                    auto pointee = ast::deref(rty.get());
                    // multiply left operand
                    ctx.writeln("imul ", reg_list[int(left)], ", ", utils::number::to_string<std::string>(pointee->size));
                }
            }
        }

        inline void ptr_sub(Context& ctx, ast::BinaryOp op, Register target, Register tmpreg, const std::shared_ptr<ast::Type>& lty, const std::shared_ptr<ast::Type>& rty) {
            if (op == ast::BinaryOp::sub &&
                lty->type == ast::ObjectType::pointer_type &&
                rty->type == ast::ObjectType::pointer_type) {
                auto bty = ast::deref(lty.get());
                // div by ptr len
                ctx.immediate(tmpreg, bty->size);
                ctx.writeln("cqo");
                ctx.writeln("idiv ", reg_list[int(target)], ", ", reg_list[int(tmpreg)]);
            }
        }

        inline bool gen_lval_expr(Context& ctx, ast::Expr* expr) {
            if (expr->type == ast::ObjectType::paren) {
                auto paren = static_cast<ast::Paren*>(expr);
                if (!gen_lval_expr(ctx, paren->expr.get())) {
                    return false;
                }
                return true;
            }
            if (expr->type == ast::ObjectType::unary) {
                auto v = static_cast<ast::Unary*>(expr);
                if (v->op == ast::UnaryOp::plus_sign) {
                    if (!gen_lval_expr(ctx, v->target.get())) {
                        return false;
                    }
                    return true;
                }
                if (v->op == ast::UnaryOp::deref) {
                    if (!gen_expr(ctx, v->target.get())) {
                        return false;
                    }
                    return true;
                }
            }
            if (expr->type == ast::ObjectType::index) {
                auto index = static_cast<ast::Index*>(expr);
                if (!gen_expr(ctx, index->object.get()) ||
                    !gen_expr(ctx, index->index.get())) {
                    return false;
                }
                ctx.pop_rdi();  // index
                ctx.pop_rax();  // object
                ptr_add(ctx, ast::BinaryOp::add, Register::rax, Register::rdi, index->object->expr_type, index->index->expr_type);
                ctx.writeln("add rax, rdi");
                ctx.push_rax();
                return true;
            }
            if (expr->type == ast::ObjectType::str_literal) {
                auto s = static_cast<ast::StrLiteral*>(expr);
                ctx.write_comment(s->str);
                ctx.writeln("lea rax, [rip + ", s->label, "]");
                ctx.push_rax();
                return true;
            }
            if (expr->type != ast::ObjectType::ident) {
                ctx.errs.push(Error{"expect lvalue but not", expr->pos});
                return false;
            }
            auto ident = static_cast<ast::Ident*>(expr);
            ctx.write_comment("&", ident->ident);
            if (ident->callee || ident->belong->global) {
                ctx.writeln("lea rax, [rip + ", ident->ident, "]");
            }
            else {
                ctx.writeln("lea rax, [rbp - ", utils::number::to_string<std::string>(ident->belong->offset), "]");
            }
            ctx.push_rax();
            return true;
        }

        // collected arguments are ordering right to left in source code
        inline void collect_arguments(Context& ctx, std::vector<ast::Expr*>& args, ast::Expr* expr) {
            if (!expr) {
                return;
            }
            if (expr->type == ast::ObjectType::binary &&
                static_cast<ast::Binary*>(expr)->op == ast::BinaryOp::comma) {
                auto b = static_cast<ast::Binary*>(expr);
                // eval right to left
                // call(x1,x2,x3)
                // then eval x3 -> x2 -> x1
                collect_arguments(ctx, args, b->right.get());
                collect_arguments(ctx, args, b->left.get());
                return;
            }
            args.push_back(expr);
        }

        inline bool gen_call(Context& ctx, ast::Call* c) {
            std::vector<ast::Expr*> args;
            collect_arguments(ctx, args, c->arguments.get());
            auto stack_arg = ctx.get_stack_arg(args.size());
            auto reg_num = ctx.get_arg_reg_len();
            auto reg_len = reg_num < args.size() ? reg_num : args.size();
            auto align = ctx.align_stack(stack_arg * 8);
            auto get_args = [&] {
                for (size_t i = 0; i < args.size(); i++) {
                    auto arg = args[i];
                    auto n = utils::number::to_string<std::string>(args.size() - 1 - i);
                    ctx.write_comment(n, " argument-->");
                    if (!gen_expr(ctx, arg)) {
                        return false;
                    }
                    ctx.write_comment("<--", n, " argument");
                }
                return true;
            };
            auto callee = c->callee.get();
            auto fn_type = static_cast<ast::FuncType*>(callee->expr_type.get());
            bool va_arg = (bool)fn_type->va_arg;
            auto set_args = [&] {
                auto regs = ctx.get_arg_regs();
                for (auto i = 0; i < reg_len; i++) {
                    ctx.write_comment("set ", utils::number::to_string<std::string>(i), " argument");
                    ctx.pop(regs[i]);
                }
                if (stack_arg) {
                    ctx.write_comment("remaining arguments are passed by stack");
                }
                if (va_arg) {
                    ctx.write_comment("set count of float for variable argument");
                    ctx.clear(Register::rax);
                }
                if (ctx.platform == Platform::win_x64) {
                    ctx.write_comment("for microsoft calling convertion");
                    ctx.writeln("sub rsp, 32");
                }
            };
            auto clean_args = [&] {
                if (ctx.platform == Platform::win_x64) {
                    ctx.writeln("add rsp, 32");
                }
                if (stack_arg) {
                    ctx.write_comment("discarding stack arguments");
                    ctx.discard_stack(stack_arg * 8);
                }
            };

            if (auto p = ast::as_ident(callee);
                p && p->callee) {
                if (!get_args()) {
                    return false;
                }
                set_args();
                ctx.writeln("call ", p->ident);  // direct call
            }
            else {
                if (!get_args()) {
                    return false;
                }
                if (!gen_lval_expr(ctx, callee)) {
                    return false;
                }
                if (callee->expr_type->type != ast::ObjectType::func_type) {
                    ctx.errs.push(Error{"callee is not a function", callee->pos});
                    return false;
                }
                ctx.pop_rax();
                ctx.transfer(Register::r10, Register::rax);
                set_args();
                ctx.writeln("call r10");
            }
            clean_args();
            align.execute();
            ctx.maybe_push_rax(fn_type->return_);
            return true;
        }

        inline bool gen_expr(Context& ctx, ast::Expr* expr) {
            if (expr->type == ast::ObjectType::int_literal) {
                ctx.push_imm(static_cast<ast::IntLiteral*>(expr)->raw);
            }
            else if (expr->type == ast::ObjectType::ident) {
                auto id = static_cast<ast::Ident*>(expr);
                if (!gen_lval_expr(ctx, id)) {
                    return false;
                }
                if (!id->belong || id->belong->def->type_->type != ast::ObjectType::array_type) {
                    ctx.pop_rax();
                    if (!ctx.load(Register::rax, Register::rax, id->expr_type)) {
                        return false;
                    }
                    ctx.push_rax();
                }
            }
            else if (expr->type == ast::ObjectType::str_literal) {
                if (!gen_lval_expr(ctx, expr)) {
                    return false;
                }
            }
            else if (expr->type == ast::ObjectType::binary) {
                auto b = static_cast<ast::Binary*>(expr);
                auto set_value = [&](auto& op) {
                    auto ptr = ptr_access("rax", b->expr_type);
                    if (!ptr) {
                        ctx.errs.push(Error{"unsupported ptr size", b->pos});
                        return false;
                    }
                    auto mapped = regs_map(Register::rdx, ptr->size);  // rdx has all size pattern
                    ctx.writeln(op, " ", ptr->to_addr(), ", ", *mapped);
                    return true;
                };
                switch (b->op) {
                    case ast::BinaryOp::logical_and: {
                        if (!gen_expr(ctx, b->left.get())) {
                            return false;
                        }
                        auto and_ = ctx.get_local_label("and");
                        ctx.pop_rax();
                        ctx.writeln("cmp rax, 0");
                        ctx.writeln("je ", and_);
                        if (!gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        ctx.pop_rax();
                        ctx.write_label(and_);
                        ctx.push_rax();
                        return true;
                    }
                    case ast::BinaryOp::logical_or: {
                        if (!gen_expr(ctx, b->left.get())) {
                            return false;
                        }
                        auto or_ = ctx.get_local_label("or");
                        ctx.pop_rax();
                        ctx.writeln("cmp rax, 0");
                        ctx.writeln("jne ", or_);
                        if (!gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        ctx.pop_rax();
                        ctx.write_label(or_);
                        ctx.push_rax();
                        return true;
                    }
                    case ast::BinaryOp::assign: {
                        if (!gen_lval_expr(ctx, b->left.get()) ||
                            !gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        ctx.pop_rdx();
                        ctx.pop_rax();
                        if (!ctx.store(Register::rax, Register::rdx, b->expr_type)) {
                            return false;
                        }
                        ctx.push_rdx();
                        return true;
                    }
                    case ast::BinaryOp::add_assign: {
                        if (!gen_lval_expr(ctx, b->left.get()) ||
                            !gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        ctx.pop_rdx();
                        ctx.pop_rax();
                        ptr_add(ctx, ast::BinaryOp::add, Register::rax, Register::rdx, b->left->expr_type, b->right->expr_type);
                        if (!ctx.compound_assign(Instruction::add, Register::rax, Register::rdx, b->left->expr_type)) {
                            return false;
                        }
                        if (!ctx.load(Register::rax, Register::rax, b->left->expr_type)) {
                            return false;
                        }
                        ctx.push_rax();
                        return true;
                    }
                    case ast::BinaryOp::sub_assign: {
                        if (!gen_lval_expr(ctx, b->left.get()) ||
                            !gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        ctx.pop_rdx();
                        ctx.pop_rax();
                        ptr_add(ctx, ast::BinaryOp::sub, Register::rax, Register::rdx, b->left->expr_type, b->right->expr_type);
                        if (!ctx.compound_assign(Instruction::sub, Register::rax, Register::rdx, b->left->expr_type)) {
                            return false;
                        }
                        if (!ctx.load(Register::rax, Register::rax, b->left->expr_type)) {
                            return false;
                        }
                        ctx.push_rax();
                        return true;
                    }
                    case ast::BinaryOp::comma: {
                        if (!gen_expr(ctx, b->left.get())) {
                            return false;
                        }
                        ctx.maybe_pop_rax(b->left->expr_type);
                        if (!gen_expr(ctx, b->right.get())) {
                            return false;
                        }
                        return true;
                    }
                    default:
                        break;
                }
                if (!gen_expr(ctx, b->left.get()) ||
                    !gen_expr(ctx, b->right.get())) {
                    return false;
                }
                ctx.pop_rdi();  // right
                ctx.pop_rax();  // left
                // for pointer addition
                ptr_add(ctx, b->op, Register::rax, Register::rdi, b->left->expr_type, b->right->expr_type);
                auto op = b->op;
                switch (op) {
                    case ast::BinaryOp::add: {
                        ctx.writeln("add rax, rdi");
                        break;
                    }
                    case ast::BinaryOp::sub: {
                        ctx.writeln("sub rax, rdi");
                        break;
                    }
                    case ast::BinaryOp::mul: {
                        ctx.writeln("imul rax, rdi");
                        break;
                    }
                    case ast::BinaryOp::div: {
                        ctx.writeln("cqo");
                        ctx.writeln("idiv rax, rdi");
                        break;
                    }
                    case ast::BinaryOp::mod: {
                        ctx.writeln("cqo");
                        ctx.writeln("idiv rax, rdi");
                        ctx.transfer(Register::rax, Register::rdx);
                        break;
                    }
                    case ast::BinaryOp::equal: {
                        ctx.writeln("cmp rax, rdi");
                        auto lab = ctx.get_local_label("equal");
                        ctx.immediate(Register::rax, 1);
                        ctx.writeln("je ", lab);
                        ctx.clear(Register::rax);
                        ctx.write_label(lab);
                        break;
                    }
                    case ast::BinaryOp::not_equal: {
                        ctx.writeln("cmp rax, rdi");
                        auto lab = ctx.get_local_label("not_equal");
                        ctx.immediate(Register::rax, 1);
                        ctx.writeln("jne ", lab);
                        ctx.clear(Register::rax);
                        ctx.write_label(lab);
                        break;
                    }
                    case ast::BinaryOp::less: {
                        ctx.writeln("cmp rax, rdi");
                        auto lab = ctx.get_local_label("less");
                        ctx.immediate(Register::rax, 1);
                        ctx.writeln("jl ", lab);
                        ctx.clear(Register::rax);
                        ctx.write_label(lab);
                        break;
                    }
                    case ast::BinaryOp::less_or_eq: {
                        ctx.writeln("cmp rax, rdi");
                        auto lab = ctx.get_local_label("less");
                        ctx.immediate(Register::rax, 1);
                        ctx.writeln("jle ", lab);
                        ctx.clear(Register::rax);
                        ctx.write_label(lab);
                        break;
                    }
                    default: {
                        ctx.errs.push(Error{"not supported binary operator ", expr->pos});
                        return false;
                    }
                }
                ptr_sub(ctx, b->op, Register::rax, Register::r9, b->left->expr_type, b->right->expr_type);
                ctx.push_rax();
            }
            else if (expr->type == ast::ObjectType::paren) {
                auto v = static_cast<ast::Paren*>(expr);
                if (!gen_expr(ctx, v->expr.get())) {
                    return false;
                }
                return true;
            }
            else if (expr->type == ast::ObjectType::unary) {
                auto u = static_cast<ast::Unary*>(expr);
                switch (u->op) {
                    case ast::UnaryOp::plus_sign: {
                        if (!gen_expr(ctx, u->target.get())) {
                            return false;
                        }
                        break;
                    }
                    case ast::UnaryOp::minus_sign: {
                        if (!gen_expr(ctx, u->target.get())) {
                            return false;
                        }
                        ctx.pop_rdi();
                        ctx.clear(Register::rax);
                        ctx.writeln("sub rax, rdi");
                        ctx.push_rax();
                        break;
                    }
                    case ast::UnaryOp::logical_not: {
                        if (!gen_expr(ctx, u->target.get())) {
                            return false;
                        }
                        ctx.pop_rax();
                        ctx.writeln("cmp rax, 0");
                        ctx.immediate(Register::rax, 1);
                        auto lab = ctx.get_local_label("not");
                        ctx.writeln("je ", lab);
                        ctx.clear(Register::rax);
                        ctx.write_label(lab);
                        ctx.push_rax();
                        break;
                    }
                    case ast::UnaryOp::address: {
                        if (!gen_lval_expr(ctx, u->target.get())) {
                            return false;
                        }
                        break;
                    }
                    case ast::UnaryOp::deref: {
                        if (!gen_expr(ctx, u->target.get())) {
                            return false;
                        }
                        ctx.pop_rax();
                        if (!ctx.load(Register::rax, Register::rax, u->expr_type)) {
                            return false;
                        }
                        ctx.push_rax();
                        break;
                    }
                    case ast::UnaryOp::size: {
                        ctx.push_imm(utils::number::to_string<std::string>(u->target->expr_type->size));
                        break;
                    }
                    case ast::UnaryOp::increment:
                    case ast::UnaryOp::decrement: {
                        if (!gen_lval_expr(ctx, u->target.get())) {
                            return false;
                        }
                        ctx.pop_rax();  // addr
                        if (u->target->expr_type->type == ast::ObjectType::pointer_type) {
                            auto bty = ast::deref(u->target->expr_type.get());
                            ctx.immediate(Register::rdi, bty->size);
                        }
                        else {
                            ctx.immediate(Register::rdi, 1);
                        }
                        if (!ctx.compound_assign(u->op == ast::UnaryOp::increment ? Instruction::add : Instruction::sub, Register::rax, Register::rdi, u->target->expr_type)) {
                            return false;
                        }
                        if (!ctx.load(Register::rax, Register::rax, u->target->expr_type)) {
                            return false;
                        }
                        ctx.push_rax();
                        break;
                    }
                    case ast::UnaryOp::post_increment:
                    case ast::UnaryOp::post_decrement: {
                        if (!gen_lval_expr(ctx, u->target.get())) {
                            return false;
                        }
                        ctx.pop_rax();  // addr
                        if (u->target->expr_type->type == ast::ObjectType::pointer_type) {
                            auto bty = ast::deref(u->target->expr_type.get());
                            ctx.immediate(Register::rdi, bty->size);
                        }
                        else {
                            ctx.immediate(Register::rdi, 1);
                        }
                        if (!ctx.load(Register::rdx, Register::rax, u->target->expr_type)) {
                            return false;
                        }
                        if (!ctx.compound_assign(u->op == ast::UnaryOp::post_increment ? Instruction::add : Instruction::sub, Register::rax, Register::rdi, u->target->expr_type)) {
                            return false;
                        }
                        ctx.push_rdx();
                        break;
                    }
                    default: {
                        ctx.errs.push(Error{"not supported unary operator ", expr->pos});
                        return false;
                    }
                }
            }
            else if (expr->type == ast::ObjectType::call) {
                if (!gen_call(ctx, static_cast<ast::Call*>(expr))) {
                    return false;
                }
            }
            else if (expr->type == ast::ObjectType::index) {
                auto index = static_cast<ast::Index*>(expr);
                if (!gen_lval_expr(ctx, expr)) {
                    return false;
                }
                ctx.pop_rax();
                if (!ctx.load(Register::rax, Register::rax, index->expr_type)) {
                    return false;
                }
                ctx.push_rax();
            }
            else {
                ctx.errs.push(Error{"not supported operation ", expr->pos});
                return false;
            }
            return true;
        }

        inline bool gen_obj(Context& ctx, ast::Object* obj);

        inline bool gen_if(Context& ctx, ast::IfStmt* if_) {
            if (!gen_expr(ctx, if_->cond.get())) {
                return false;
            }
            std::optional<std::string> else_;
            auto end_ = ctx.get_local_label("if.end");
            ctx.pop_rax();
            ctx.writeln("cmp rax, 0");
            if (if_->els_) {
                else_ = ctx.get_local_label("else");
                ctx.writeln("je ", *else_);
            }
            else {
                ctx.writeln("je ", end_);
            }
            if (!gen_obj(ctx, if_->then.get())) {
                return false;
            }
            if (else_) {
                ctx.writeln("jmp ", end_);
                ctx.write_label(*else_);
                if (!gen_obj(ctx, if_->els_.get())) {
                    return false;
                }
            }
            ctx.write_label(end_);
            return true;
        }

        inline bool gen_while(Context& ctx, ast::WhileStmt* while_) {
            auto start_ = ctx.get_local_label("while");
            ctx.write_label(start_);
            if (!gen_expr(ctx, while_->cond.get())) {
                return false;
            }
            auto end_ = ctx.get_local_label("while.end");
            ctx.pop_rax();
            ctx.writeln("cmp rax, 0");
            ctx.writeln("je ", end_);
            if (!gen_obj(ctx, while_->body.get())) {
                return false;
            }
            ctx.writeln("jmp ", start_);
            ctx.write_label(end_);
            return true;
        }

        inline bool gen_for(Context& ctx, ast::ForStmt* for_) {
            auto start_ = ctx.get_local_label("for");
            if (for_->init) {
                if (!gen_obj(ctx, for_->init.get())) {
                    return false;
                }
            }
            std::optional<std::string> end_;
            ctx.write_label(start_);
            if (for_->cond) {
                end_ = ctx.get_local_label("for.end");
                if (!gen_expr(ctx, for_->cond.get())) {
                    return false;
                }
                ctx.pop_rax();
                ctx.writeln("cmp rax, 0");
                ctx.writeln("je ", *end_);
            }
            if (!gen_obj(ctx, for_->body.get())) {
                return false;
            }
            if (for_->cont) {
                if (!gen_expr(ctx, for_->cont.get())) {
                    return false;
                }
                ctx.maybe_pop_rax(for_->cont->expr_type);
            }
            ctx.writeln("jmp ", start_);
            if (end_) {
                ctx.write_label(*end_);
            }
            return true;
        }

        inline bool gen_obj(Context& ctx, ast::Object* obj) {
            if (ast::is_typeof(obj->type, ast::ObjectType::expr)) {
                auto expr = static_cast<ast::Expr*>(obj);
                if (!gen_expr(ctx, expr)) {
                    return false;
                }
                ctx.maybe_pop_rax(expr->expr_type);
            }
            else if (obj->type == ast::ObjectType::return_) {
                auto ret = static_cast<ast::Return*>(obj);
                if (ret->value) {
                    if (!gen_expr(ctx, ret->value.get())) {
                        return false;
                    }
                    ctx.maybe_pop_rax(ret->value->expr_type);
                }
                ctx.cleanup();
            }
            else if (obj->type == ast::ObjectType::if_) {
                auto if_ = static_cast<ast::IfStmt*>(obj);
                if (!gen_if(ctx, if_)) {
                    return false;
                }
            }
            else if (obj->type == ast::ObjectType::while_) {
                auto while_ = static_cast<ast::WhileStmt*>(obj);
                if (!gen_while(ctx, while_)) {
                    return false;
                }
            }
            else if (obj->type == ast::ObjectType::for_) {
                auto for_ = static_cast<ast::ForStmt*>(obj);
                if (!gen_for(ctx, for_)) {
                    return false;
                }
            }
            else if (obj->type == ast::ObjectType::block) {
                auto block = static_cast<ast::Block*>(obj);
                for (auto& obj : block->objects) {
                    if (!gen_obj(ctx, obj.get())) {
                        return false;
                    }
                }
            }
            else if (obj->type == ast::ObjectType::field) {
                // nothing to do
            }
            else {
                ctx.errs.push(Error{"not supported operation ", obj->pos});
                return false;
            }
            return true;
        }

        inline bool gen_global_var(Context& ctx, ast::Field* f) {
            ctx.set_section(".data");
            ctx.write_label(f->ident);
            auto n = ctx.w.indent_scope();
            ctx.writeln(".zero ", utils::number::to_string<std::string>(f->type_->size));
            ctx.writeln(".align ", utils::number::to_string<std::string>(f->type_->align));
            return true;
        }

        inline void gen_string(Context& ctx, ast::Global* g) {
            for (auto s : g->strs) {
                ctx.set_section(".rodata");
                s->label = ctx.get_local_label("str");
                ctx.write_label(s->label);
                const auto c = ctx.w.indent_scope();
                ctx.writeln(".string ", s->str);
            }
        }

        inline bool gen_func(Context& ctx, ast::Func* f) {
            if (!f->body) {
                return true;  // nothing to do now
            }
            ctx.set_section(".text");
            ctx.write_label(f->ident);
            auto s = ctx.w.indent_scope();
            ctx.reset_offset();
            ctx.push(Register::rbp);
            ctx.transfer(Register::rbp, Register::rsp);
            size_t align16 = 0;
            if (f->variables.max_offset % 16) {
                align16 = 16 - f->variables.max_offset % 16;
            }
            ctx.reserve_stack(f->variables.max_offset + align16);
            size_t i = 0;
            if (f->params.size() > ctx.get_arg_reg_len()) {
                ctx.errs.push(Error{"currently, more than " + utils::number::to_string<std::string>(ctx.get_arg_reg_len()) + " parameters are not supported", f->pos});
                return false;
            }
            for (auto& param : f->params) {
                auto& p = f->variables.idents[param->ident];
                if (p.offset != 0) {
                    ctx.write_comment(param->ident, "-->");
                    auto offset = p.offset;
                    ctx.writeln("lea rax, [rbp - ", utils::number::to_string<std::string>(offset), "]");
                    auto ptr = ptr_access("rax", p.def->type_);
                    auto arg_reg = ctx.get_arg_regs()[i];
                    if (!ctx.store(Register::rax, arg_reg, p.def->type_)) {
                        return false;
                    }
                    ctx.write_comment(param->ident, "<--");
                }
                i++;
            }
            if (!gen_obj(ctx, f->body.get())) {
                return false;
            }
            if (f->ident == "main") {
                ctx.clear(Register::rax);
                ctx.cleanup();
            }
            else {
                if (f->return_->type == ast::ObjectType::void_type) {
                    ctx.clear(Register::rax);
                    ctx.cleanup();
                }
                else {
                    ctx.writeln("ud2");  // undefined behaviour
                }
            }
            return true;
        }

        inline bool gen_prog(Writer& w, Errors& errs, std::unique_ptr<ast::Program>& prog) {
            Context ctx{w, errs};
            ctx.writeln(".intel_syntax noprefix");
            ctx.writeln(".global main");
            ctx.writeln();
            gen_string(ctx, &prog->global);
            for (auto& p : prog->objects) {
                if (p->type == ast::ObjectType::func) {
                    if (!gen_func(ctx, static_cast<ast::Func*>(p.get()))) {
                        return false;
                    }
                    continue;
                }
                if (p->type == ast::ObjectType::field) {
                    if (!gen_global_var(ctx, static_cast<ast::Field*>(p.get()))) {
                        return false;
                    }
                    continue;
                }
                errs.push(Error{"expect function but not", p->pos});
                return false;
            }
            return true;
        }
    }  // namespace x64
}  // namespace utils::langc::gen
