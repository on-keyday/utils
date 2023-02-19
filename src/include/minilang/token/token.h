/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// token - tokenizer
#pragma once
#include "../../core/sequencer.h"
#include <string>
#include <memory>
#include "../../helper/defer.h"
#include <functional>

namespace utils {
    namespace minilang::token {
        enum class Kind {
            // primitives

            ident,
            number,
            string,
            keyword,
            symbol,
            space,
            line,
            comment,

            // sentry symbol

            bof,  // begin of file
            eof,  // end of file

            // complex

            unary_tree,
            bin_tree,
            tri_tree,
            block,
            brackets,

            // user defined

            func_,
            if_,
            for_,
            var_,
            type_,
            typedef_,
            struct_,
            interface_,
            union_,
            enum_,
            import_,
            export_,
            module_,
            packege_,
            ref_,
            user_defined_,
        };

        constexpr bool is_primitive(Kind kind) {
            return int(Kind::ident) <= int(kind) && int(kind) <= int(Kind::comment);
        }

        struct Pos {
            size_t begin;
            size_t end;

            constexpr auto operator==(const Pos& a) const {
                return begin == a.begin &&
                       end == a.end;
            }
        };

        struct Token {
            const Kind kind;
            Pos pos;

           protected:
            Token(Kind k)
                : kind(k) {}
        };

        template <class T, class Ctx, class Logger>
        struct Source {
            Sequencer<T> seq;
            Ctx ctx;
            Logger log;
            bool err = false;
            std::shared_ptr<Token> next;
        };

        template <class Logger, class T, class Ctx>
        Source<T, Ctx, Logger> make_source(Sequencer<T>&& seq, Ctx&& ctx) {
            return Source<T, Ctx, Logger>{std::move(seq), std::move(ctx)};
        }

        constexpr auto yield_single_token(bool should_move = false) {
            return [=](auto&& src) -> std::shared_ptr<Token> {
                if (!src.next) {
                    src.next = src.ctx.yield_token(src);
                }
                if (should_move) {
                    return std::move(src.next);
                }
                else {
                    return src.next;
                }
            };
        }

        constexpr bool load_next(auto&& src) {
            constexpr auto load = yield_single_token(false);
            return load(src) != nullptr;
        }

        namespace internal {
            template <class T, class V>
            concept has_lookup_first = requires(T t, V v) {
                                           { t.lookup_first(v) };
                                       };
        }

        inline std::shared_ptr<Token> lookup_first(auto&& src, const auto& token) {
            if constexpr (internal::has_lookup_first<decltype(src.ctx), decltype(token)>) {
                return src.ctx.lookup_first(token);
            }
            return nullptr;
        }

        template <class Tokens, class Primitive, class Statement, class Expr>
        struct Yielder {
            Tokens tokens;
            Primitive primitive;
            Statement statement;
            Expr expr;
        };

        template <class... YieldArg>
        struct Context {
            Yielder<YieldArg...> yielder;

            std::shared_ptr<Token> yield_token(auto&& src) {
                return yielder.tokens(src);
            }
        };

        template <class Tokens, class Prim, class Stat, class Expr>
        auto make_context(Tokens&& tokens, Prim&& prim, Stat&& stat, Expr&& expr) {
            return Context<std::decay_t<Tokens>, std::decay_t<Prim>, std::decay_t<Stat>, std::decay_t<Expr>>{
                .yielder = {std::move(tokens), std::move(prim), std::move(stat), std::move(expr)},
            };
        }

        struct PrimitiveToken : Token {
            std::string token;

           protected:
            PrimitiveToken(Kind k)
                : Token(k) {}
        };

        constexpr auto yield_oneof(auto&&... fn) {
            return [=](auto&& src, auto&&... args) -> std::shared_ptr<Token> {
                std::shared_ptr<Token> token;
                auto fold = [&](auto&& f) {
                    token = f(src, args...);
                    if (!token) {
                        if (src.err) {
                            return true;
                        }
                        return false;
                    }
                    return true;
                };
                (... || fold(fn));
                return token;
            };
        }

        constexpr auto yield_nullptr() {
            return [](auto&& src) {
                return nullptr;
            };
        }

        constexpr auto not_eof_then(auto&& then) {
            return [=](auto&& src, auto&&... a) {
                if (src.seq.eos()) {
                    return decltype(then(src, a...))(nullptr);
                }
                return then(src, a...);
            };
        }

        [[nodiscard]] constexpr auto trace_log(auto& src, auto msg) {
            src.log.enter(msg);
            return helper::defer([&, msg] {
                src.log.leave(msg);
            });
        }

        constexpr void pass_log(auto&& src, auto msg) {
            src.log.pass(msg);
        }

        constexpr void maybe_log(auto&& src, auto msg) {
            src.log.maybe(msg);
        }

        constexpr void error_log(auto&& src, auto&&... msg) {
            src.log.error_with_seq(src.seq, msg...);
        }

        constexpr void token_log(auto&& src, auto&& token) {
            src.log.token_with_seq(src.seq, token);
        }

        struct NullLog {
            void enter(auto&& msg) {}
            void leave(auto&& msg) {}
            void error_with_seq(auto&& seq, auto&&...) {}
            void error(auto&&...) {}
            void maybe(auto&& msg) {}
            void pass(auto&& msg) {}
            void token_with_seq(auto&& seq, auto&&...) {}
            void token(auto&&) {}
        };

        constexpr auto yield_error(auto&& msg) {
            return [=](auto&& src, auto&&...) {
                error_log(src, msg);
                src.err = true;
                return nullptr;
            };
        }

    }  // namespace minilang::token
}  // namespace utils

namespace std {

    template <>
    struct hash<utils::minilang::token::Pos> {
        constexpr size_t operator()(utils::minilang::token::Pos pos) const {
            return pos.begin;
        }
    };

}  // namespace std
