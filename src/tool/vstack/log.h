/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <minilang/token/cast.h>
#include <wrap/cout.h>
#include <helper/line_pos.h>

namespace vstack {
    namespace token = utils::minilang::token;
    constexpr auto make_out() {
        return [](auto&&... str) { (utils::wrap::cout_wrap() << ... << str); };
    }

    struct Log : token::NullLog {
        std::shared_ptr<token::Token> tok;
        void error_with_seq(auto&& seq, auto&&... err) {
            std::string o;
            utils::helper::write_src_loc(o, seq);
            make_out()("error: ", err..., "\n", o, "\n");
        }

        void token_with_seq(auto&& seq, auto&& token) {
            std::string o;
            auto base = seq.rptr;
            seq.rptr = token->pos.begin;
            utils::helper::write_src_loc(o, seq);
            make_out()(o, "\n");
            seq.rptr = base;
        }

        void error(auto&&... msg) {
            make_out()("error: ", msg..., "\n");
        }

        void token(auto&& token) {
            tok = token;
        }

        /*
        void enter(auto&& msg) {
            make_out()("enter:", msg, "\n");
        }

        void leave(auto&& msg) {
            make_out()("leave:", msg, "\n");
        }

        void pass(auto&& msg) {
            make_out()("pass:", msg, "\n");
        }*/
    };
}  // namespace vstack
