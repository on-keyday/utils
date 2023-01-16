/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_contexts
#pragma once
#include "../../dll/dllh.h"
#include "../../boxbytelen.h"
#include "../quic.h"
#include "../../tls.h"
#include "../transport_error.h"
#include <list>
#include <string>
#include <set>
#include <random>
#include <unordered_map>
#include <deque>
#include "../crypto/crypto_suite.h"
#include "../transport_parameter/transport_param_ctx.h"
#include "../../dll/allocator.h"
#include "../ack/sent_packet.h"
#include "../ack/loss_detection.h"
#include "../../error.h"
#include "../../mem.h"
// #include "../frame/vframe.h"
#include "../frame/dynframe.h"
#include "../dgram/limit.h"
#include "../../address.h"
// #include "../stream/stream_impl.h"
// #include "../stream/stream_handler.h"

namespace utils {
    namespace dnet {
        namespace quic {
            struct UDPDatagram {
                BoxByteLen data;
                NetAddrPort addr;
            };

            struct SendWaitQUICPacket {
                ack::SentPacket sent;
                ack::PacketNumberSpace pn_space;
                BoxByteLen sending_packet;
            };

            struct ParsePendingQUICPackets {
                BoxByteLen cipher_packet;
                PacketType type = PacketType::Unknown;
                ack::time_t queued_at = ack::invalid_time;
            };

            using ubytes = std::basic_string<byte, std::char_traits<byte>, glheap_allocator<byte>>;

            struct ExternalRandomDevice {
                closure::Closure<std::uint32_t>& ref;
                static constexpr std::uint32_t(min)() {
                    return 0;
                }
                static constexpr std::uint32_t(max)() {
                    return ~0;
                }

                std::uint32_t operator()() {
                    return ref();
                }
            };

            static_assert(std::uniform_random_bit_generator<ExternalRandomDevice>, "library bug!!");

            struct ConnID {
                ubytes id;
                byte stateless_reset_token[16] = {};

                ByteLen get() {
                    return {id.data(), id.size()};
                }
            };

            using ConnIDMap = easy::H<size_t, ConnID>;

            struct ConnIDIssuer {
                ConnIDMap connIDs;
                closure::Closure<std::uint32_t> gen_rand;
                size_t seq_id = 0;

                ubytes genrandom(size_t len) {
                    ubytes val;
                    if (!gen_rand) {
                        return {};
                    }
                    ExternalRandomDevice dev{gen_rand};
                    std::uniform_int_distribution<> uni(0, 0xff);
                    for (size_t i = 0; i < len; i++) {
                        val.push_back(byte(uni(dev)));
                    }
                    return val;
                }

               private:
                void fill_stateless_reset(byte* token) {
                    ExternalRandomDevice dev{gen_rand};
                    std::uniform_int_distribution<> uni(0, 0xff);
                    for (auto i = 0; i < 16; i++) {
                        token[i] = uni(dev);
                    }
                }

               public:
                std::pair<size_t, ByteLen> issue(size_t len) {
                    if (!gen_rand) {
                        return {};
                    }
                    while (true) {
                        ConnID val;
                        val.id = genrandom(len);
                        fill_stateless_reset(val.stateless_reset_token);
                        auto it = connIDs.emplace(seq_id, std::move(val));
                        seq_id++;
                        return {seq_id - 1, {it.first->second.id.data(), len}};
                    }
                }
            };

            struct ConnIDManager {
                ConnIDMap connIDs;
                size_t largest_retired = 0;

                auto& get() {
                    for (auto& d : connIDs) {
                        return d.second;
                    }
                    __builtin_unreachable();
                }

                bool retire(size_t id) {
                    if (id < largest_retired) {
                        return false;
                    }
                    largest_retired = id;
                    return connIDs.erase(id);
                }

                bool add_new(size_t seq, ubytes id, byte* stateless_reset) {
                    ConnID connID;
                    connID.id = std::move(id);
                    if (stateless_reset) {
                        memcpy(connID.stateless_reset_token, stateless_reset, 16);
                    }
                    return connIDs.emplace(seq, std::move(connID)).second;
                }
            };

            struct UnackedPacketNumber {
                easy::Vec<packetnum::Value> numbers;
                bool gen_range(ack::ACKRangeVec& range) {
                    return ack::gen_ragnes_from_recved(numbers, range);
                }

                void clear() {
                    numbers.clear();
                }
            };

            struct UnmergedFrame {
                std::deque<frame::DynFramePtr, glheap_allocator<frame::DynFramePtr>> frames;

                error::Error push(auto&& frame) {
                    auto vf = frame::makeDynFrame(std::move(frame));
                    frames.push_back(std::move(vf));
                    return error::none;
                }

                error::Error push_front(const auto& frame) {
                    auto vf = frame::makeDynFrame(frame);
                    frames.push_front(std::move(vf));
                    return error::none;
                }

                void pop(size_t count) {
                    for (size_t i = 0; i < count; i++) {
                        frames.pop_front();
                    }
                }

                frame::DynFramePtr& front() {
                    return frames.front();
                }

                frame::DynFramePtr& operator[](size_t i) {
                    return frames[i];
                }

                size_t size() const {
                    return frames.size();
                }
            };

            struct UnmergedFrames {
                UnmergedFrame frames[3];
                error::Error push(ack::PacketNumberSpace space, auto&& frame) {
                    return frames[int(space)].push(std::move(frame));
                }

                error::Error push_front(ack::PacketNumberSpace space, auto&& frame) {
                    return frames[int(space)].push_front(std::move(frame));
                }

                UnmergedFrame* begin() {
                    return frames;
                }

                UnmergedFrame* end() {
                    return frames + 3;
                }

                UnmergedFrame& operator[](size_t i) {
                    assert(0 <= i && i <= 2);
                    return frames[i];
                }
            };

            enum class QState {
                start,
                receving_peer_handshake_packets,
                sending_local_handshake_packets,
                waiting_for_handshake_done,
                established,

                tls_alert,
                on_error,
                closed,

                fatal,
            };

            struct ErrorState {
                QUICError* qerr = nullptr;
                error::Error err;
                error::Error close_err;
            };

            struct SendWaitPacketQueue {
                std::list<SendWaitQUICPacket, glheap_objpool_allocator<SendWaitQUICPacket>> quic_packets;
                error::Error enqueue_cipher(
                    BoxByteLen plain, BoxByteLen cipher, PacketType type, packetnum::Value packet_number,
                    bool ack_eliciting, bool in_flight) {
                    SendWaitQUICPacket sent;
                    sent.pn_space = ack::from_packet_type(type);
                    sent.sent.sent_bytes = cipher.len();
                    sent.sending_packet = std::move(cipher);
                    sent.sent.sent_plain = std::move(plain);
                    sent.sent.type = type;
                    sent.sent.packet_number = packet_number;
                    sent.sent.ack_eliciting = ack_eliciting;
                    sent.sent.in_flight = in_flight;
                    quic_packets.push_back(std::move(sent));
                    return error::none;
                }

                error::Error enqueue_plain(BoxByteLen plain, PacketType typ) {
                    SendWaitQUICPacket sent;
                    sent.sending_packet = std::move(plain);
                    sent.pn_space = ack::PacketNumberSpace::no_space;
                    sent.sent.type = typ;
                    quic_packets.push_back(std::move(sent));
                    return error::none;
                }

                size_t size() const {
                    return quic_packets.size();
                }

                SendWaitQUICPacket& front() {
                    return quic_packets.front();
                }

                void pop_front() {
                    quic_packets.pop_front();
                }
            };

            struct QUICFlags {
                bool zero_connID = false;
                bool remote_src_recved = false;
                bool is_server = false;
                size_t max_udp_datagram_size = dgram::initial_datagram_size_limit;
            };

            struct QUICContexts {
                // state machine
                QState state = QState::start;

                QUICFlags flags;

                // ACK handler
                ack::LossDetectionHandler ackh;
                UnackedPacketNumber unacked[3];

                // provide/receive from user
                std::list<ParsePendingQUICPackets, glheap_allocator<ParsePendingQUICPackets>> parse_wait;
                std::list<UDPDatagram, glheap_objpool_allocator<UDPDatagram>> datagrams;
                SendWaitPacketQueue quic_packets;
                // crypto
                crypto::CryptoSuite crypto;

                // error state
                ErrorState errs;

                // transport parameter and connection id
                trsparam::TransportParameters params;
                // srcIDs is issued by local
                ConnIDIssuer srcIDs;
                // dstIDs is issued by remote
                ConnIDManager dstIDs;

                // frames
                UnmergedFrames frames;

                // streams

                // temporary packet creation buffer
                BoxByteLen tmpbuf;

                WPacket get_tmpbuf() {
                    if (tmpbuf.len() < 3000) {
                        tmpbuf = BoxByteLen{3000};
                    }
                    WPacket wp;
                    wp.b = tmpbuf.unbox();
                    return wp;
                }

                size_t udp_limit() {
                    return dgram::current_limit(flags.max_udp_datagram_size, params.peer.max_udp_payload_size);
                }
            };

            error::Error start_initial_handshake(QUICContexts* p);
            error::Error receiving_peer_handshake_packets(QUICContexts* q);
            error::Error sending_local_handshake_packets(QUICContexts* q);

            error::Error handle_crypto_error(QUICContexts* q, error::Error err);

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
