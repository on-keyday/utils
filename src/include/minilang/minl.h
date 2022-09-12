/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minl - minilang
#pragma once
// using shared_ptr and weak_ptr
#include <memory>
// using string
#include <string>
#include "../helper/view.h"
#include "../core/sequencer.h"
#include "../escape/read_string.h"
#include "../number/prefix.h"
#include "../helper/space.h"

namespace utils {
    namespace minilang {
        using CView = helper::SizedView<const char>;
        using Seq = Sequencer<CView>;

        struct Pos {
            size_t begin = 0;
            size_t end = 0;
        };

        constexpr auto invalid_pos = Pos{size_t(~0), size_t(~0)};

        enum NodeType {
            nt_min = 0x0000,
            nt_binary = 0x0001,
            nt_cond = 0x0101,
            nt_for = 0x1101,
            nt_type = 0x0002,
            nt_arrtype = 0x0102,
            nt_gentype = 0x0202,
            nt_struct_field = 0x0202,
            nt_funcparam = 0x0302,
            nt_func = 0x1302,
            nt_typedef = 0x0003,
            nt_let = 0x0004,
            nt_block = 0x0005,
            nt_comment = 0x0006,
            nt_import = 0x0007,
            nt_wordstat = 0x0008,
            nt_min_derive_mask = 0x00ff,
            nt_second_derive_mask = 0x0fff,
            nt_max = 0xffff,
        };

        constexpr const char* node_type_to_string(unsigned int type) {
            switch (type) {
                case nt_min:
                    return "min";
                case nt_binary:
                    return "binary";
                case nt_cond:
                    return "cond";
                case nt_for:
                    return "for";
                case nt_type:
                    return "type";
                case nt_arrtype:
                    return "arrtype";
                case nt_struct_field:
                    return "struct_field";
                case nt_funcparam:
                    return "funcparam";
                case nt_func:
                    return "func";
                case nt_typedef:
                    return "typedef";
                case nt_let:
                    return "let";
                case nt_block:
                    return "block";
                case nt_comment:
                    return "comment";
                case nt_import:
                    return "import";
                case nt_wordstat:
                    return "wordstat";
                default:
                    return "(unknown)";
            }
        }

#ifndef MINL_Constexpr
#undef MINL_Constexpr
#endif
#ifdef __cpp_lib_constexpr_string
#define MINL_Constexpr constexpr
#else
#define MINL_Constexpr inline
#endif

        struct MinNode {
            const unsigned int node_type = 0;
            std::string str;
            // std::shared_ptr<MinNode> left;
            // std::shared_ptr<MinNode> right;
            Pos pos;
            MINL_Constexpr MinNode() = default;

           protected:
            MINL_Constexpr MinNode(int id)
                : node_type(id) {}
        };

        struct BinaryNode : MinNode {
            std::shared_ptr<MinNode> left;
            std::shared_ptr<MinNode> right;
            MINL_Constexpr BinaryNode()
                : MinNode(nt_binary) {}

           protected:
            MINL_Constexpr BinaryNode(int id)
                : MinNode(id) {}
        };

        template <class ErrC, class Str>
        struct log_raii {
            ErrC& errc;
            Str str;
            const char* file;
            int line;
            constexpr log_raii(ErrC& c, Str s, const char* file, int line)
                : errc(c), str(s), file(file), line(line) {
                errc.log_enter(s, file, line);
            }

            constexpr ~log_raii() {
                errc.log_leave(str, file, line);
            }
        };

#define MINL_FUNC_LOG(FUNC) auto log_object____ = log_raii(errc, FUNC, __FILE__, __LINE__);

        constexpr auto ident_default() {
            return [](auto& seq) -> bool {
                return !seq.eos() &&
                       !number::is_control_char(seq.current()) &&
                       !number::is_symbol_char(seq.current());
            };
        }

        constexpr auto cond_read(auto&& cond) {
            return [=](auto& str, auto& seq) {
                while (cond(seq)) {
                    str.push_back(seq.current());
                    seq.consume();
                }
                return str.size() != 0;
            };
        }

        constexpr auto ident_default_read = cond_read(ident_default());
        template <class T>
        concept PassCtx = requires(T t) {
            {t.user_defined};
            {t.more_inner};
            {t.parse_on};
        };

