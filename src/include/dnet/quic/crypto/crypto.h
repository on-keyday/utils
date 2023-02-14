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
    namespace dnet {
        namespace tls {
            // forward cipher
            struct TLSCipher;
        }  // namespace tls

        namespace quic::crypto {

            dnet_dll_export(bool) has_quic_ext();

            dnet_dll_export(bool) HKDF_Extract(view::wvec extracted, view::rvec secret, view::rvec salt);
            dnet_dll_export(bool) HKDF_Expand_label(view::wvec tmp, view::wvec output, view::rvec secret, view::rvec quic_label);

            // encrypt_packet encrypts packet with specific cipher
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            // key shpuld
            dnet_dll_export(error::Error) encrypt_packet(const Keys& keys, const tls::TLSCipher& cipher, packet::CryptoPacket& packet);

            // decrypt_packet decrypts packet with specific cipher
            // cipher is got from TLS.get_cipher or is TLSCipher{}
            dnet_dll_export(error::Error) decrypt_packet(const Keys& keys, const tls::TLSCipher& cipher, packet::CryptoPacket& packet, size_t largest_pn);

            // cipher_payload encrypts/decrypts payload with specific cipher
            // info should be parsed by CryptoPacket.parse_pnknown()
            dnet_dll_export(error::Error) cipher_payload(const tls::TLSCipher& cipher, packet::CryptoPacketPnKnown& info, view::rvec key, view::rvec iv_nonce, bool enc);

            dnet_dll_export(error::Error) generate_retry_integrity_tag(view::wvec tag, view::rvec pseduo_packet, std::uint32_t version);

        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
