/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../dll/dllh.h"
#include "../../../view/iovec.h"
#include <cstddef>
#include "../types.h"
#include "../../error.h"

namespace utils {
    namespace dnet {
        struct TLSCipher;
        namespace quic::crypto {

            template <size_t size_>
            struct Key {
                byte key[size_];

                constexpr size_t size() const {
                    return size_;
                }
            };

            template <size_t size_>
            constexpr Key<size_ - 1> make_key(const char (&data)[size_]) {
                Key<size_ - 1> key;
                for (auto i = 0; i < size_ - 1; i++) {
                    key.key[i] = data[i];
                }
                return key;
            }

            template <size_t size_>
            constexpr auto make_key_from_bintext(const char (&data)[size_]) {
                static_assert((size_ - 1) % 2 == 0, "must be multiple of 2");
                auto hex_to_bin = [](auto c) -> byte {
                    if ('0' <= c && c <= '9') {
                        return c - '0';
                    }
                    if ('a' <= c && c <= 'f') {
                        return c - 'a' + 0xa;
                    }
                    if ('A' <= c && c <= 'F') {
                        return c - 'A' + 0xa;
                    }
                    // undefined behaviour
                    int* ptr = 0;
                    *ptr = 0;
                    return 0xff;
                };
                auto make_byte = [&](char a, char b) {
                    return hex_to_bin(a) << 4 | hex_to_bin(b);
                };
                Key<(size_ - 1) / 2> key;
                for (auto i = 0; i < (size_ - 1) / 2; i++) {
                    key.key[i] = make_byte(data[i * 2], data[i * 2 + 1]);
                }
                return key;
            }

            struct Keys {
               private:
                byte resource[32 + 12 + 32];

                view::rvec res() const {
                    return resource;
                }

                view::wvec res() {
                    return resource;
                }

               public:
                byte hash_len = 0;

                void clear() {
                    res().force_fill(0);
                }

                constexpr view::wvec key() {
                    return res().substr(0, hash_len);
                }

                constexpr view::wvec iv() {
                    return res().substr(hash_len, 12);
                }

                constexpr view::wvec hp() {
                    return res().substr(hash_len + 12, hash_len);
                }

                constexpr view::rvec key() const {
                    return res().substr(0, hash_len);
                }

                constexpr view::rvec iv() const {
                    return res().substr(hash_len, 12);
                }

                constexpr view::rvec hp() const {
                    return res().substr(hash_len + 12, hash_len);
                }
            };

            constexpr auto quic_key = make_key("quic key");
            constexpr auto quic_iv = make_key("quic iv");
            constexpr auto quic_hp = make_key("quic hp");
            constexpr auto server_in = make_key("server in");
            constexpr auto client_in = make_key("client in");
            constexpr auto quic_ku = make_key("quic ku");

            constexpr auto quic_v1_initial_salt = make_key_from_bintext("38762cf7f55934b34d179ae6a4c80cadccbb7f0a");

            // for explaining
            constexpr auto quic_v1_retry_integrity_tag_secret = make_key_from_bintext("d9c9943e6101fd200021506bcc02814c73030f25c79d71ce876eca876e6fca8e");

            constexpr auto quic_v1_retry_integrity_tag_key = make_key_from_bintext("be0c690b9f66575a1d766b54e368c84e");

            constexpr auto quic_v1_retry_integrity_tag_nonce = make_key_from_bintext("461599d35d632bf2239825bb");

            // make_initial_secret generates initial secret from clientOrigDstID
            // WARNING(on-keyday): clientOrigDstID must be dstID field value of client sent at first packet
            dnet_dll_export(bool) make_initial_secret(view::wvec secret, std::uint32_t version, view::rvec clientConnID, bool enc_client);

            dnet_dll_export(error::Error) make_keys_from_secret(Keys& keys, const TLSCipher& cipher, std::uint32_t version, view::rvec secret);

            dnet_dll_export(error::Error) make_updated_key(view::wvec new_secret, view::rvec secret, std::uint64_t version);

        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils