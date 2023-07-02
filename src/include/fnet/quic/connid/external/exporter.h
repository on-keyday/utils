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
            // multiplexer pointer
            std::weak_ptr<void> mux;
            // object pointer (may refer self)
            std::weak_ptr<void> obj;
            void (*add_connID)(void*, void*, view::rvec id) = nullptr;
            void (*retire_connID)(void*, void*, view::rvec id) = nullptr;

            constexpr void set_addConnID(void (*add_)(void*, void*, view::rvec id)) {
                add_connID = add_;
            }

            template <class T, class U>
            void set_addConnID(void (*add_)(T*, U*, view::rvec id)) {
                add_connID = reinterpret_cast<decltype(add_connID)>(add_);
            }

            constexpr void set_retireConnID(void (*retire_)(void*, void*, view::rvec id)) {
                retire_connID = retire_;
            }

            template <class T, class U>
            void set_retireConnID(void (*retire_)(T*, U*, view::rvec id)) {
                retire_connID = reinterpret_cast<decltype(retire_connID)>(retire_);
            }

            void add(view::rvec id, view::rvec statless_reset) {
                if (add_connID != nullptr) {
                    add_connID(mux.lock().get(), obj.lock().get(), id);
                }
            }

            void retire(view::rvec id, view::rvec stateless_reset) {
                if (retire_connID != nullptr) {
                    retire_connID(mux.lock().get(), obj.lock().get(), id);
                }
            }
        };
    }  // namespace fnet::quic::connid
}  // namespace utils
