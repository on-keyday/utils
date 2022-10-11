/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>

namespace utils {
    namespace dnet {
        namespace crypto {
            using byte = unsigned char;

            struct InitialKeys {
                Key<32> initial;
                Key<16> key;
                Key<12> iv;
                Key<16> hp;
            };

            template <tsize size_>
            struct Key {
                byte key[size_];

                constexpr size_t size() const {
                    return size_;
                }
            };

            constexpr Key<8> quic_key{'q', 'u', 'i', 'c', ' ', 'k', 'e', 'y'};
            constexpr Key<7> quic_iv{'q', 'u', 'i', 'c', ' ', 'i', 'v'};
            constexpr Key<7> quic_hp{'q', 'u', 'i', 'c', ' ', 'h', 'p'};
            constexpr Key<9> server_in{'s', 'e', 'r', 'v', 'e', 'r', ' ', 'i', 'n'};
            constexpr Key<9> client_in{'c', 'l', 'i', 'e', 'n', 't', ' ', 'i', 'n'};

            constexpr Key<20> quic_v1_initial_salt = {0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34,
                                                      0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8,
                                                      0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a};
        }  // namespace crypto
    }      // namespace dnet
}  // namespace utils
