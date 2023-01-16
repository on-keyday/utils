/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../tls.h"
#include "../../storage.h"
#include "keys.h"

namespace utils {
    namespace dnet::quic::crypto {
        constexpr auto err_not_installed = error::Error("NOT_INSTALLED");

        struct Secret {
           private:
            storage secret_;
            TLSCipher cipher_;
            Keys keys_;
            bool exist_key = false;

           public:
            void install(view::rvec new_secret, const TLSCipher& cipher) {
                discard();
                secret_ = make_storage(new_secret);
                cipher_ = cipher;
            }

            void discard() {
                secret_.force_fill(0);
                secret_.clear();
                keys_.clear();
                exist_key = false;
                cipher_ = TLSCipher{};
            }

            constexpr bool is_installed() const {
                return !secret_.null();
            }

            std::pair<const Keys*, error::Error> keys(std::uint32_t version) {
                if (!is_installed()) {
                    return {nullptr, err_not_installed};
                }
                if (!exist_key) {
                    if (auto err = make_keys_from_secret(keys_, cipher_, version, secret_)) {
                        return {nullptr, err};
                    }
                    exist_key = true;
                }
                return {&keys_, error::none};
            }

            const TLSCipher* cipher() const {
                return &cipher_;
            }

            ~Secret() {
                discard();
            }
        };

    }  // namespace dnet::quic::crypto
}  // namespace utils
