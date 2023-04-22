/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../../view/iovec.h"
#include <memory>

namespace utils {
    namespace fnet::quic::connid {
        struct Exporter {
            std::shared_ptr<void> arg;
            void (*add_connID)(void*, view::rvec id)=nullptr;
            void (*retire_connID)(void*, view::rvec id)=nullptr;

            void add(view::rvec id) {
                if (add_connID != nullptr) {
                    add_connID(arg.get(), id);
                }
            }

            void retire(view::rvec id) {
                if (retire_connID != nullptr) {
                    retire_connID(arg.get(), id);
                }
            }
        };
    }  // namespace fnet::quic::connid
}  // namespace utils
