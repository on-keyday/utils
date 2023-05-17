/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <memory>

namespace utils {
    namespace fnet::quic::connid {
        // ConnIDExporter exports connection IDs to multiplexer
        struct ConnIDExporter {
            std::weak_ptr<void> arg;
            void (*add_connID)(void*, view::rvec id) = nullptr;
            void (*retire_connID)(void*, view::rvec id) = nullptr;

            void add(view::rvec id, view::rvec statless_reset) {
                if (add_connID != nullptr) {
                    add_connID(arg.lock().get(), id);
                }
            }

            void retire(view::rvec id,view::rvec stateless_reset) {
                if (retire_connID != nullptr) {
                    retire_connID(arg.lock().get(), id);
                }
            }
        };
    }  // namespace fnet::quic::connid
}  // namespace utils
