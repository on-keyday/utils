/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_contexts
#pragma once
#include "../boxbytelen.h"
#include "../quic.h"
#include "../ack/ackhandler.h"
#include "../../tls.h"
#include "../error.h"
#include <list>
#include <string>
#include <set>
#include <random>

namespace utils {
    namespace dnet {
        namespace quic {
            struct UDPDatagram {
                BoxByteLen b;
            };

            struct QUICPackets {
                ack::PacketNumber packet_number;
                ack::PacketNumberSpace pn_space;
                bool ack_eliciting = false;
                bool in_flight = false;
                BoxByteLen b;
            };

            struct Secret {
                BoxByteLen b;
                TLSCipher cipher;
            };

            using ubytes = std::basic_string<byte, std::char_traits<byte>, glheap_allocator<byte>>;
            using ststring = std::basic_string<char, std::char_traits<char>, glheap_allocator<char>>;

            struct HandshakeData {
                ubytes data;
            };

            struct ConnIDPool {
                std::set<ubytes, std::less<>, glheap_objpool_allocator<ubytes>> connIDs;

                ubytes issue(size_t len) {
                    ubytes val;
                    auto dev = std::random_device();
                    std::uniform_int_distribution<> uni(0, 0xff);
                    for (size_t i = 0; i < len; i++) {
                        val.push_back(dnet::byte(uni(dev)));
                    }
                    connIDs.insert(val);
                    return val;
                }
            };

            enum class QState {
                start,
                sending_initial_packet,
                receving_peer_handshake_packets,
                established,

                tls_alert,
                internal_error,
                closed,

                fatal,
            };

            struct QUICContexts {
                QState state = QState::start;
                bool has_established = false;

                TLS tls;
                ack::AckHandler ackh;

                // provide/receive
                std::list<UDPDatagram, glheap_objpool_allocator<UDPDatagram>> datagrams;
                std::list<QUICPackets, glheap_objpool_allocator<QUICPackets>> quic_packets;

                Secret wsecrets[3];
                Secret rsecrets[3];

                HandshakeData hsdata[4];

                byte tls_alert = 0;
                bool flush_called = false;

                TransportError errcode = TransportError::NO_ERROR;
                ststring debug_msg;

                void internal_error(auto&& msg) {
                    errcode = TransportError::INTERNAL_ERROR;
                    debug_msg = msg;
                    state = QState::internal_error;
                }

                DefinedTransportParams params;

                ubytes initialDstID;

                ConnIDPool connIDs;
            };

            enum class quic_wait_state {
                next,
                block,
            };
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
