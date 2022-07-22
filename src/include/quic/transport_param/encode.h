/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// encode - transport parameter encoding
#pragma once
#include "../common/variable_int.h"
#include "../mem/bytes.h"
#include "../mem/alloc.h"
#include <cassert>

namespace utils {
    namespace quic {
        namespace tpparam {
            struct TransportParam {
                tsize id;
                tsize len;
                bytes::Bytes data;
            };

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

            varint::Error encode(bytes::Buffer& b, TransportParam& param) {
                auto idlen = varint::least_enclen(param.id);
                auto lenlen = varint::least_enclen(param.len);
                if (param.len <= param.data.size()) {
                    return varint::Error::invalid_argument;
                }
                if (!idlen || !lenlen) {
                    return varint::Error::too_large_number;
                }
                byte s[16];
                tsize offset = b.len;
                auto ok = append(b, s, idlen + lenlen);
                if (ok <= 0) {
                    return varint::Error::need_more_length;
                }
                auto err = varint::encode(b.b.c_str(), param.id, idlen, b.len, &offset);
                if (err != varint::Error::none) {
                    return err;
                }
                err = varint::encode(b.b.c_str(), param.id, lenlen, b.len, &offset);
                if (err != varint::Error::none) {
                    return err;
                }
                assert(b.len == offset);
                return append(b, param.data.c_str(), param.len) <= 0
                           ? varint::Error::need_more_length
                           : varint::Error::none;
            }
        }  // namespace tpparam
    }      // namespace quic
}  // namespace utils
