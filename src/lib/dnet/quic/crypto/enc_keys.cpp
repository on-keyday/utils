/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/ssldll.h>
#include <dnet/tls.h>
#include <dnet/quic/crypto/crypto.h>
#include <helper/defer.h>
#include <dnet/quic/version.h>
#include <dnet/quic/crypto/internal.h>

namespace utils {
    namespace dnet {
        namespace quic::crypto {

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

            bool HKDF_Extract_openssl(view::wvec initial,
                                      view::rvec secret, view::rvec salt) {
                auto& c = ssldl;
                auto kdf = c.EVP_KDF_fetch_(nullptr, "HKDF", nullptr);
                auto ctx = c.EVP_KDF_CTX_new_(kdf);
                auto _ = helper::defer([&] {
                    c.EVP_KDF_CTX_free_(ctx);
                });
                c.EVP_KDF_free_(kdf);
                ssl_import::open_quic_ext::OSSL_PARAM params[5];
                params[0] = c.OSSL_PARAM_construct_utf8_string_("mode", const_cast<char*>("EXTRACT_ONLY"), 12);
                params[1] = c.OSSL_PARAM_construct_octet_string_("key", const_cast<byte*>(secret.data()), secret.size());
                params[2] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                if (!salt.null()) {
                    params[3] = c.OSSL_PARAM_construct_octet_string_("salt", const_cast<byte*>(salt.data()), salt.size());
                    params[4] = c.OSSL_PARAM_construct_end_();
                }
                else {
                    params[3] = c.OSSL_PARAM_construct_end_();
                }
                auto v = c.EVP_KDF_derive_(ctx, initial.data(), initial.size(), params);
                if (v <= 0) {
                    return false;
                }
                return true;
            }

            dnet_dll_implement(bool) HKDF_Extract(view::wvec extracted,
                                                  view::rvec secret, view::rvec salt) {
                if (!loading()) {
                    return false;
                }
                if (!ssldl.HKDF_extract_) {
                    return HKDF_Extract_openssl(extracted, secret, salt);
                }
                size_t outlen = extracted.size();
                return (bool)ssldl.HKDF_extract_(
                    extracted.data(), &outlen,
                    ssldl.EVP_sha256_(), secret.data(), secret.size(),
                    salt.data(), salt.size());
            }

            bool HKDF_Expand_openssl(view::wvec out, view::rvec secret, view::rvec label) {
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
                params[1] = c.OSSL_PARAM_construct_octet_string_("key", const_cast<byte*>(secret.data()), secret.size());
                params[2] = c.OSSL_PARAM_construct_octet_string_("info", const_cast<byte*>(label.data()), label.size());
                params[3] = c.OSSL_PARAM_construct_utf8_string_("digest", const_cast<char*>(digest_SHA256), 6);
                params[4] = c.OSSL_PARAM_construct_end_();
                auto err = c.EVP_KDF_derive_(ctx, out.data(), out.size(), params);
                if (err <= 0) {
                    return false;
                }
                return true;
            }

            dnet_dll_implement(bool) HKDF_Expand_label(view::wvec tmp, view::wvec output, view::rvec secret, view::rvec quic_label) {
                if (6 + quic_label.size() > 0xff) {
                    return false;
                }
                if (output.size() > 0xffff) {
                    return false;
                }
                io::writer w{tmp};
                byte label_len = 6 + quic_label.size();
                // refer RFC8446 section 3.4
                // A variable vector length has a vector length as a prefix of vector.
                // strlen("tls13 ")
                // WARN(on-keyday): DON'T forget " " after "tls13"
                // output.size() + (6 + quic_label.len) + "tls13 " + quic_label +
                // (hash_value.len==0) + (hash_value=="")
                bool ok = io::write_num(w, std::uint16_t(output.size())) &&
                          w.write(label_len, 1) &&
                          w.write("tls13 ") &&
                          w.write(quic_label) &&
                          w.write(0, 1);
                if (!ok) {
                    return false;
                }
                auto label = w.written();
                if (!loading()) {
                    return false;
                }
                if (!ssldl.HKDF_expand_) {
                    return HKDF_Expand_openssl(output, secret, label);
                }
                return ssldl.HKDF_expand_(output.data(), output.size(), ssldl.EVP_sha256_(),
                                          secret.data(), secret.size(), label.data(), label.size());
            }

            dnet_dll_implement(bool) make_initial_secret(view::wvec secret, std::uint32_t version, view::rvec clientConnID, bool enc_client) {
                byte extract_buf[32]{};
                auto extract = view::wvec(extract_buf);
                bool ok = false;
                if (secret.size() != 32) {
                    return false;
                }
                if (version == version_1) {
                    // see RFC 9001 Section 5.2
                    // https://tex2e.github.io/rfc-translater/html/rfc9001.html
                    ok = HKDF_Extract(extract, clientConnID, quic_v1_initial_salt.key);
                }
                if (!ok) {
                    return false;
                }
                byte src[100];
                if (enc_client) {
                    ok = HKDF_Expand_label(src, secret, extract, client_in.key);
                }
                else {
                    ok = HKDF_Expand_label(src, secret, extract, server_in.key);
                }
                return ok;
            }

            dnet_dll_export(error::Error) make_keys_from_secret(Keys& keys, const TLSCipher& cipher, std::uint32_t version, view::rvec secret) {
                auto suite = judge_cipher(cipher);
                if (suite == tls::TLSSuite::Unsupported) {
                    return error::Error("cipher spec is not AES based or CHACHA20 based", error::ErrorCategory::validationerr);
                }
                auto hash_len = tls::hash_len(suite);
                if (hash_len != 16 && hash_len != 32) {
                    return error::Error("key: invalid hash length. expect 16 or 32.", error::ErrorCategory::validationerr);
                }
                keys.hash_len = hash_len;
                byte tmp[100];
                if (version == version_1) {
                    auto ok = HKDF_Expand_label(tmp, keys.key(), secret, quic_key.key) &&
                              HKDF_Expand_label(tmp, keys.iv(), secret, quic_iv.key) &&
                              HKDF_Expand_label(tmp, keys.hp(), secret, quic_hp.key);
                    if (!ok) {
                        return error::Error("key: failed to generate encryption key");
                    }
                    return error::none;
                }
                return error::Error("key: unknown QUIC version", error::ErrorCategory::quicerr);
            }

            dnet_dll_implement(error::Error) make_updated_key(view::wvec new_secret, view::rvec secret, std::uint64_t version) {
                if (new_secret.size() != secret.size()) {
                    return error::Error("new_secret size is not same as old secret size");
                }
                if (version == version_1) {
                    byte tmp[100];
                    if (!HKDF_Expand_label(tmp, new_secret, secret, quic_ku.key)) {
                        return error::Error("key: failed to generate encryption key");
                    }
                    return error::none;
                }
                return error::Error("key: unknown QUIC version", error::ErrorCategory::quicerr);
            }

        }  // namespace quic::crypto
    }      // namespace dnet
}  // namespace utils
