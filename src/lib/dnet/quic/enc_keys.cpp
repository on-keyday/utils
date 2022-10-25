/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/ssldll.h>
#include <dnet/tls.h>
#include <dnet/quic/crypto.h>
#include <helper/defer.h>

namespace utils {
    namespace dnet {
        namespace quic {

            bool load_openssl_quic() {
                static auto res = load_crypto() && ssldl.load_openssl_quic_ext();
                return res;
            }

            constexpr auto digest_SHA256 = "SHA256";

            bool loading() {
                if (!load_crypto()) {
                    return false;
                }
                // loading
                load_openssl_quic();
                is_boringssl_crypto();
                return true;
            }

            bool HKDF_Extract_openssl(ByteLen& initial,
                                      ByteLen secret, ByteLen salt) {
                auto& c = ssldl;
                auto kdf = c.EVP_KDF_fetch_(nullptr, "HKDF", nullptr);
                auto ctx = c.EVP_KDF_CTX_new_(kdf);
                auto _ = helper::defer([&] {
                    c.EVP_KDF_CTX_free_(ctx);
                });
                c.EVP_KDF_free_(kdf);
                ssl_import::open_quic_ext::OSSL_PARAM params[5];
                params[0] = c.OSSL_PARAM_construct_utf8_string_("mode", const_cast<char*>("EXTRACT_ONLY"), 12);
                params[1] = c.OSSL_PARAM_construct_octet_string_("key", secret.data, secret.len);
                params[2] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                if (salt.valid()) {
                    params[3] = c.OSSL_PARAM_construct_octet_string_("salt", salt.data, salt.len);
                    params[4] = c.OSSL_PARAM_construct_end_();
                }
                else {
                    params[3] = c.OSSL_PARAM_construct_end_();
                }
                auto v = c.EVP_KDF_derive_(ctx, initial.data, initial.len, params);
                if (v <= 0) {
                    return false;
                }
                return true;
            }

            dnet_dll_implement(bool) HKDF_Extract(ByteLen& initial,
                                                  ByteLen secret, ByteLen salt) {
                if (!loading()) {
                    return false;
                }
                if (!ssldl.HKDF_extract_) {
                    return HKDF_Extract_openssl(initial, secret, salt);
                }
                return (bool)ssldl.HKDF_extract_(
                    initial.data, &initial.len,
                    ssldl.EVP_sha256_(), secret.data, secret.len,
                    salt.data, salt.len);
            }

            bool HKDF_Expand_openssl(ByteLen& out, ByteLen secret, ByteLen label) {
                auto& c = ssldl;
                auto kdf = c.EVP_KDF_fetch_(nullptr, "HKDF", nullptr);
                auto ctx = c.EVP_KDF_CTX_new_(kdf);
                auto _ = helper::defer([&] {
                    c.EVP_KDF_CTX_free_(ctx);
                });
                c.EVP_KDF_free_(kdf);
                ssl_import::open_quic_ext::OSSL_PARAM params[5];
                static_assert(sizeof(params[0]) == 40);
                params[0] = c.OSSL_PARAM_construct_utf8_string_("mode", const_cast<char*>("EXPAND_ONLY"), 11);
                params[1] = c.OSSL_PARAM_construct_octet_string_("key", secret.data, secret.len);
                params[2] = c.OSSL_PARAM_construct_octet_string_("info", label.data, label.len);
                params[3] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                params[4] = c.OSSL_PARAM_construct_end_();
                auto err = c.EVP_KDF_derive_(ctx, out.data, out.len, params);
                if (err <= 0) {
                    return false;
                }
                return true;
            }

            dnet_dll_implement(bool) HKDF_Expand_label(WPacket& w, ByteLen output, ByteLen secret, ByteLen quic_label) {
                if (6 + quic_label.len > 0xff) {
                    return false;
                }
                byte label_len = 6 + quic_label.len;
                byte tmpdat[2];
                WPacket tmp{ByteLen{tmpdat, 2}};
                auto outlen = tmp.as<std::uint16_t>(output.len);
                // refer RFC8446 section 3.4
                // A variable vector length has a vector length as a prefix of vector.
                // strlen("tls13 ")
                // WARN(on-keyday): DON'T forget " " after "tls13"
                // output.len + (6 + quic_label.len) + "tls13 " + quic_label.data +
                // (hash_value.len==0) + (hash_value=="")
                tmp = {w.zeromem(2 + 1 + label_len + 1), 0};
                tmp.append(outlen.data, outlen.len);
                tmp.add(label_len, 1);
                tmp.append("tls13 ", 6);
                tmp.append(quic_label.data, quic_label.len);
                tmp.add(0, 1);
                auto label = tmp.b;
                if (tmp.overflow) {
                    return false;
                }
                if (!loading()) {
                    return false;
                }
                if (!ssldl.HKDF_expand_) {
                    return HKDF_Expand_openssl(output, secret, label);
                }
                return ssldl.HKDF_expand_(output.data, output.len, ssldl.EVP_sha256_(),
                                          secret.data, secret.len, label.data, label.len);
            }

            dnet_dll_implement(bool) make_initial_keys(InitialKeys& key, WPacket& src, ByteLen clientConnID, bool enc_client) {
                byte extract_buf[32]{};
                auto extract = ByteLen{extract_buf, 32};
                auto inisalt = quic_v1_initial_salt;
                // see RFC 9001 Section 5.2
                // https://tex2e.github.io/rfc-translater/html/rfc9001.html
                auto ok = HKDF_Extract(extract, clientConnID, {inisalt.key, inisalt.size()});
                if (!ok) {
                    return false;
                }
                auto key_to_blen = [](auto& key) {
                    return ByteLen{key.key, key.size()};
                };
                auto initial = key_to_blen(key.initial);
                if (enc_client) {
                    auto cl_in = client_in;
                    ok = HKDF_Expand_label(src, initial, extract, key_to_blen(cl_in));
                }
                else {
                    auto sv_in = server_in;
                    ok = HKDF_Expand_label(src, initial, extract, key_to_blen(sv_in));
                }
                auto qkey = quic_key;
                auto qiv = quic_iv;
                auto qhp = quic_hp;
                ok = ok &&
                     HKDF_Expand_label(src, key_to_blen(key.key), initial, key_to_blen(qkey)) &&
                     HKDF_Expand_label(src, key_to_blen(key.iv), initial, key_to_blen(qiv)) &&
                     HKDF_Expand_label(src, key_to_blen(key.hp), initial, key_to_blen(qhp));
                return ok;
            }

            dnet_dll_implement(bool) make_keys_from_secret(Keys& keys, WPacket& src, ByteLen secret, int hash_len) {
                auto key_to_blen = [](auto& key) {
                    return ByteLen{key.key, key.size()};
                };
                WPacket res{{keys.resource, sizeof(keys.resource)}};
                if (hash_len == 32) {
                    keys.key = res.zeromem(32);
                    keys.iv = res.zeromem(12);
                    keys.hp = res.zeromem(32);
                    keys.ku = res.zeromem(32);
                }
                else if (hash_len == 16) {
                    keys.key = res.zeromem(16);
                    keys.iv = res.zeromem(12);
                    keys.hp = res.zeromem(16);
                    keys.ku = res.zeromem(16);
                }
                else {
                    return false;
                }
                auto qkey = quic_key;
                auto qiv = quic_iv;
                auto qhp = quic_hp;
                auto qku = quic_ku;
                auto ok = HKDF_Expand_label(src, keys.key, secret, key_to_blen(qkey)) &&
                          HKDF_Expand_label(src, keys.iv, secret, key_to_blen(qiv)) &&
                          HKDF_Expand_label(src, keys.hp, secret, key_to_blen(qhp)) &&
                          HKDF_Expand_label(src, keys.ku, secret, key_to_blen(qku));
                return ok;
            }

