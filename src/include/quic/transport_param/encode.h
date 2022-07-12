/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encode - transport parameter encoding
#pragma once
#include "../common/variable_int.h"

namespace utils {
    namespace quic {
        namespace tpparam {
            template <class Bytes>
            varint::Error decode(Bytes&& bytes, tsize size, tsize* offset, auto&& callback) {
                tsize id, len, red;
                auto err = varint::decode(bytes, &id, &red, size, *offset);
                if (err != varint::Error::none) {
                    return err;
                }
                *offset += red;
                auto err = varint::decode(bytes, &len, &red, size, *offset);
                if (err != varint::Error::none) {
                    return err;
                }
                *offset += red;
                return callback(id, len);
            }
        }  // namespace tpparam
    }      // namespace quic
}  // namespace utils
