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

namespace utils {
    namespace fnet::quic::datagram {

        struct SendDatagram {
            storage data;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        struct RecvDatagram {
            storage data;
            packetnum::Value packet_number;
        };

        struct DatagramManager {
           private:
            slib::list<SendDatagram> send_que;
            slib::list<RecvDatagram> recv_que;
            slib::list<SendDatagram> retransmit_que;
            size_t peer_param_size = 0;

           public:
            void reset() {
                send_que.clear();
                recv_que.clear();
                peer_param_size = 0;
            }

            void on_transport_parameter(std::uint64_t max_dgram_frame) {
                peer_param_size = max_dgram_frame;
            }

            IOResult send(frame::fwriter& fw) {
                if (send_que.empty()) {
                    return IOResult::no_data;
                }
            }

            bool add_datagram(view::rvec data) {
                if (data.size() > peer_param_size) {
                    return false;
                }
                SendDatagram sd;
                sd.data = make_storage(data);
            }
        };
    }  // namespace fnet::quic::datagram
}  // namespace utils
