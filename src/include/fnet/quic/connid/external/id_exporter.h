/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <view/iovec.h>
#include <memory>
#include <fnet/error.h>
#include "random.h"

namespace futils {
    namespace fnet::quic::connid {

        struct ExporterFn {
            error::Error (*gen_callback)(std::shared_ptr<void> mux, std::shared_ptr<void> obj, Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec statless_reset) = nullptr;
            void (*add_connID)(std::shared_ptr<void> mux, std::shared_ptr<void> obj, view::rvec id, view::rvec sr) = nullptr;
            void (*retire_connID)(std::shared_ptr<void> mux, std::shared_ptr<void> obj, view::rvec id, view::rvec sr) = nullptr;
        };

        // ConnIDExporter exports connection IDs to multiplexer
        struct ConnIDExporter {
            const ExporterFn* exporter = nullptr;
            // multiplexer pointer
            std::weak_ptr<void> mux;
            // object pointer (may refer self)
            std::weak_ptr<void> obj;

            void add(view::rvec id, view::rvec stateless_reset) {
                if (exporter && exporter->add_connID != nullptr) {
                    exporter->add_connID(mux.lock(), obj.lock(), id, stateless_reset);
                }
            }

            void retire(view::rvec id, view::rvec stateless_reset) {
                if (exporter && exporter->retire_connID != nullptr) {
                    exporter->retire_connID(mux.lock(), obj.lock(), id, stateless_reset);
                }
            }

            error::Error generate(Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec stateless_reset) {
                if (exporter && exporter->gen_callback != nullptr) {
                    return exporter->gen_callback(mux.lock(), obj.lock(), rand, version, len, connID, stateless_reset);
                }
                return error::Error("no connID generator", error::Category::lib, error::fnet_quic_user_arg_error);
            }
        };

        inline error::Error default_generator(std::shared_ptr<void>, std::shared_ptr<void>, Random& rand, std::uint32_t version, byte len, view::wvec connID, view::wvec stateless_reset) {
            if (!rand.valid()) {
                return error::Error("invalid random", error::Category::lib, error::fnet_quic_user_arg_error);
            }
            if (!rand.gen_random(connID, RandomUsage::connection_id) ||
                !rand.gen_random(stateless_reset, RandomUsage::stateless_reset_token)) {
                return error::Error("gen_random failed", error::Category::lib, error::fnet_quic_user_arg_error);
            }
            return error::none;
        }

        constexpr ExporterFn default_exporter = {
            default_generator,
            nullptr,
            nullptr,
        };

    }  // namespace fnet::quic::connid
}  // namespace futils
