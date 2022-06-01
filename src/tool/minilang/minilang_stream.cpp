/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "minilang.h"
#include <parser/stream/token_stream.h>
#include <parser/stream/tree_stream.h>
#include <number/char_range.h>

namespace minilang {
    namespace st = utils::parser::stream;

    st::Stream make_stream() {
        auto num = st::make_simplecond<wrap::string>("number", [](const char* c) {
            return utils::number::is_digit(*c);
        });
        return st::make_binary(num, st::Expect{"+"}, st::Expect{"-"});
    }

    void test_code() {
    }
}  // namespace minilang
