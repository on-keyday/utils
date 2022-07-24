/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/crypto/crypto.h>
#include <quic/packet/packet_head.h>
#include <quic/internal/external_func_internal.h>
#include <quic/internal/context_internal.h>
#include <quic/mem/raii.h>
#include <cassert>

namespace utils {
    namespace quic {
        namespace crypto {
            namespace ext = external;

            Error generate_masks(const byte* hp_key, byte* sample, int samplelen, byte* masks) {
                auto& c = ext::libcrypto;
                auto ctx_r = c.EVP_CIPHER_CTX_new_();
                if (!ctx_r) {
                    return Error::memory_exhausted;
                }
                mem::RAII ctx{ctx_r, c.EVP_CIPHER_CTX_free_};
                auto err = c.EVP_CipherInit_(ctx, c.EVP_aes_128_ecb_(), hp_key, nullptr, 1);
                if (err != 1) {
                    return Error::internal;
                }
                byte data[32]{};
                int data_len = 32;
                err = c.EVP_CipherUpdate_(ctx, data, &data_len, sample, samplelen);
                if (err != 1) {
                    return Error::internal;
                }
                bytes::copy(masks, data, 5);
                return Error::none;
            }

            Error decrypt_payload(pool::BytesPool& bpool, InitialKeys& inikey, varint::Swap<uint>& packet_number, byte length, packet::Packet* ppacket) {
                // decrypt payload protection
                constexpr auto iv_len = sizeof(inikey.iv);
                byte nonce[iv_len]{0};
                for (tsize i = 0; i < length; i++) {
                    nonce[iv_len - 1 - i] = packet_number.swp[sizeof(uint) - 1 - i];
                }
                for (tsize i = 0; i < iv_len; i++) {
                    nonce[i] ^= inikey.iv.key[i];
                }
                auto header_length = ppacket->raw_payload - ppacket->raw_header;
                assert(header_length > 0);
                auto& c = ext::libcrypto;
                auto ctx_r = c.EVP_CIPHER_CTX_new_();
                if (!ctx_r) {
                    return Error::memory_exhausted;
                }
                mem::RAII ctx{ctx_r, c.EVP_CIPHER_CTX_free_};
#define ERR_CHECK()             \
    if (err != 1) {             \
        return Error::internal; \
    }
                // set cipher
                auto err = c.EVP_CipherInit_ex_(ctx, c.EVP_aes_128_gcm_(), nullptr, nullptr, nullptr, 0);
                ERR_CHECK()
                // set iv len
                err = c.EVP_CIPHER_CTX_ctrl_(ctx, EVP_CTRL_GCM_SET_IVLEN, inikey.iv.size(), nullptr);
                ERR_CHECK()
                // set key and iv
                err = c.EVP_CipherInit_ex_(ctx, nullptr, nullptr, inikey.key.key, nonce, 0);
                ERR_CHECK()
                int outlen = 0;
                // set packet_header as AAD
                err = c.EVP_CipherUpdate_(ctx, nullptr, &outlen, ppacket->raw_header, header_length);
                ERR_CHECK()
                mem::RAII out{bpool.get(ppacket->payload_length),
                              [&](bytes::Bytes& b) { bpool.put(std::move(b)); }};

                if (!out->own()) {
                    return Error::memory_exhausted;
                }
                EVP_CTRL_GCM_SET_TAG;
                // decrypt plaintext
                err = c.EVP_CipherUpdate_(ctx, out->own(), &outlen, ppacket->raw_payload, ppacket->payload_length);
                ERR_CHECK()
                // TODO(on-keyday):
                // check MAC?
                /*
                auto tag_data = out->own() + outlen - 16;
                err = c.EVP_CIPHER_CTX_ctrl_(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag_data);
                ERR_CHECK()

                err = c.EVP_CipherFinal_(ctx, out->own(), &outlen);
                ERR_CHECK()*/
                ppacket->decrypted_payload = std::move(out());
                ppacket->decrypted_length = outlen - 16;  // remove mac tag length
                return Error::none;
            }

            dll(Error) decrypt_packet_protection(Mode mode, pool::BytesPool& co, packet::Packet* ppacket) {
                if (!ext::load_LibCrypto()) {
                    return Error::libload_failed;
                }
                auto& bpool = co;
                InitialKeys inikey{};
                if (!make_initial_keys(inikey, ppacket->dstID, ppacket->dstID_len, mode)) {
                    return Error::memory_exhausted;
                }
                // TODO(on-keyday): size check and nullptr check
                // get sample
                // enough offset to avoid reading packet number is 4
                auto sample_head = ppacket->raw_payload + 4;
                byte masks[5];
                varint::Swap<uint> packet_number;
                auto err = generate_masks(inikey.hp.key, sample_head, 16, masks);
                if (err != Error::none) {
                    return err;
                }
                ppacket->flags.protect(masks[0]);
                // in order to decrypt payload protection
                ppacket->raw_header[0] = ppacket->flags.raw;
                auto length = ppacket->flags.packet_number_length();
                bytes::copy(packet_number.swp, ppacket->raw_payload, length);
                for (tsize i = sizeof(uint) - length; i < length; i++) {
                    packet_number.swp[i] ^= masks[i + 1];
                }
                ppacket->packet_number = varint::swap_if(packet_number.t);
                // in order to decrypt payload protection
                for (tsize i = 0; i < length; i++) {
                    ppacket->raw_payload[i] = packet_number.swp[i];
                }
                ppacket->raw_payload += length;
                ppacket->payload_length -= length;
                ppacket->remain -= length;
                return decrypt_payload(bpool, inikey, packet_number, length, ppacket);
            }
        }  // namespace crypto
    }      // namespace quic
}  // namespace utils
