/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../../std/list.h"
#include "../frame/datagram.h"
#include "../../storage.h"
#include "../packet_number.h"
#include "../ioresult.h"
#include "../frame/writer.h"
#include "../frame/datagram.h"
#include "../ack/ack_lost_record.h"
#include "../resend/fragments.h"
#include "../../../helper/defer.h"
#include "../transport_error.h"
#include "config.h"

namespace utils {
    namespace fnet::quic::datagram {

        struct SendDatagram {
            storage data;
            size_t pending;
            packetnum::Value pn;
        };

        struct RecvDatagram {
            storage data;
            packetnum::Value packet_number;
        };

        struct DatagramDropRetransmit {
            resend::Retransmiter<SendDatagram, slib::list> retransmit;
            std::shared_ptr<void> arg;
            void (*notify_drop_cb)(std::shared_ptr<void>&, storage&, packetnum::Value) = nullptr;

            void drop(storage& s, packetnum::Value pn) {
                if (notify_drop_cb != nullptr) {
                    notify_drop_cb(arg, s, pn);
                }
            }

            void detect() {
                ack::ACKRecorder rec;
                retransmit.retransmit(rec, [&](SendDatagram& sd, auto&&) {
                    drop(sd.data, sd.pn);
                    return IOResult::ok;
                });
            }

            void sent(auto&& observer, SendDatagram&& dgram) {
                retransmit.sent(observer, std::move(dgram));
            }
        };

        template <class Locker, class DropDetector>
        struct DatagramManager {
           private:
            slib::list<SendDatagram> send_que;
            slib::list<RecvDatagram> recv_que;
            std::atomic_size_t peer_param_size = 0;
            size_t local_param_size = 0;
            Config config;
            Locker locker;
            DropDetector drop;

            auto lock() {
                locker.lock();
                return helper::defer([&] {
                    locker.unlock();
                });
            }

           public:
            void reset(size_t local_size, Config config, DropDetector&& detect = DropDetector{}) {
                send_que.clear();
                recv_que.clear();
                local_param_size = local_size;
                peer_param_size = 0;
                this->config = config;
                drop = std::move(detect);
            }

            void on_transport_parameter(std::uint64_t max_dgram_frame) {
                peer_param_size = max_dgram_frame;
            }

            error::Error recv(packetnum::Value pn, const frame::DatagramFrame& dgram) {
                if (local_param_size == 0) {
                    return QUICError{
                        .msg = "received DatagramFrame when max_datagram_frame_size is 0",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = dgram.type.type_detail(),
                    };
                }
                if (local_param_size < dgram.datagram_data.size()) {
                    return QUICError{
                        .msg = "DatagramFrame data size is over max_datagram_frame_size",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = dgram.type.type_detail(),
                    };
                }
                const auto d = lock();
                RecvDatagram recv;
                recv.data = make_storage(dgram.datagram_data);
                recv.packet_number = pn;
                recv_que.push_back(std::move(recv));
                if (recv_que.size() > config.recv_buf_limit) {
                    recv_que.pop_front();
                }
                return error::none;
            }

            IOResult send(packetnum::Value pn, frame::fwriter& fw, auto&& observer) {
                const auto d = lock();
                if (send_que.empty()) {
                    return IOResult::no_data;
                }
                drop.detect();
                for (auto it = send_que.begin(); it != send_que.end();) {
                    auto [dgram, ok] = frame::make_fit_size_Datagram(fw.remain().size(), it->data);
                    if (!ok) {
                        it->pending++;
                        if (it->pending >= config.pending_limit) {
                            drop.drop(it->data, packetnum::infinity);
                            it = send_que.erase(it);
                            continue;
                        }
                        it++;
                        continue;
                    }
                    if (!fw.write(dgram)) {
                        return IOResult::fatal;
                    }
                    drop.sent(observer, std::move(*it));
                    it++;
                }
                return IOResult::ok;
            }

            void get_datagram(auto&& save) {
                const auto d = lock();
                save(recv_que);
                recv_que.clear();
            }

            bool add_datagram(view::rvec data) {
                if (peer_param_size == 0 || data.size() > peer_param_size) {
                    return false;
                }
                SendDatagram sd;
                sd.data = make_storage(data);
                const auto d = lock();
                send_que.push_back(std::move(sd));
                return true;
            }
        };
    }  // namespace fnet::quic::datagram
}  // namespace utils
