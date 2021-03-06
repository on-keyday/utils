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
#include "../transport_param/tpparam.h"

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

                constexpr tsize size() const {
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
                libload_failed,    // failed to load external library
                memory_exhausted,  // memory exhausted
                internal,          // external library error
                invalid_argument,  // invalid argument
            };

            enum EncryptionLevel {
                initial,
                handshake,
                zero_rtt,
                application,
            };
            Dll(Error) set_alpn(conn::Connection* c, const char* alpn, uint len);
            Dll(Error) set_hostname(conn::Connection* c, const char* host);
            Dll(Error) set_peer_cert(conn::Connection* c, const char* cert);
            Dll(Error) set_self_cert(conn::Connection* c, const char* cert, const char* private_key);
            Dll(Error) set_transport_parameter(conn::Connection* c, const tpparam::DefinedParams* param, bool force_all = false, const tpparam::TransportParam* custom = nullptr, tsize custom_len = 0);
            Dll(Error) start_handshake(conn::Connection* c, mem::CBN<void, const byte*, tsize, bool /*err*/> send_data);

            Dll(Error) encrypt_packet_protection(Mode mode, pool::BytesPool& conn, packet::Packet* packet);
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
