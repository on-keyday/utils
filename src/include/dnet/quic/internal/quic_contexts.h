/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_contexts
#pragma once
#include "../../boxbytelen.h"
#include "../quic.h"
#include "../../tls.h"
#include "../transport_error.h"
#include <list>
#include <string>
#include <set>
#include <random>
#include <deque>
#include "../transport_param.h"
#include "../../dll/allocator.h"
#include "../ack/sent_packet.h"
#include "../ack/loss_detection.h"

namespace utils {
    namespace dnet {
        namespace quic {
            struct UDPDatagram {
                BoxByteLen b;
            };

            struct QUICPackets {
                ack::SentPacket sent;
                ack::PacketNumberSpace pn_space;
                BoxByteLen cipher;
            };

            struct Secret {
                TLSCipher cipher;
                BoxByteLen b;
            };

            struct CryptoData {
                size_t offset;
                BoxByteLen b;
            };

            struct CryptoDataSet {
                size_t read_offset = 0;
                std::deque<CryptoData, glheap_allocator<CryptoData>> data;
            };

            using ubytes = std::basic_string<byte, std::char_traits<byte>, glheap_allocator<byte>>;
            using ststring = std::basic_string<char, std::char_traits<char>, glheap_allocator<char>>;

            struct HandshakeData {
                ubytes data;
            };

            struct ConnIDPool {
                std::set<ubytes, std::less<>, glheap_objpool_allocator<ubytes>> connIDs;

                ubytes genrandom(size_t len) {
                    ubytes val;
                    auto dev = std::random_device();
                    std::uniform_int_distribution<> uni(0, 0xff);
                    for (size_t i = 0; i < len; i++) {
                        val.push_back(dnet::byte(uni(dev)));
                    }
                    return val;
                }

                ubytes issue(size_t len) {
                    auto val = genrandom(len);
                    connIDs.insert(val);
                    return val;
                }
            };

            enum class QState {
                start,
                receving_peer_handshake_packets,
                established,

                tls_alert,
                on_error,
                closed,

                fatal,
            };

            struct ACKWait {
                BoxByteLen b;
                size_t packet_number;
            };

            struct QUICContexts {
                // state machine
                bool is_server = false;
                QState state = QState::start;
                bool has_established = false;
                bool block = false;

                // ACK handler
                ack::LossDetectionHandler ackh;

                // provide/receive from user
                std::list<UDPDatagram, glheap_objpool_allocator<UDPDatagram>> datagrams;
                std::deque<QUICPackets, glheap_objpool_allocator<QUICPackets>> quic_packets;

                // crypto
                TLS tls;
                CryptoDataSet crypto_data[4];
                Secret wsecrets[4];
                Secret rsecrets[4];
                HandshakeData hsdata[4];
                byte tls_alert = 0;
                bool flush_called = false;

                // error state
                TransportError errcode = TransportError::NO_ERROR;
                FrameType err_frame_type = FrameType::PADDING;
                ststring debug_msg;

                void internal_error(auto&& msg) {
                    errcode = TransportError::INTERNAL_ERROR;
                    debug_msg = msg;
                    state = QState::on_error;
                }

                void transport_error(TransportError err, FrameType type, auto&& msg) {
                    errcode = err;
                    debug_msg = msg;
                    err_frame_type = type;
                    state = QState::on_error;
                }

                // transport parameter and connection id
                DefinedTransportParams params;
                ubytes initialDstID;
                ConnIDPool srcIDs;
            };

            enum class quic_wait_state {
                next,
                block,
            };

            std::pair<WPacket, BoxByteLen> make_tmpbuf(size_t size);

            void start_initial_handshake(QUICContexts* p);
            quic_wait_state receiving_peer_handshake_packets(QUICContexts* q);

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
