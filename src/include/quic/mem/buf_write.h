/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// buf_write - buf write
#pragma once
#include "pool.h"
#include "../common/variable_int.h"
#include <cassert>

namespace utils {
    namespace quic {
        namespace mem {
            template <class Error>
            inline Error append_buf(bytes::Buffer& b, const byte* data, tsize len) {
                auto err = append(b, data, len);
                if (err < 0) {
                    return Error::memory_exhausted;
                }
                return err ? Error::none : Error::invalid_arg;
            }

            template <class Error>
            inline Error write_varint(bytes::Buffer& b, tsize data, varint::Error* verr) {
                tsize offset = b.len;
                auto enclen = varint::least_enclen(data);
                if (enclen == 0) {
                    if (verr) {
                        *verr = varint::Error::too_large_number;
                    }
                    return Error::varint_error;
                }
                byte s[8];
                if (auto err = append_buf<Error>(b, s, enclen); err != Error::none) {
                    return Error::memory_exhausted;
                }
                auto ptr = b.b.own();
                assert(ptr);
                auto err = varint::encode(ptr, data, enclen, b.b.size(), &offset);
                if (err != varint::Error::none) {
                    if (verr) {
                        *verr = err;
                    }
                    return Error::varint_error;
                }
                assert(offset == b.len);
                return Error::none;
            }

        }  // namespace mem
    }      // namespace quic
}  // namespace utils
