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
#include <helper/view.h>
#include <escape/read_string.h>
#include <number/prefix.h>
#include <helper/space.h>
#include "../minnode.h"

namespace utils {
    namespace minilang {
        using CView = helper::SizedView<const char>;
        using Seq = Sequencer<CView>;

        template <class T>
        concept PassCtx = requires(T t) {
            {t.user_defined};
            {t.more_inner};
            {t.parse_on};
        };

        constexpr auto primitive() {
            return [](auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG_OLD("primitive")
                std::string str;
                helper::space::consume_space(seq, true);
                const auto start = seq.rptr;
                auto num_read = [&](std::shared_ptr<NumberNode>& node) {
                    if (!number::is_digit(seq.current())) {
                        return false;
                    }
                    int prefix = 10;
                    bool is_float = false;
                    if (!number::read_prefixed_number(seq, str, &prefix, &is_float)) {
                        errc.say("expect number but not");
                        errc.trace(start, seq);
                        return true;
                    }
                    node = std::make_shared<NumberNode>();
                    node->is_float = is_float;
                    node->radix = prefix;
                    node->pos = {start, seq.rptr};
                    node->str = std::move(str);
                    number::NumConfig conf{number::default_num_ignore()};
                    conf.offset = prefix == 10 ? 0 : 2;
                    if (is_float) {
                        node->parsable = number::parse_float(node->str, node->floating, node->radix, conf);
                    }
                    else {
                        node->parsable = number::parse_integer(node->str, node->integer, node->radix, conf);
                    }
                    return true;
                };
                if (seq.current() == '"' || seq.current() == '\'') {
                    auto pf = seq.current();
                    if (!escape::read_string(str, seq, escape::ReadFlag::parser_mode,
                                             escape::c_prefix())) {
                        errc.say("expect string but not");
                        errc.trace(start, seq);
                        return nullptr;
                    }
                    auto node = std::make_shared<StringNode>();
                    node->pos = {start, seq.rptr};
                    node->prefix = pf;
                    node->store_c = 0;
                    node->str = std::move(str);
                    return node;
                }
                else if (seq.current() == '`') {
                    auto c = 0;
                    size_t count = 0;
                    if (!escape::varialbe_prefix_string(str, seq, escape::single_char_varprefix('`', c, count))) {
                        errc.say("expect raw string but not");
                        errc.trace(start, seq);
                        return nullptr;
                    }
                    auto node = std::make_shared<StringNode>();
                    node->pos = {start, seq.rptr};
                    node->prefix = '`';
                    node->store_c = c;
                    node->str = std::move(str);
                    return node;
                }
                else if (std::shared_ptr<NumberNode> node; num_read(node)) {
                    return node;
                }
                if (!ident_default_read(str, seq)) {
                    errc.say("expect identifier but not");
                    errc.trace(start, seq);
                    return nullptr;
                }
                auto node = std::make_shared<MinNode>();
                node->str = std::move(str);
                node->pos = {start, seq.rptr};
                return node;
            };
        }

        constexpr auto simple_primitive() {
            return [&](auto& seq, auto& errc) {
                MINL_FUNC_LOG_OLD("simple_primitive")
                constexpr auto prim_ = primitive();
                return prim_(nullptr, seq, errc);
            };
        }

        struct Op {
            const char* op = nullptr;
            char errs[2]{};
        };

        constexpr auto expecter(auto... op) {
            return [=](auto& seq, auto& expected, Pos& pos) {
                auto fold = [&](auto op) {
                    const auto tmp = seq.rptr;
                    helper::space::consume_space(seq, false);
                    const auto start = seq.rptr;
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
                    seq.rptr = tmp;
                    return false;
                };
                return (... || fold(op));
            };
        }

        constexpr auto unary(auto expect, auto inner) {
            auto f = [=](auto&& f, auto&& pass, auto& seq, auto& errc) -> std::shared_ptr<MinNode> {
                MINL_FUNC_LOG_OLD("unary")
                std::string str;
                Pos pos{};
                if (expect(seq, str, pos)) {
                    helper::space::consume_space(seq, true);
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
                MINL_FUNC_LOG_OLD("binary")
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
                MINL_FUNC_LOG_OLD("assign")
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
                MINL_FUNC_LOG_OLD("brackets")
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
                        helper::space::consume_space(seq, true);
                        if (op.rem_comma) {
                            seq.seek_if(op.rem_comma);
                            helper::space::consume_space(seq, true);
                        }
                        if (!seq.seek_if(op.end)) {
                            errc.say("expect ", op.end, " but not");
                            err = true;
                            return false;
                        }
                        const auto end = seq.rptr;
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
                MINL_FUNC_LOG_OLD("prim")
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
                MINL_FUNC_LOG_OLD("statements")
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

    }  // namespace minilang
}  // namespace utils
