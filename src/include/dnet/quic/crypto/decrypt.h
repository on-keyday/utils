/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// decrypt - decrypt call
#pragma once
#include "crypto.h"
#include "../packet/parse.h"
#include "encdec_suite.h"

namespace utils {
    namespace dnet::quic::crypto {

        // get_crypto_suite is DecryptSuite*(PacketType,Packet)
        // err_cb is bool(PacketType,Packet,CryptoPacket,error::Error,bool is_decrypted)
        // plain_cb is bool(packetnum::Value,PlainPacket,view::wvec raw_packet)
        constexpr auto decrypt_callback(auto&& get_crypto_suite, auto&& plain_cb, auto&& err_cb) {
            return [=](PacketType type, auto&& packet, packet::CryptoPacket parsed) {
                DecryptSuite suite;
                if (auto err = get_crypto_suite(&suite, type, std::as_const(packet))) {
                    return err_cb(type, packet, parsed, err, false);
                }
                auto err = decrypt_packet(*suite.keys, *suite.cipher, parsed, suite.largest_pn);
                if (err) {
                    return err_cb(type, packet, parsed, err, false);
                }
                auto [plain, ok] = packet::convert_cipher_to_plain(parsed, packet, authentication_tag_length);
                if (!ok) {
                    return err_cb(type, packet, parsed, error::Error("failed to parse as plain packet"), true);
                }
                return plain_cb(parsed.packet_number, plain, parsed.src);
            };
        }

        // is_stateless_reset is bool(StatelessReset)
        // get_dstID_len is bool(io::reader,size_t*)
        // crypto_cb is bool(packetnum::Value,DecryptedPacket,view::wvec src)
        // get_decrypt_suite is DecryptSuite*(PacketType,Packet)
        // plain_cb is bool(PacketType,Packet,view::wvec src)
        // crypto_err is bool(PacketType,Packet,CryptoPacket,error::Error,bool is_decrypted)
        // plain_err is bool(PacketType,Packet,view::wvec src,bool err,bool valid_type)
        template <template <class...> class Vec>
        constexpr auto parse_with_decrypt(
            view::wvec src, size_t auth_tag_len, auto&& is_stateless_reset, auto&& get_dstID_len, auto&& crypto_cb,
            auto&& get_decrypt_suite, auto&& plain_cb, auto&& crypto_err, auto&& plain_err) {
            auto decrypt_cb = decrypt_callback(get_decrypt_suite, crypto_cb, crypto_err);
            auto with_crypto = packet::with_crypto_parse(decrypt_cb, plain_cb, plain_err);
            return packet::parse_packets<Vec>(src, auth_tag_len, with_crypto, is_stateless_reset, get_dstID_len);
        }

    }  // namespace  dnet::quic::crypto
}  // namespace utils
