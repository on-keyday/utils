/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto - QUIC crypto
#pragma once
#include "../../dll/dllh.h"
#include "keys.h"
#include "../../error.h"
#include "../packet/crypto.h"
#include "crypto_tag.h"
#include "tls_suite.h"

namespace utils {
    namespace fnet {
        namespace tls {
            // forward cipher
            struct TLSCipher;
        }  // namespace tls

        namespace quic::crypto {

            fnet_dll_export(bool) has_quic_ext();

            fnet_dll_export(bool) HKDF_Extract(view::wvec extracted, view::rvec secret, view::rvec salt);
            fnet_dll_export(bool) HKDF_Expand_label(view::wvec tmp, view::wvec output, view::rvec secret, view::rvec quic_label);

            // encrypt_packet encrypts packet (both header and payload) with specific cipher
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            fnet_dll_export(error::Error) encrypt_packet(const KeyIV& keyiv, const HP& hp, const tls::TLSCipher& cipher, packet::CryptoPacket& packet);

            // decrypt_header decrypts packet header with specific cipher
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            // to decrypt payload, call decrypt_payload after call this
            fnet_dll_export(error::Error) decrypt_header(const HP& hp, const tls::TLSCipher& cipher, packet::CryptoPacket& packet, packetnum::Value largest_pn);

            // decrypt_payload decrypts packet payload with specific cipher
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            // packet MUST be parsed by decrypt_header
            fnet_dll_export(error::Error) decrypt_payload(const KeyIV& keyiv, const tls::TLSCipher& cipher, packet::CryptoPacket& packet);

            // cipher_payload encrypts/decrypts payload with specific cipher
            // info should be parsed by CryptoPacket.parse_pnknown()
            fnet_dll_export(error::Error) cipher_payload(const tls::TLSCipher& cipher, packet::CryptoPacketPnKnown& info, view::rvec key, view::rvec iv_nonce, bool enc);

            fnet_dll_export(error::Error) generate_retry_integrity_tag(view::wvec tag, view::rvec pseduo_packet, std::uint32_t version);

        }  // namespace quic::crypto
    }      // namespace fnet
}  // namespace utils
