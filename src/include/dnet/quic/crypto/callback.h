/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// callback - tls callback for quic
#pragma once
#include "suite.h"

namespace utils {
    namespace dnet::quic::crypto {

        inline int qtls_callback(CryptoSuite* suite, MethodArgs args) {
            auto set_wsecret = [&](CryptoData& data) {
                data.wsecret.install(view::rvec(args.write_secret, args.secret_len), args.cipher);
            };
            auto set_rsecret = [&](CryptoData& data) {
                data.rsecret.install(view::rvec(args.read_secret, args.secret_len), args.cipher);
            };
            switch (args.type) {
                case ArgType::flush:
                    return 1;
                case ArgType::alert:
                    suite->alert_code = args.alert;
                    suite->alert = true;
                    return 1;
                case ArgType::handshake_data: {
                    auto& data = suite->crypto_data[level_to_index(args.level)];
                    data.write_data.append(view::rvec(args.data, args.len));
                    return 1;
                }
                case ArgType::secret: {
                    auto& data = suite->crypto_data[level_to_index(args.level)];
                    set_rsecret(data);
                    set_wsecret(data);
                    return 1;
                }
                case ArgType::wsecret: {
                    set_wsecret(suite->crypto_data[level_to_index(args.level)]);
                    return 1;
                }
                case ArgType::rsecret: {
                    set_rsecret(suite->crypto_data[level_to_index(args.level)]);
                    return 1;
                }
                default:
                    return 0;
            }
        }
    }  // namespace dnet::quic::crypto
}  // namespace utils
