/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../storage.h"

namespace utils {
    namespace fnet::quic::token {

        struct RetryToken {
           private:
            storage token;

           public:
            void on_retry_received(view::rvec retry_token) {
                token = make_storage(retry_token);
            }

            constexpr view::rvec get_token() const {
                return token;
            }

            void reset() {
                token = {};
            }
        };

        struct Token {
            view::rvec token;
        };

        struct TokenStorage {
            storage token;
        };

        struct ZeroRTTStorage {
            std::shared_ptr<void> obj;
            TokenStorage (*find_token)(void*, view::rvec key) = nullptr;
            void (*store_token)(void*, view::rvec key, Token token)=nullptr;

            void store(view::rvec key, Token token) const {
                if (store_token != nullptr) {
                    store_token(obj.get(), key, token);
                }
            }

            TokenStorage find(view::rvec key) const {
                if (find_token == nullptr) {
                    return {};
                }
                return find_token(obj.get(), key);
            }
        };

        struct ZeroRTTTokenManager {
           private:
            ZeroRTTStorage importer;
            storage key;
            storage found_cache;

           public:
            void reset(ZeroRTTStorage&& in) {
                importer = std::move(in);
                found_cache = {};
            }

            void store(Token token) const {
                importer.store(key, token);
            }

            Token find() {
                if (!found_cache.null()) {
                    return Token{.token = found_cache};
                }
                auto s = importer.find(key);
                if (s.token.null()) {
                    return {};
                }
                found_cache = std::move(s.token);
                return Token{.token = found_cache};
            }
        };
    }  // namespace fnet::quic::token
}  // namespace utils
