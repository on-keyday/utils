/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../tls/tls.h"
#include "../../storage.h"
#include "keys.h"

namespace futils {
    namespace fnet::quic::crypto {
        constexpr auto err_not_installed = error::Error("secret is not installed", error::Category::lib, error::fnet_quic_crypto_op_error);

        struct Secret {
           private:
            storage secret_;
            tls::TLSCipher cipher_;
            KeyIV keyiv_;

           public:
            Secret& operator=(Secret&& s) {
                if (this == &s) {
                    return *this;
                }
                secret_ = std::move(s.secret_);
                cipher_ = s.cipher_;
                s.cipher_ = {};
                keyiv_ = s.keyiv_;
                s.keyiv_.clear();
                return *this;
            }

            void install(view::rvec new_secret, const tls::TLSCipher& cipher) {
                discard();
                secret_ = make_storage(new_secret);
                cipher_ = cipher;
            }

            void discard() {
                secret_.force_fill(0);
                secret_.clear();
                keyiv_.clear();
                cipher_ = tls::TLSCipher{};
            }

            constexpr bool is_installed() const {
                return !secret_.null();
            }

            constexpr bool is_used() const {
                return keyiv_.hash_len != 0;
            }

            std::pair<const KeyIV*, error::Error> keyiv(std::uint32_t version) {
                if (!is_installed()) {
                    return {nullptr, err_not_installed};
                }
                if (keyiv_.hash_len == 0) {
                    if (auto err = make_key_and_iv(keyiv_, cipher_, version, secret_)) {
                        return {nullptr, err};
                    }
                }
                return {&keyiv_, error::none};
            }

            error::Error hp(HP& hp_, std::uint32_t version) const {
                if (!is_installed()) {
                    return err_not_installed;
                }
                return make_hp(hp_, cipher_, version, secret_);
            }

            error::Error hp_if_zero(HP& hp_, std::uint32_t version) const {
                if (hp_.hash_len == 0) {
                    return hp(hp_, version);
                }
                return error::none;
            }

            const tls::TLSCipher* cipher() const {
                return &cipher_;
            }

            std::pair<view::rvec, error::Error> update(view::wvec out, std::uint32_t version) const {
                if (!is_installed()) {
                    return {{}, err_not_installed};
                }
                out = out.substr(0, secret_.size());
                auto err = make_updated_key(out, secret_, version);
                if (err) {
                    return {{}, std::move(err)};
                }
                return {out, error::none};
            }

            ~Secret() {
                discard();
            }
        };

    }  // namespace fnet::quic::crypto
}  // namespace futils
