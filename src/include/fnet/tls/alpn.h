/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// alpn - wrapper of alpn selecter
#pragma once
#include "../../core/byte.h"
#include "../../view/iovec.h"
#include <cstdint>

namespace utils {
    namespace fnet::tls {
        struct ALPNSelector {
           private:
            const byte* input = nullptr;
            std::uint32_t input_len = 0;
            const byte** output = nullptr;
            byte* ouput_len = 0;

            constexpr bool is_out_of_range(view::rvec protocol_name) const {
                if (std::is_constant_evaluated()) {
                    return false;
                }
                return protocol_name.begin() < input || protocol_name.end() <= input + input_len;
            }

           public:
            constexpr ALPNSelector(const byte* in, std::uint32_t in_len, const byte** out, byte* out_len)
                : input(in), input_len(in_len), output(out), ouput_len(out_len) {
            }

            constexpr bool next(view::rvec& protocol_name) const {
                if (!input) {
                    return false;
                }
                if (protocol_name.null() || is_out_of_range(protocol_name)) {
                    if (input[0] == 0) {
                        protocol_name = {};
                        return false;
                    }
                    protocol_name = view::rvec(input + 1, input[0]);
                    return true;
                }
                auto next_len = *protocol_name.end();
                auto next_name = view::rvec(protocol_name.end() + 1, next_len);
                if (next_len == 0 ||
                    is_out_of_range(next_name)) {
                    protocol_name = {};
                    return false;
                }
                protocol_name = next_name;
                return true;
            }

            constexpr bool unselect() {
                if (!output || !ouput_len) {
                    return false;
                }
                *output = nullptr;
                *ouput_len = 0;
                return true;
            }

            constexpr bool select(view::rvec protocol_name) {
                if (!output || !ouput_len) {
                    return false;
                }
                if (is_out_of_range(protocol_name) || protocol_name.size() > 0xff) {
                    return false;
                }
                *output = protocol_name.data();
                *ouput_len = protocol_name.size();
                return true;
            }
        };

        namespace test {
            constexpr bool check_alpn() {
                const byte* out = nullptr;
                byte out_len = 0;
                const byte in[] = "\x02h2\x02h3\x08http/1.1";
                ALPNSelector select{in, sizeof(in), &out, &out_len};
                view::rvec data;
                while (select.next(data)) {
                    if (data.null()) {
                        return false;
                    }
                    if (!select.select(data)) {
                        return false;
                    }
                }
                return out == in + 3 + 3 + 1;
            }

            static_assert(check_alpn());
        }  // namespace test
    }      // namespace fnet::tls
}  // namespace utils