        constexpr auto primitive() {
            return [](auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("primitive")
                std::string str;
                helper::space::consume_space(seq);
                const size_t start = seq.rptr;
                Pos pos{start, start};
                auto num_read = [&]() {
                    if (!number::is_digit(seq.current())) {
                        return false;
                    }
                    int prefix = 10;
                    bool is_float = false;
                    if (!number::read_prefixed_number(seq, str, &prefix, &is_float)) {
                        errc.say("expect number but not");
                        errc.trace(start, seq);
                        str = {};
                    }
                    return true;
                };
                if (seq.current() == '"' || seq.current() == '\'') {
                    if (!escape::read_string(str, seq, escape::ReadFlag::parser_mode,
                                             escape::c_prefix())) {
                        errc.say("expect string but not");
                        errc.trace(start, seq);
                        return nullptr;
                    }
                }
                else if (seq.current() == '`') {
                    auto c = 0;
                    size_t count = 0;
                    if (!escape::varialbe_prefix_string(str, seq, escape::single_char_varprefix('`', c, count))) {
                        errc.say("expect raw string but not");
                        errc.trace(start, seq);
                        return nullptr;
                    }
                }
                else if (num_read()) {
                    if (str == decltype(str){}) {
                        return nullptr;
                    }
                }
                else {
                    if (!ident_default_read(str, seq)) {
                        errc.say("expect identifier but not");
                        errc.trace(start, seq);
                        return nullptr;
                    }
                }
                pos.end = seq.rptr;
                helper::space::consume_space(seq);
                auto node = std::make_shared<MinNode>();
                node->str = std::move(str);
                node->pos = pos;
                return node;
            };
        }

        constexpr auto simple_primitive() {
            return [&](auto& seq, auto& errc) {
                MINL_FUNC_LOG("simple_primitive")
                constexpr auto prim_ = primitive();
                return prim_(nullptr, seq, errc);
            };
        }

        inline auto make_ident_node(auto&& name, Pos pos) {
            auto res = std::make_shared<MinNode>();
            res->str = std::move(name);
            res->pos = pos;
            return res;
        }

        struct Op {
            const char* op = nullptr;
            char errs[2]{};
        };

        constexpr auto expecter(auto... op) {
            return [=](auto& seq, auto& expected, Pos& pos) {
                const auto start = seq.rptr;
                auto fold = [&](auto op) {
                    if (seq.seek_if(op.op)) {
                        for (auto c : op.errs) {
                            if (c == 0) {
                                break;
                            }
                            if (seq.current() == c) {
                                seq.rptr = start;
                                return false;
                            }
                        }
                        expected = op.op;
                        pos = {start, seq.rptr};
                        return true;
                    }
                    return false;
                };
                return (... || fold(op));
            };
        }

        constexpr auto unary(auto expect, auto inner) {
            auto f = [=](auto&& f, auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("unary")
                std::string str;
                Pos pos{};
                if (expect(seq, str, pos)) {
                    helper::space::consume_space(seq);
                    auto node = f(f, pass, seq, errc);
                    if (!node) {
                        return node;
                    }
                    auto tmp = std::make_shared<BinaryNode>();
                    tmp->str = std::move(str);
                    tmp->right = std::move(node);
                    tmp->pos = pos;
                    return tmp;
                }
                return inner(pass, seq, errc);
            };
            return [=](auto&& pass, auto& seq, auto& errc) {
                return f(f, pass, seq, errc);
            };
        }

        constexpr auto binary(auto expect, auto inner) {
            return [=](auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("binary")
                auto node = inner(pass, seq, errc);
                if (!node) {
                    return node;
                }
                std::string expected;
                Pos pos{};
                while (expect(seq, expected, pos)) {
                    helper::space::consume_space(seq, true);
                    auto right = inner(pass, seq, errc);
                    if (!right) {
                        return right;
                    }
                    auto tmp = std::make_shared<BinaryNode>();
                    tmp->str = std::move(expected);
                    tmp->left = node;
                    tmp->right = right;
                    tmp->pos = pos;
                    node = tmp;
                }
                return node;
            };
        }

        constexpr auto assign(auto expect, auto inner) {
            auto f = [=](auto&& f, auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("assign")
                auto node = inner(pass, seq, errc);
                if (!node) {
                    return node;
                }
                std::string expected;
                Pos pos{};
                while (expect(seq, expected, pos)) {
                    helper::space::consume_space(seq, true);
                    auto right = f(f, pass, seq, errc);
                    if (!right) {
                        return right;
                    }
                    auto tmp = std::make_shared<BinaryNode>();
                    tmp->str = std::move(expected);
                    tmp->left = node;
                    tmp->right = right;
                    tmp->pos = pos;
                    node = tmp;
                }
                return node;
            };
            return [=](auto&& pass, auto& seq, auto& errc) {
                return f(f, pass, seq, errc);
            };
        }

        constexpr auto make_binary(auto&& inner) {
            return inner;
        }

        constexpr auto make_binary(auto&& inner, auto&& one, auto&&... args) {
            return binary(std::forward<decltype(one)>(one),
                          make_binary(std::forward<decltype(inner)>(inner),
                                      std::forward<decltype(args)>(args)...));
        }

        enum BrCondition {
            brc_expr,
            brc_cond,
            brc_recursive,
        };