            /*
            bool HKDF_Extract_openssl(byte* initial, size_t& inilen,
                                      const byte* clientConnID, size_t len) {
                auto& c = ssldl;
                auto kdf = c.EVP_KDF_fetch_(nullptr, "HKDF", nullptr);
                auto ctx = c.EVP_KDF_CTX_new_(kdf);
                auto _ = helper::defer([&] {
                    c.EVP_KDF_CTX_free_(ctx);
                });
                c.EVP_KDF_free_(kdf);
                ssl_import::open_quic_ext::OSSL_PARAM params[5];
                auto salt = quic_v1_initial_salt;
                params[0] = c.OSSL_PARAM_construct_utf8_string_("mode", const_cast<char*>("EXTRACT_ONLY"), 12);
                params[1] = c.OSSL_PARAM_construct_octet_string_("salt", salt.key, sizeof(salt));
                params[2] = c.OSSL_PARAM_construct_octet_string_("key", const_cast<byte*>(clientConnID), len);
                params[3] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                params[4] = c.OSSL_PARAM_construct_end_();
                auto v = c.EVP_KDF_derive_(ctx, initial, inilen, params);

                if (v <= 0) {
                    return false;
                }
                return true;
            }

            bool HKDF_Extract(byte* initial, size_t& inilen,
                              const byte* clientConnID, size_t len) {
                auto& c = ssldl;
                if (!c.HKDF_extract_) {
                    return HKDF_Extract_openssl(initial, inilen, clientConnID, len);
                }
                return (bool)c.HKDF_extract_(initial, &inilen, c.EVP_sha256_(), clientConnID, len, quic_v1_initial_salt.key, quic_v1_initial_salt.size());
            }

            template <size_t outlen, size_t lablen>
            bool HKDF_Expand_openssl(Key<outlen>& out, const byte* secret, size_t secret_len, byte (&label)[lablen]) {
                auto& c = ssldl;
                auto kdf = c.EVP_KDF_fetch_(nullptr, "HKDF", nullptr);
                auto ctx = c.EVP_KDF_CTX_new_(kdf);
                auto _ = helper::defer([&] {
                    c.EVP_KDF_CTX_free_(ctx);
                });
                c.EVP_KDF_free_(kdf);
                ssl_import::open_quic_ext::OSSL_PARAM params[5];
                static_assert(sizeof(params[0]) == 40);
                params[0] = c.OSSL_PARAM_construct_utf8_string_("mode", const_cast<char*>("EXPAND_ONLY"), 11);
                params[1] = c.OSSL_PARAM_construct_octet_string_("key", const_cast<byte*>(secret), secret_len);
                params[2] = c.OSSL_PARAM_construct_octet_string_("info", label, lablen);
                params[3] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                params[4] = c.OSSL_PARAM_construct_end_();
                auto err = c.EVP_KDF_derive_(ctx, out.key, outlen, params);
                if (err <= 0) {
                    return false;
                }
                return true;
            }

            template <size_t outlen, size_t lablen>
            bool HKDF_Expand_label(Key<outlen>& out, const byte* secret,
                                   size_t secret_len, Key<lablen> labelk) {
                std::uint16_t value = outlen;
                byte swp[] = {byte((value >> 8) & 0xff),
                              byte(value & 0xff)};
                // refer RFC8446 section 3.4
                // A variable vector length has a vector length as a prefix of vector.

                // strlen("tls13 ")
                // WARN(on-keyday): DON'T forget " " after "tls13"
                constexpr auto tls13_len = 6;
                constexpr auto tlslabellen = tls13_len + lablen;
                // target_len + label_len + "tls13 " + klabel + hash_value_len + hash_value
                byte label[2 + 1 + tlslabellen + 1 + 0]{0, 0, tlslabellen, 't', 'l', 's', '1', '3', ' '};
                label[0] = swp[0];
                label[1] = swp[1];
                constexpr auto offset = 2 + 1 + tls13_len;
                for (size_t i = offset; i < sizeof(label) - 1; i++) {
                    label[i] = labelk.key[i - offset];
                }
                label[sizeof(label) - 1] = 0;
                auto& c = ssldl;
                if (!c.HKDF_expand_) {
                    return HKDF_Expand_openssl(out, secret, secret_len, label);
                }
                return (bool)c.HKDF_expand_(out.key, out.size(), c.EVP_sha256_(), secret, secret_len, label, sizeof(label));
            }

           dnet_dll_implement(bool) make_initial_keys(InitialKeys& key,
                                                      const byte* client_conn_id, size_t len, bool enc_client) {
               if (!load_crypto()) {
                   return false;
               }
               // loading
               load_openssl_quic();
               is_boringssl_crypto();
               byte initialbuf[32]{};
               size_t initial_len = 32;

               auto ok = HKDF_Extract(initial, initial_len, client_conn_id, len);
               if (!ok) {
                   return false;
               }
               if (enc_client) {
                   ok = HKDF_Expand_label(key.initial, initial, initial_len, client_in);
               }
               else {
                   ok = HKDF_Expand_label(key.initial, initial, initial_len, server_in);
               }
               ok = ok &&
                    HKDF_Expand_label(key.key, key.initial.key, key.initial.size(), quic_key) &&
                    HKDF_Expand_label(key.iv, key.initial.key, key.initial.size(), quic_iv) &&
                    HKDF_Expand_label(key.hp, key.initial.key, key.initial.size(), quic_hp);
               return ok;
           }*/

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
