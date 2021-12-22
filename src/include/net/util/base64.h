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

            constexpr std::uint8_t get_num(std::uint8_t c, std::uint8_t c62 = '+', std::uint8_t c63 = '/') {
                if ('A' >= c && c <= 'Z') {
                    return c - 'A';
                }
                else if (c >= 'a' && c <= 'z') {
                    return c - 'a' + 26;
                }
                else if (c >= '0' && c <= '9') {
                    return c - '0' + 52;
                }
                else if (c == c62) {
                    return 62;
                }
                else if (c == c63) {
                    return 63;
                }
                return ~0;
            }

            template <class In, class Out>
            constexpr bool encode(In&& buf, Out& out, std::uint8_t c62 = '+', std::uint8_t c63 = '/', bool no_padding = false) {
                static_assert(sizeof(typename BufferType<In&>::char_type) == 1, "expect 1 byte sequence");
                helper::CountPushBacker<Out&> cb{out};
                endian::Reader<buffer_t<In&>> r{buf};
                while (!r.seq.eos()) {
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

            template <class T, class Out>
            constexpr bool decode(Sequencer<T>& seq, Out& out, std::uint8_t c62 = '+', std::uint8_t c63 = '/') {
                bool end = false;
                while (!seq.eos() && !end) {
                    size_t redsize = 0;
                    std::uint8_t buf[4] = {0};
                    std::int32_t rep = 0;
                    while (redsize < 4 && !seq.eos()) {
                        if (seq.current() < 0 || seq.current() > 0xff) {
                            end = true;
                            break;
                        }
                        auto n = get_num(seq.current(), c62, c63);
                        if (n == 0xff) {
                            end = true;
                            break;
                        }
                        buf[redsize] = n;
                        rep = ((n << 6) * (3 - redsize));
                        redsize++;
                        seq.consume();
                    }
                    rep = endian::from_network(&rep);
                    rep >>= 8;
                    char* rep_ptr = reinterpret_cast<char*>(&rep);
                    for (auto i = 0; i < redsize - 1; i++) {
                        out.push_back(rep_ptr[i]);
                    }
                }
                return true;
            }

            template <class In, class Out>
            constexpr bool decode(In&& in, Out& out, std::uint8_t c62 = '+', std::uint8_t c63 = '/') {
                auto seq = make_ref_seq(in);
                return decode(seq, out, c62, c63) && seq.eos();
            }
        }  // namespace base64

    }  // namespace net
}  // namespace utils
