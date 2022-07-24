/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto - crypto data
#pragma once
#include "../doc.h"
#include "../mem/pool.h"
#include "../mem/vec.h"
#include "../frame/types.h"
#include "../conn/conn.h"

namespace utils {
    namespace quic {
        namespace conn {
            struct Connection;
        }

        namespace packet {
            struct Packet;
        }

        namespace crypto {

            template <tsize size_>
            struct Key {
                byte key[size_];

                constexpr tsize size() {
                    return size_;
                }
            };

            struct InitialKeys {
                Key<32> initial;
                Key<16> key;
                Key<12> iv;
                Key<16> hp;
            };

            constexpr Key<8> quic_key{'q', 'u', 'i', 'c', ' ', 'k', 'e', 'y'};
            constexpr Key<7> quic_iv{'q', 'u', 'i', 'c', ' ', 'i', 'v'};
            constexpr Key<7> quic_hp{'q', 'u', 'i', 'c', ' ', 'h', 'p'};
            constexpr Key<9> server_in{'s', 'e', 'r', 'v', 'e', 'r', ' ', 'i', 'n'};
            constexpr Key<9> client_in{'c', 'l', 'i', 'e', 'n', 't', ' ', 'i', 'n'};

            constexpr Key<20> quic_v1_initial_salt = {0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34,
                                                      0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8,
                                                      0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a};
            bool make_initial_keys(InitialKeys& key,
                                   const byte* client_conn_id, tsize len, Mode mode);

            enum class Error {
                none,
                libload_failed,
                memory_exhausted,
                internal,
            };

            enum EncryptionLevel {
                initial,
                handshake,
                zero_rtt,
                application,
            };

            Dll(Error) decrypt_packet_protection(Mode mode, pool::BytesPool& conn, packet::Packet* ppacket);
            Dll(Error) advance_handshake(frame::Crypto& cframe, conn::Conn& con, EncryptionLevel level);
            Dll(Error) advance_handshake_vec(bytes::Buffer& buf, mem::Vec<frame::Crypto>& cframe, conn::Conn& con, EncryptionLevel level);
        }  // namespace crypto

        namespace external {
            Dll(bool) load_LibCrypto();
            Dll(bool) load_LibSSL();
            Dll(void) set_libcrypto_location(const char* location);
            Dll(void) set_libssl_location(const char* location);
        }  // namespace external
    }      // namespace quic
}  // namespace utils
