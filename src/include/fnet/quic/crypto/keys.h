/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../dll/dllh.h"
#include "../../../view/iovec.h"
#include <cstddef>
#include "../types.h"
#include "../../error.h"

namespace futils {
    namespace fnet {
        namespace tls {
            struct TLSCipher;
        }

        namespace quic::crypto {

            template <size_t size_>
            struct KeyMaterial {
                byte material[size_];

                constexpr size_t size() const {
                    return size_;
                }
            };

            template <size_t size_>
            constexpr KeyMaterial<size_ - 1> make_key_material(const char (&data)[size_]) {
                KeyMaterial<size_ - 1> key;
                for (auto i = 0; i < size_ - 1; i++) {
                    key.material[i] = data[i];
                }
                return key;
            }

            template <size_t size_>
            constexpr auto make_material_from_bintext(const char (&data)[size_]) {
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
                KeyMaterial<(size_ - 1) / 2> key;
                for (auto i = 0; i < (size_ - 1) / 2; i++) {
                    key.material[i] = make_byte(data[i * 2], data[i * 2 + 1]);
                }
                return key;
            }

            struct KeyIV {
               private:
                byte resource[32 + 12];
                view::rvec res() const {
                    return resource;
                }

                view::wvec res() {
                    return resource;
                }

               public:
                byte hash_len = 0;

                constexpr view::wvec key() {
                    return res().substr(0, hash_len);
                }

                constexpr view::wvec iv() {
                    return res().substr(hash_len, 12);
                }

                constexpr view::rvec key() const {
                    return res().substr(0, hash_len);
                }

                constexpr view::rvec iv() const {
                    return res().substr(hash_len, 12);
                }

                constexpr void clear() {
                    res().force_fill(0);
                    hash_len = 0;
                }
            };

            // Header Protection Key
            struct HP {
               private:
                byte resource[32];

               public:
                byte hash_len = 0;

                constexpr view::rvec hp() const {
                    return view::rvec(resource).substr(0, hash_len);
                }

                constexpr view::wvec hp() {
                    return view::wvec(resource).substr(0, hash_len);
                }

                constexpr void clear() {
                    view::wvec(resource).force_fill(0);
                    hash_len = 0;
                }
            };

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

                constexpr void clear() {
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

            constexpr auto quic_key = make_key_material("quic key");
            constexpr auto quic_iv = make_key_material("quic iv");
            constexpr auto quic_hp = make_key_material("quic hp");
            constexpr auto server_in = make_key_material("server in");
            constexpr auto client_in = make_key_material("client in");
            constexpr auto quic_ku = make_key_material("quic ku");

            constexpr auto quic_v1_initial_salt = make_material_from_bintext("38762cf7f55934b34d179ae6a4c80cadccbb7f0a");

            // for explaining
            constexpr auto quic_v1_retry_integrity_tag_secret = make_material_from_bintext("d9c9943e6101fd200021506bcc02814c73030f25c79d71ce876eca876e6fca8e");

            constexpr auto quic_v1_retry_integrity_tag_key = make_material_from_bintext("be0c690b9f66575a1d766b54e368c84e");

            constexpr auto quic_v1_retry_integrity_tag_nonce = make_material_from_bintext("461599d35d632bf2239825bb");

            // make_initial_secret generates initial secret from clientOrigDstID
            // WARNING(on-keyday): clientOrigDstID must be dstID field value of client sent at first packet
            fnet_dll_export(bool) make_initial_secret(view::wvec secret, std::uint32_t version, view::rvec clientConnID, bool enc_client);

            // fnet_dll_export(error::Error) make_keys_from_secret(Keys& keys, const fnet::tls::TLSCipher& cipher, std::uint32_t version, view::rvec secret);

            fnet_dll_export(error::Error) make_key_and_iv(KeyIV& keys, const fnet::tls::TLSCipher& cipher, std::uint32_t version, view::rvec secret);

            fnet_dll_export(error::Error) make_hp(HP& hp, const fnet::tls::TLSCipher& cipher, std::uint32_t version, view::rvec secret);

            fnet_dll_export(error::Error) make_updated_key(view::wvec new_secret, view::rvec secret, std::uint64_t version);

        }  // namespace quic::crypto
    }      // namespace fnet
}  // namespace futils