/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parser_compiler - compile parser from text
#pragma once
#include "parser_base.h"

namespace utils {
    namespace parser {
        template <class Input, class String, class Kind, template <class...> class Vec, class Src>
        utils::wrap::shared_ptr<Parser<Input, String, Kind, Vec>> compile_parser(Sequencer<Src>& seq) {
        }
    }  // namespace parser
}  // namespace utils