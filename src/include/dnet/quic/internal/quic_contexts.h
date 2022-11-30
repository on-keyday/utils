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
#include <unordered_map>
#include <deque>
#include "../crypto/crypto_suite.h"
#include "../transport_parameter/transport_param_ctx.h"
#include "../../dll/allocator.h"
#include "../ack/sent_packet.h"
#include "../ack/loss_detection.h"
#include "../../error.h"
#include "../../mem.h"
#include "../frame/vframe.h"

namespace utils {
    namespace dnet {
        namespace quic {
            struct UDPDatagram {
                BoxByteLen b;
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
            };

            using ConnIDMap = std::unordered_map<size_t, ConnID, std::hash<size_t>, std::equal_to<size_t>, glheap_objpool_allocator<std::pair<const size_t, ConnID>>>;

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
                        val.push_back(dnet::byte(uni(dev)));
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

            using ACKRangeVec = std::vector<frame::ACKRange, glheap_allocator<frame::ACKRange>>;

            struct UnackedPacketNumber {
                std::vector<ack::PacketNumber, glheap_allocator<ack::PacketNumber>> numbers;
                bool gen_range(ACKRangeVec& range) {
                    if (numbers.size() == 0) {
                        return false;
                    }
                    if (numbers.size() == 1) {
                        auto res = size_t(numbers[0]);
                        range.push_back(frame::ACKRange{res, res});
                        return true;
                    }
                    std::sort(numbers.begin(), numbers.end(), std::greater<>{});
                    auto size = numbers.size();
                    frame::ACKRange cur;
                    cur.largest = size_t(numbers[0]);
                    cur.smallest = cur.largest;
                    for (size_t i = 1; i < size; i++) {
                        if (numbers[i - 1] == numbers[i] + 1) {
                            cur.smallest = size_t(numbers[i]);
                        }
                        else {
                            range.push_back(std::move(cur));
                            cur.largest = size_t(numbers[i]);
                            cur.smallest = cur.largest;
                        }
                    }
                    range.push_back(std::move(cur));
                    return true;
                }
            };

            struct UnmergedFrame {
                using VFramePtr = std::shared_ptr<frame::VFrame>;
                std::vector<VFramePtr, glheap_allocator<VFramePtr>> frames;

                error::Error push(const auto& frame) {
                    auto vf = frame::make_vframe(frame);
                    frames.push_back(std::move(vf));
                    return error::none;
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

            struct ErrorState {
                QUICError* qerr = nullptr;
                error::Error err;
                error::Error close_err;
            };

            struct SendWaitPacketQueue {
                std::list<SendWaitQUICPacket, glheap_objpool_allocator<SendWaitQUICPacket>> quic_packets;
                error::Error enqueue_cipher(
                    BoxByteLen plain, BoxByteLen cipher, PacketType type, ack::PacketNumber packet_number,
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

                error::Error enqueue_plain(BoxByteLen plain) {
                    SendWaitQUICPacket sent;
                    sent.sending_packet = std::move(plain);
                    sent.pn_space = ack::PacketNumberSpace::no_space;
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
                ubytes initialDstID;
                // srcIDs is issued by local
                ConnIDIssuer srcIDs;
                // dstIDs is issued by remote
                ConnIDManager dstIDs;

                // frames
                UnmergedFrame frames;

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
            };

            error::Error start_initial_handshake(QUICContexts* p);
            error::Error receiving_peer_handshake_packets(QUICContexts* q);

            error::Error handle_crypto_error(QUICContexts* q, error::Error err);

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
