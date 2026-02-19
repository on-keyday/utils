/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstddef>
#include <memory>

namespace futils {
    namespace fnet::quic::datagram {

        struct Config {
            size_t pending_limit = 3;
            size_t recv_buf_limit = ~0;
        };

        struct DatagramDropNull {
            void drop(auto& s, auto pn) {}
            void detect() {}
            void sent(auto&& observer, auto&& dgram) {}

            void (*on_send_data_added_cb)(std::shared_ptr<void>&&) = nullptr;
            void (*on_recv_data_added_cb)(std::shared_ptr<void>&&) = nullptr;

            void on_send_data_added(std::shared_ptr<void>&& conn_ctx) {
                if (on_send_data_added_cb) {
                    on_send_data_added_cb(std::move(conn_ctx));
                }
            }

            void on_recv_data_added(std::shared_ptr<void>&& conn_ctx) {
                if (on_recv_data_added_cb) {
                    on_recv_data_added_cb(std::move(conn_ctx));
                }
            }
        };

        template <template <class...> class List,
                  class DropDatagram>
        struct DatagramTypeConfig {
            template <class T>
            using list = List<T>;
            using drop_datagram = DropDatagram;
        };

    }  // namespace fnet::quic::datagram
}  // namespace futils