/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/quic/internal/frame_handler.h>

namespace utils {
    namespace dnet {
        namespace quic::handler {
            bool on_ack(QUICContexts* q, frame::ACKFrame& ack, ack::PacketNumberSpace space) {
                if (auto err = q->ackh.on_ack_received(ack, space); err != TransportError::NO_ERROR) {
                    q->transport_error(err, ack.type.type_detail(), "failed on ACKHandler.on_ack_recived");
                    return false;
                }
                return true;
            }

            bool on_crypto(QUICContexts* q, frame::CryptoFrame& c, crypto::EncryptionLevel level) {
                auto& set = q->crypto_data[int(level)];
                bool added = false;
                auto offset = c.offset.qvarint();
                // check offset
                // avoid heap allocation
                if (set.read_offset == offset) {
                    q->tls.provide_quic_data(level, c.crypto_data.data, c.crypto_data.len);
                    set.read_offset += c.crypto_data.len;
                    added = true;
                }
                else {
                    CryptoData store;
                    store.offset = offset;
                    store.b = c.crypto_data;
                    if (!store.b.valid()) {
                        q->internal_error("memory exhausted");
                        return false;
                    }
                    set.data.push_back(std::move(store));
                    auto cmp = [](CryptoData& a, CryptoData& b) {
                        return a.offset < b.offset;
                    };
                    if (set.data.size() > 1) {
                        std::sort(set.data.begin(), set.data.end(), cmp);
                        // std::ranges::sort(set.data, cmp);
                    }
                }
                while (set.data.size() &&
                       set.read_offset == set.data[0].offset) {
                    auto dat = set.data[0].b.unbox();
                    q->tls.provide_quic_data(level, dat.data, dat.len);
                    set.read_offset += dat.len;
                    added = true;
                    set.data.pop_front();
                }
                if (added) {
                    if (q->has_established) {
                        return q->tls.progress_quic();
                    }
                    if (q->is_server) {
                        return q->tls.accept();
                    }
                    else {
                        return q->tls.connect();
                    }
                }
                return true;
            }

            bool on_connection_close(QUICContexts* q, frame::ConnectionCloseFrame& c) {
                q->errcode = TransportError(c.error_code.qvarint());
                q->state = QState::closed;
                return true;
            }
        }  // namespace quic::handler
    }      // namespace dnet
}  // namespace utils
