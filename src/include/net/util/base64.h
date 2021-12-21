/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// base64 - parse base64
#pragma once

#include "../../core/sequencer.h"
#include "../../helper/view.h"
#include "../../endian/reader.h"

namespace utils {
    namespace net {
        namespace base64 {

            constexpr std::uint8_t get_char(std::uint8_t num, std::uint8_t c62 = '+', std::uint8_t c63 = '/') {
                if (num < 26) {
                    return 'A' + num;
                }
                else if (num >= 26 && num <= 52) {
                    return 'a' + (num - 26);
                }
                else if (num >= 52 && num < 62) {
                    return '0' + (num - 52);
                }
                else if (num == 62) {
                    return c62;
                }
                else if (num == 63) {
                    return c63;
                }
                return 0;
            }

            template <class T, class Out>
            constexpr bool encode(Sequencer<T>& seq, Out& out, std::uint8_t c62 = '+', std::uint8_t c63 = '/', bool no_padding = false) {
                static_assert(sizeof(typename BufferType<T>::char_type) == 1, "expect 1 byte sequence");
                helper::CountPushBacker<Out&> cb{out};
                endian::Reader<buffer_t<std::remove_reference_t<T>&>> r{seq.buf};
                while (!seq.eos()) {
                    std::uint32_t num;
                    auto red = r.template read_ntoh<std::uint32_t, 4, 1, true>(num);
                    if (!red) {
                        break;
                    }
                    std::uint8_t buf[] = {std::uint8_t((num >> 18) & 0x3f), std::uint8_t((num >> 12) & 0x3f), std::uint8_t((num >> 6) & 0x3f), std::uint8_t(num & 0x3f)};
                    for (auto i = 0; i < red + 1; i++) {
                        cb.push_back(get_char(buf[i]));
                    }
                }
                while (!no_padding && cb.count % 4) {
                    cb.push_back('=');
                }
                return true;
            }
        }  // namespace base64
    }      // namespace net
}  // namespace utils
