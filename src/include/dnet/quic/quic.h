/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic - QUIC object
#pragma once
#include "../dll/dllh.h"
#include "../tls.h"

namespace utils {
    namespace dnet {
        namespace quic {
            struct QUICContexts;

            struct dnet_class_export QUIC {
               private:
                QUICContexts* p = nullptr;
                friend dnet_dll_export(QUIC) make_quic(TLS& tls);
                constexpr QUIC(QUICContexts* ptr)
                    : p(ptr) {}

               public:
                bool provide_udp_datagram(const void* data, size_t size);
                bool receive_quic_packet(void* data, size_t size, size_t* recved);

                void set_hostname(const char* name, size_t len);
                bool connect();
                bool accept();
                constexpr QUIC() {}
                ~QUIC();
                constexpr QUIC(QUIC&& q)
                    : p(std::exchange(q.p, nullptr)) {}
                constexpr QUIC& operator=(QUIC&& q) {
                    if (this == &q) {
                        return *this;
                    }
                    this->~QUIC();
                }

                constexpr explicit operator bool() const {
                    return p != nullptr;
                }
            };

            dnet_dll_export(QUIC) make_quic(TLS& tls);

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
