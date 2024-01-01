/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <helper/expected.h>

namespace futils::wasm {
    enum class Error {
        short_input,
        large_input,
        large_output,
        short_buffer,
        encode_uint,
        encode_int,
        decode_uint,
        decode_int,
        decode_float,
        encode_float,
        decode_utf8,
        encode_utf8,
        large_int,
        unexpected_type,
        unexpected_instruction,
        unexpected_instruction_arg,
        unexpected_import_desc,

        magic_mismatch,
    };

    template <class T>
    using result = futils::helper::either::expected<T, Error>;

    auto unexpect(auto&& v) {
        return futils::helper::either::unexpected(std::forward<decltype(v)>(v));
    }

}  // namespace futils::wasm