        struct Br {
            const char* op = nullptr;
            const char* begin = nullptr;
            const char* end = nullptr;
            const char* rem_comma = nullptr;
            bool suppression_on_cond = false;
        };

        // brackets makes brackets expression parser
        // inner argument is inner parser call
        // op arguments are instances of Br structure
        // if op.begin != nullptr then this function parses between op.begin and op.end
        // otherwise this function parses after op.op
        // argument of function that this function returns is different from others
        // signature is
        // bool(auto&& user_defined,auto&& more_inner,bool& err,std::shared_ptr<MinNode>& node,Sequencer& seq)
        constexpr auto brackets(auto... op) {
            return [=](auto&& expr, auto&& prim, auto&& pass, bool& err, std::shared_ptr<MinNode>& node, auto& seq, auto& errc) {
                MINL_FUNC_LOG("brackets")
                err = false;
                auto surrounded = [&](auto op) -> bool {
                    if (op.suppression_on_cond && pass.parse_on == brc_cond) {
                        return false;
                    }
                    const auto start = seq.rptr;
                    if (seq.seek_if(op.begin)) {
                        helper::space::consume_space(seq, true);
                        if (seq.seek_if(op.end)) {
                            auto tmp = std::make_shared<BinaryNode>();
                            tmp->str = op.op;
                            tmp->left = std::move(node);
                            tmp->pos = {start, seq.rptr};
                            node = std::move(tmp);
                            return true;
                        }
                        auto in = expr(pass, seq, errc);
                        if (!in) {
                            err = true;
                            return false;
                        }
                        if (op.rem_comma) {
                            helper::space::consume_space(seq, true);
                            seq.seek_if(op.rem_comma);
                            helper::space::consume_space(seq, true);
                        }
                        if (!seq.seek_if(op.end)) {
                            errc.say("expect ", op.end, " but not");
                            err = true;
                            return false;
                        }
                        const auto end = seq.rptr;
                        helper::space::consume_space(seq, true);
                        auto tmp = std::make_shared<BinaryNode>();
                        tmp->left = node;
                        tmp->str = op.op;
                        tmp->right = in;
                        tmp->pos = {start, end};
                        node = std::move(tmp);
                        return true;
                    }
                    return false;
                };
                auto not_surrounded = [&](auto op) -> bool {
                    if (!node) {
                        return false;
                    }
                    helper::space::consume_space(seq, true);
                    const auto start = seq.rptr;
                    if (!seq.seek_if(op.op)) {
                        return false;
                    }
                    const auto end = seq.rptr;
                    helper::space::consume_space(seq, true);
                    auto in = prim(pass, seq, errc);
                    if (!in) {
                        err = true;
                        return false;
                    }
                    helper::space::consume_space(seq, true);
                    auto tmp = std::make_shared<BinaryNode>();
                    tmp->left = node;
                    tmp->str = op.op;
                    tmp->right = in;
                    tmp->pos = {start, end};
                    node = std::move(tmp);
                    return true;
                };
                auto fold = [&](auto op) -> bool {
                    if (err) {
                        return false;
                    }
                    if (op.begin) {
                        return surrounded(op);
                    }
                    return not_surrounded(op);
                };
                return (... || fold(op));
            };
        }

        constexpr auto more_inner() {
            return [](auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                return pass.more_inner(pass, seq, errc);
            };
        }

        constexpr auto make_unary(auto inner) {
            return unary(expecter(Op{"*"}, Op{"++"}, Op{"--"}, Op{"+"}, Op{"-"}, Op{"&"}), inner);
        }

        constexpr auto make_binaries() {
            auto defines = expecter(Op{":="}, Op{"="}, Op{"<-"});
            auto comma = expecter(Op{","});
            auto op_assigns = expecter(Op{"+="}, Op{"-="},
                                       Op{"*="}, Op{"/="}, Op{"%="}, Op{"&="},
                                       Op{"|="}, Op{"<<="}, Op{">>="});
            auto or_ = expecter(Op{"||"});
            auto and_ = expecter(Op{"&&"});
            auto equals = expecter(Op{"=="}, Op{"!="});
            auto compare = expecter(Op{"<="}, Op{">="}, Op{"<", {'-'}}, Op{">"});
            auto adder = expecter(Op{"+", {'='}}, Op{"-", {'='}}, Op{"<<", {'='}}, Op{">>", {'='}});
            auto multipler = expecter(Op{"*", "="}, Op{"/", "="}, Op{"%", "="}, Op{"&", {'&', '='}}, Op{"|", {'|', '='}});
            auto unarys = make_unary(more_inner());
            auto bin_1 = make_binary(unarys, or_, and_, equals, compare, adder, multipler);
            auto bins = assign(defines, binary(comma, assign(op_assigns, bin_1)));
            return bins;
        }

