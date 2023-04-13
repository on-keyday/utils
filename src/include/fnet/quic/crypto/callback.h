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
    namespace fnet::quic::crypto {

        inline int qtls_callback(CryptoSuite* suite, MethodArgs args) {
            auto set_wsecret = [&] {
                return suite->secrets.install_wsecret(args.level, view::rvec(args.write_secret, args.secret_len), args.cipher);
            };
            auto set_rsecret = [&] {
                return suite->secrets.install_rsecret(args.level, view::rvec(args.read_secret, args.secret_len), args.cipher);
            };

            switch (args.type) {
                case ArgType::flush:
                    return 1;
                case ArgType::alert:
                    suite->handshaker.set_alert(args.alert);
                    return 1;
                case ArgType::handshake_data: {
                    return suite->handshaker.add_handshake_data(args.level, view::rvec(args.data, args.len)) ? 1 : 0;
                }
                case ArgType::secret: {
                    return set_rsecret() && set_wsecret()
                               ? 1
                               : 0;
                }
                case ArgType::wsecret: {
                    return set_wsecret() ? 1 : 0;
                }
                case ArgType::rsecret: {
                    return set_rsecret() ? 1 : 0;
                }
                default:
                    return 0;
            }
        }
    }  // namespace fnet::quic::crypto
}  // namespace utils
