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
#include "config.h"

namespace utils {
    namespace dnet {
        namespace quic {

            struct QUICContexts;

            struct QUIC;

            struct dnet_class_export QUICTLS {
               private:
                TLS* tls = nullptr;
                constexpr QUICTLS(TLS* t)
                    : tls(t) {}
                friend struct QUIC;

               public:
                constexpr QUICTLS() = default;
                bool set_hostname(const char* name, bool verify = true);
                bool set_alpn(const char* name, size_t len);
                bool set_verify(int mode, int (*verify_callback)(int, void*) = nullptr);
                bool set_client_cert_file(const char* cert);

                constexpr explicit operator bool() const {
                    return tls != nullptr;
                }
            };

            struct dnet_class_export QUIC {
               private:
                QUICContexts* p = nullptr;
                friend dnet_dll_export(QUIC) make_quic(TLS& tls, const Config&);
                constexpr QUIC(QUICContexts* ptr)
                    : p(ptr) {}

               public:
                bool provide_udp_datagram(const void* data, size_t size);
                bool receive_quic_packets(void* data, size_t size, size_t* recved, size_t* npacket = nullptr);
                size_t quic_packet_len(size_t npacket) const;
                QUICTLS tls();

                bool connect();
                bool accept();

                bool block() const;
                constexpr QUIC() {}
                ~QUIC();
                constexpr QUIC(QUIC&& q)
                    : p(std::exchange(q.p, nullptr)) {}
                constexpr QUIC& operator=(QUIC&& q) {
                    if (this == &q) {
                        return *this;
                    }
                    this->~QUIC();
                    p = std::exchange(q.p, nullptr);
                    return *this;
                }

                constexpr explicit operator bool() const {
                    return p != nullptr;
                }
            };

            dnet_dll_export(QUIC) make_quic(TLS& tls, const Config& config);

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
