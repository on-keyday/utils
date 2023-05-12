/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include <utility>
#include "../hpack/hpack_encode.h"
#include "error.h"
#include "field_refs.h"

namespace utils {
    namespace qpack {

        struct Field {
            view::rvec head;
            view::rvec value;
        };

        namespace internal {
            QpackError convert_hpack_error(hpack::HpackError err) {
                switch (err) {
                    case hpack::HpackError::input_length:
                        return QpackError::input_length;
                    case hpack::HpackError::output_length:
                        return QpackError::output_length;
                    case hpack::HpackError::too_large_number:
                    case hpack::HpackError::too_short_number:
                        return QpackError::encoding_error;
                    default:
                        return QpackError::unexpected_error;
                }
            }
        }  // namespace internal

    }  // namespace qpack
}  // namespace utils
