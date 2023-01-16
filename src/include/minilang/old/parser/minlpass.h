/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlpass - minilang passing context
#pragma once
#include "../minnode.h"
#include <type_traits>

namespace utils {
    namespace minilang {
        namespace parser {
            enum class pass_loc {
                normal,
                condition,
                dot,
                swtich_expr,
            };

            template <class Expr, class Stat, class Type, class Prim, class Errc, class Seq>
            struct ParserContext {
                Expr expr;
                Stat stat;
                Type type;
                Prim prim;
                Seq seq;
                Errc errc;
                bool err = false;
                pass_loc loc = pass_loc::normal;
                std::shared_ptr<MinNode> tmppass;
            };

            template <class Expr, class Stat, class Type, class Prim, class Errc, class Seq>
            auto make_ctx_ref(Expr& expr, Stat& stat, Type& type, Prim& prim, Errc& errc, Seq& seq) {
                return ParserContext<Expr&, Stat&, Type&, Prim&, Errc&, Seq&>{
                    expr,
                    stat,
                    type,
                    prim,
                    seq,
                    errc,
                    false,
                    pass_loc::normal,
                };
            }

            template <class Expr, class Stat, class Type, class Prim, class Errc, class Seq>
            auto make_ctx_copy(Expr&& expr, Stat&& stat, Type&& type, Prim&& prim, Errc&& errc, Seq&& seq) {
                using ExprT = std::decay_t<Expr>;
                using StatT = std::decay_t<Stat>;
                using TypeT = std::decay_t<Type>;
                using PrimT = std::decay_t<Prim>;
                using ErrcT = std::decay_t<Errc>;
                using SeqT = std::decay_t<Seq>;
                return ParserContext<ExprT, StatT, TypeT, PrimT, ErrcT, SeqT>{
                    std::forward<Expr>(expr),
                    std::forward<Stat>(stat),
                    std::forward<Type>(type),
                    std::forward<Prim>(prim),
                    std::forward<Seq>(seq),
                    std::forward<Errc>(errc),
                    false,
                    pass_loc::normal,
                };
            }

            template <class Expr, class Stat, class Type, class Prim, class Errc, class Seq>
            auto make_ctx(Expr&& expr, Stat&& stat, Type&& type, Prim&& prim, Errc& errc, Seq& seq) {
                using ExprT = std::decay_t<Expr>;
                using StatT = std::decay_t<Stat>;
                using TypeT = std::decay_t<Type>;
                using PrimT = std::decay_t<Prim>;
                using ErrcT = Errc&;
                using SeqT = Seq&;
                return ParserContext<ExprT, StatT, TypeT, PrimT, ErrcT, SeqT>{
                    std::forward<Expr>(expr),
                    std::forward<Stat>(stat),
                    std::forward<Type>(type),
                    std::forward<Prim>(prim),
                    seq,
                    errc,
                    false,
                    pass_loc::normal,
                };
            }

            template <class T>
            struct ParserT;

            template <class Expr, class Stat, class Type, class Prim, class Errc, class Seq>
            struct ParserT<ParserContext<Expr, Stat, Type, Errc, Prim, Seq>> {
                using ExprT = Expr;
                using StatT = Stat;
                using TypeT = Type;
                using PrimT = Prim;
                using ErrcT = Errc;
                using SeqT = Seq;
                using ContextT = ParserContext<Expr, Stat, Type, Prim, Errc, Seq>;
            };

#define MINL_BEGIN_AND_START(seq)            \
    const size_t begin = seq.rptr;           \
    space::consume_space(seq, true); \
    const size_t start = seq.rptr

            struct NullErrC {
                void say(auto&&...) {
                    ;
                }
                void trace(auto start, auto&& seq) {
                    ;
                }
                void node(auto&& node) {
                    ;
                }
                void log_enter(auto&& func, auto file, auto line) {
                    ;
                }
                void log_leave(auto&& func, auto file, auto line) {
                    ;
                }
            };
        }  // namespace parser
    }      // namespace minilang
}  // namespace utils