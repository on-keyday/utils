/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// langdef - language parser definition
#pragma once
#include "parser/minl_primitive.h"
#include "parser/minl_binary.h"
#include "parser/minl_block.h"
#include "parser/minl_func.h"
#include "parser/minl_type.h"

namespace utils {
    namespace minilang {
        namespace langdef {
            namespace ps = parser;
            constexpr auto primitive = ps::or_(
                ps::parenthesis(ps::Br{"()", "(", ")"}),
                ps::type_primitive(),
                ps::func_(fe_expr),
                ps::string(true),
                ps::number(true),
                ps::ident(),
                ps::always_error("expect identifier but not"));

            constexpr auto after = ps::or_(
                ps::dot(ps::expecter(false, ps::Op{"."}, ps::Op{"->"})),
                ps::parenthesis(ps::Br{"()", "(", ")"}),
                ps::slices(ps::Br{"[]", "[", "]", ":"}));

        }  // namespace langdef
    }      // namespace minilang
}  // namespace utils
