/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// minlstat - minilang statement
#pragma once
#include "minltype.h"
#include "minlfunc.h"
#include "minlctrl.h"
#include "minlprog.h"

namespace utils {
    namespace minilang {
        constexpr auto stat_default = statment(
            one_word_symbol(";", nilstat_str_),
            block_stat(), typedef_default,
            wrap_with_type(let_stat(), typesig_default), if_statement(), for_statement(),
            switch_statement(), wrap_with_type(func_expr(fe_def), typesig_default),
            one_word("break", break_str_), one_word("continue", continue_str_),
            one_word_plus_expr("return", return_str_, true),
            one_word_plus_and_block("linkage", linkage_str_, simple_primitive(), true),
            an_expr());

        constexpr auto mod_ = "(mod)";

        constexpr auto stat_head_default = statment(
            one_word_plus("mod", mod_, simple_primitive()),
            imports(simple_primitive()));

    }  // namespace minilang
}  // namespace utils
