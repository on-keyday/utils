/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// connid - connection id
#pragma once

#include "../doc.h"
#include "../mem/bytes.h"
#include "../mem/pool.h"

namespace utils {
    namespace quic {
        namespace conn {
            // ConnID holds Connection ID
            struct ConnID {
                bytes::Bytes id;
                tsize len;
            };

            constexpr tsize InvalidLength = ~0;

            inline bool operator==(const ConnID& a, const ConnID& b) {
                // validate range
                if (a.id.size() < a.len || b.id.size() < b.len) {
                    return false;
                }
                // validate length
                if (a.len != b.len) {
                    return false;
                }
                const byte* a_str = a.id.c_str();
                const byte* b_str = b.id.c_str();
                // validate pointer
                if (a_str == b_str) {
                    return true;
                }
                // validate nullptr
                if (!a_str || !b_str) {
                    return a.len == 0 && b.len == 0;
                }
                // validate byte details
                tsize len = a.len;
                for (tsize i = 0; i < len; i++) {
                    if (a_str[i] != b_str[i]) {
                        return false;
                    }
                }
                return true;
            }

            struct ConnIDList {
                void* C;
                ConnID (*fetch_)(void* C, const byte* b, tsize size);
                ConnID get_known(pool::BytesPool& bpool, auto&& b, tsize size, tsize* offset, tsize max_len) {
                    tsize remain = size - *offset;
                    tsize to_fetch = remain > max_len ? max_len : remain;
                    ConnID ret;
                    pool::select_bytes_way<bool, true, false>(b, bpool, *offset + to_fetch, true, *offset, [&](const byte* b) {
                        ret = fetch_(C, b, to_fetch);
                        return true;
                    });
                    return ret;
                }
            };

            struct Token {
                bytes::Bytes token;
                tsize len;
            };
        }  // namespace conn
    }      // namespace quic
}  // namespace utils