        constexpr auto make_prim() {
            auto bins = make_binaries();
            auto brac = brackets(Br{"()", "(", ")"}, Br{"[]", "[", "]"},
                                 Br{"{}", "{", "}", ",", true}, Br{"."}, Br{"->"});
            auto prim = [=](auto&& pass, auto& seq, auto& errc)
                -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("prim")
                std::shared_ptr<MinNode> node;
                bool err = false;
                auto uscall = [&](auto& seq, BrCondition cond = brc_expr) {
                    auto before = pass.parse_on;
                    pass.parse_on = cond;
                    auto res = bins(pass, seq, errc);
                    pass.parse_on = before;
                    return res;
                };
                auto brcprim = [&uscall](auto&& pass, auto& seq, auto&& errc) -> std::shared_ptr<MinNode> {
                    std::shared_ptr<MinNode> node;
                    bool err = false;
                    if (pass.user_defined(uscall, seq, node, err, errc)) {
                        if (err) {
                            return nullptr;
                        }
                        return node;
                    }
                    else {
                        if (err) {
                            return nullptr;
                        }
                        constexpr auto prim_ = primitive();
                        return prim_(pass, seq, errc);
                    }
                };
                if (pass.user_defined(uscall, seq, node, err, errc)) {
                    if (err) {
                        return nullptr;
                    }
                }
                else if (brac(bins, brcprim, pass, err, node, seq, errc)) {
                }
                else {
                    if (err) {
                        return nullptr;
                    }
                    constexpr auto prim_ = primitive();
                    node = prim_(pass, seq, errc);
                    if (!node) {
                        return nullptr;
                    }
                }
                while (brac(bins, brcprim, pass, err, node, seq, errc))
                    ;
                if (err) {
                    return nullptr;
                }
                return node;
            };
            return prim;
        }

        constexpr auto make_exprs() {
            auto bins = make_binaries();
            auto prim = make_prim();
            return [=]<class T>(Sequencer<T>& seq, auto&& user_defined_primitive, auto&& errc, BrCondition cond = brc_expr) {
                struct {
                    const std::remove_reference_t<decltype(user_defined_primitive)>& user_defined;
                    const std::remove_reference_t<decltype(prim)>& more_inner;
                    BrCondition parse_on;
                } pass{user_defined_primitive, prim};
                return bins(pass, seq, errc, cond);
            };
        }

        constexpr auto make_exprs_with_statement(auto&& primitive, auto&& stats) {
            auto bins = make_binaries();
            auto prim = make_prim();
            return [=]<class T>(Sequencer<T>& seq, auto&& errc, BrCondition cond = brc_expr) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG("statements")
                struct {
                    const std::remove_reference_t<decltype(primitive)>& user_defined;
                    const std::remove_reference_t<decltype(prim)>& more_inner;
                    BrCondition parse_on;
                } pass{primitive, prim};
                auto expr = [&](auto& seq, BrCondition cond = brc_expr) {
                    auto before = pass.parse_on;
                    pass.parse_on = cond;
                    auto res = bins(pass, seq, errc);
                    pass.parse_on = before;
                    return res;
                };
                std::shared_ptr<MinNode> node;
                bool err = false;
                if (!stats(stats, expr, seq, node, err, errc)) {
                    return nullptr;
                }
                if (err) {
                    return nullptr;
                }
                return node;
            };
        }

        // minl_expr is basic and generic expression parser
        // seq argument is input to be parsed
        // user_defined_primitive argument is user defined primitive value parser
        // signature of user_defined_primitive is
        // bool(auto&& recursive,Sequecner& seq,std::shared_ptr<MinNode>& node,bool& err,auto& errc)
        // recursive is used for recursive parser call
        // seq is input
        // node is result of parse
        // using err argument, user_defined_primitive reports error
        // errc is user defined error reporter
        // return value represents user_defined_primitive processes input or not
        // if you don't defines user_defined_primitive, use null_userdefined instead
        constexpr auto minl_expr = make_exprs();

        // null_userdefined
        constexpr auto null_userdefined = [](auto&&, auto&, auto&, auto&, auto&) { return false; };

        struct NullErrC {
            constexpr void say(auto&&...) const {}
            constexpr void trace(size_t, auto&) const {}
            constexpr void log_enter(auto&&...) const {}
            constexpr void log_leave(auto&&...) const {}
        };

        constexpr auto null_errc = NullErrC{};

        constexpr bool expect_ident(auto& seq, auto ident) {
            const auto start = seq.rptr;
            if (!seq.seek_if(ident)) {
                return false;
            }
            if (ident_default()(seq)) {
                seq.rptr = start;
                return false;
            }
            return true;
        }

        constexpr bool match_ident(auto& seq, auto ident) {
            const auto start = seq.rptr;
            auto res = expect_ident(seq, ident);
            seq.rptr = start;
            return res;
        }
    }  // namespace minilang
}  // namespace utils
