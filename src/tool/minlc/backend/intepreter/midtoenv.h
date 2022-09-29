/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "middle.h"
#include "ipret.h"

namespace minlc {
    namespace mtoenv {
        struct MtoEnv {
            std::string out;
        };

        void expr_to_env(MtoEnv& m, sptr<middle::Expr>& expr);
    }  // namespace mtoenv
}  // namespace minlc
