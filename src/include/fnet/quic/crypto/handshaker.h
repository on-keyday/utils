/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../resend/fragments.h"
#include "../../std/vector.h"
#include "../frame/writer.h"
#include "../../tls/tls.h"
#include "../frame/crypto.h"
#include <algorithm>
#include "../transport_error.h"

namespace utils {
    namespace fnet::quic::crypto {
        struct CryptoRecvFragment {
            storage fragment;
            std::uint64_t offset = 0;
        };

        struct CryptoHandshaker {
           private:
            using Vec = slib::vector<CryptoRecvFragment>;
            Vec recv_data;
            size_t read_offset = 0;

            flex_storage write_data;
            size_t write_offset = 0;
            resend::Retransmiter<resend::CryptoFragment, slib::list> frags;

            IOResult send_retransmit(auto&& observer_vec, frame::fwriter& w) {
                return frags.retransmit(observer_vec, [&](resend::CryptoFragment& frag, auto&& save_new) {
                    auto [frame, ok] = frame::make_fit_size_Crypto(w.remain().size(), frag.offset, frag.data);
                    if (!ok) {
                        return IOResult::not_in_io_state;
                    }
                    if (!w.write(frame)) {
                        return IOResult::fatal;
                    }
                    resend::CryptoFragment new_frag;
                    new_frag.data = frame.crypto_data;
                    new_frag.offset = frame.offset;
                    save_new(resend::CryptoFragment{
                        .offset = frame.offset,
                        .data = frame.crypto_data,
                    });
                    if (frag.data.size() != frag.data.size()) {
                        size_t new_offset = frame.offset + frame.length;
                        view::rvec remain = frag.data.rvec().substr(new_offset);
                        frag.offset = new_offset;
                        frag.data = flex_storage(remain);
                        return IOResult::not_in_io_state;
                    }
                    return IOResult::ok;
                });
            }

            IOResult send_crypto(auto&& observer_vec, frame::fwriter& w) {
                auto remain_data = write_data.rvec().substr(write_offset);
                const auto writable_size = w.remain().size();
                auto [frame, ok] = frame::make_fit_size_Crypto(writable_size, write_offset, remain_data);
                if (!ok) {
                    return IOResult::no_capacity;
                }
                if (!w.write(frame)) {
                    return IOResult::fatal;
                }
                write_offset += frame.crypto_data.size();
                frags.sent(observer_vec,
                           resend::CryptoFragment{
                               .offset = frame.offset,
                               .data = frame.crypto_data,
                           });
                return IOResult::ok;
            }

           public:
            void reset() {
                write_data.resize(0);
                write_offset = 0;
                frags.reset();
                recv_data.clear();
                read_offset = 0;
            }

            void add_send_data(view::rvec data) {
                write_data.append(data);
            }

            IOResult send(auto&& observer_vec, frame::fwriter& w) {
                if (auto res = send_retransmit(observer_vec, w);
                    res == IOResult::fatal) {
                    return res;
                }
                if (write_data.size() - write_offset == 0) {
                    return IOResult::no_data;
                }
                return send_crypto(observer_vec, w);
            }

            error::Error recv_crypto(tls::TLS& tls, EncryptionLevel level,
                                     bool is_server, bool& handshake_complete,
                                     const frame::CryptoFrame& frame) {
                if (frame.offset < read_offset) {
                    return error::none;  // duplicated recv
                }
                if (read_offset == frame.offset) {  // offset matched
                    auto stdata = frame.crypto_data;
                    if (auto err = tls.provide_quic_data(level, stdata)) {
                        return err;
                    }
                    read_offset += stdata.size();
                    if (recv_data.size() > 1) {
                        std::sort(recv_data.begin(), recv_data.end(), [](auto& a, auto& b) {
                            return a.offset > b.offset;
                        });
                    }
                    while (recv_data.size() &&
                           read_offset == recv_data.back().offset) {
                        auto& src = recv_data.back();
                        if (auto err = tls.provide_quic_data(level, src.fragment)) {
                            return err;
                        }
                        read_offset += src.fragment.size();
                        recv_data.pop_back();
                    }
                    if (handshake_complete) {
                        if (auto err = tls.progress_quic()) {
                            return err;
                        }
                    }
                    else {
                        if (is_server) {
                            if (auto err = tls.accept(); err) {
                                if (tls::isTLSBlock(err)) {
                                    return error::none;
                                }
                                return err;  // error
                            }
                        }
                        else {
                            if (auto err = tls.connect(); err) {
                                if (tls::isTLSBlock(err)) {
                                    return error::none;
                                }
                                return err;  // error
                            }
                        }
                        handshake_complete = true;
                    }
                }
                else {  // offset not matched. buffering
                    CryptoRecvFragment frag;
                    frag.fragment = make_storage(frame.crypto_data);
                    frag.offset = frame.offset;
                    recv_data.push_back(std::move(frag));
                }
                return error::none;
            }
        };

        struct CryptoHandshakers {
           private:
            enum : byte {
                flag_server = 0x1,
                flag_handshake_complete = 0x2,
                flag_handshake_done = 0x4,
                flag_alert = 0x8,
            };
            byte flag = 0;

            // bool server = false;
            //  TLS handshake completed
            // bool handshake_complete_ = false;
            // client: HANDSHAKE_DONE received
            // server: HANDSHAKE_DONE acked
            // not strictly same as handshake_confirmed
            // use handshake_confirmed() instead of this
            // bool handshake_done_ = false;
            tls::TLS tls;
            CryptoHandshaker initial, handshake, onertt;

            std::shared_ptr<ack::ACKLostRecord> wait_for_done;

            byte alert_code = 0;
            // bool alert = false;

           public:
            constexpr void set_alert(byte code) {
                alert_code = code;
                flag |= flag_alert;
            }

            void reset(tls::TLS&& new_tls, bool is_server) {
                tls = std::move(new_tls);
                flag = is_server ? flag_server : 0;
                // is_server = is_server;
                // handshake_done_ = false;
                // handshake_complete_ = false;
                wait_for_done = nullptr;
                initial.reset();
                handshake.reset();
                onertt.reset();
                alert_code = 0;
                // alert = false;
            }

            tls::TLS& get_tls() {
                return tls;
            }

            bool add_handshake_data(EncryptionLevel level, view::rvec data) {
                switch (level) {
                    case EncryptionLevel::initial:
                        initial.add_send_data(data);
                        return true;
                    case EncryptionLevel::handshake:
                        handshake.add_send_data(data);
                        return true;
                    case EncryptionLevel::application:
                        onertt.add_send_data(data);
                        return true;
                    default:
                        return false;
                }
            }

            IOResult send(PacketType type, auto&& observer_vec, frame::fwriter& w) {
                if (is_server() && handshake_complete() && !handshake_done() && type == PacketType::OneRTT) {
                    if (!wait_for_done || wait_for_done->is_lost()) {
                        if (w.remain().size() == 0) {
                            return IOResult::no_capacity;
                        }
                        frame::HandshakeDoneFrame frame;
                        if (!w.write(frame)) {
                            return IOResult::fatal;
                        }
                        wait_for_done = ack::make_ack_wait();
                        observer_vec.push_back(wait_for_done);
                    }
                    else if (wait_for_done->is_ack()) {
                        ack::put_ack_wait(std::move(wait_for_done));
                        flag |= flag_handshake_done;
                    }
                }
                switch (type) {
                    case PacketType::Initial:
                        return initial.send(observer_vec, w);
                    case PacketType::Handshake:
                        return handshake.send(observer_vec, w);
                    case PacketType::OneRTT:
                        return onertt.send(observer_vec, w);
                    default:
                        return IOResult::invalid_data;
                }
            }

            error::Error recv_handshake_done() {
                if (is_server()) {
                    return QUICError{
                        .msg = "recv HANDSHAKE_DONE on server",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::HANDSHAKE_DONE,
                    };
                }
                flag |= flag_handshake_done;
                return error::none;
            }

            error::Error recv_crypto(PacketType type, const frame::CryptoFrame& frame) {
                bool hs_complete = handshake_complete();
                const auto d = helper::defer([&] {
                    if (hs_complete) {
                        flag |= flag_handshake_complete;
                    }
                });
                switch (type) {
                    case PacketType::Initial:
                        return initial.recv_crypto(tls, EncryptionLevel::initial, is_server(), hs_complete, frame);
                    case PacketType::Handshake:
                        return handshake.recv_crypto(tls, EncryptionLevel::handshake, is_server(), hs_complete, frame);
                    case PacketType::OneRTT:
                        return onertt.recv_crypto(tls, EncryptionLevel::application, is_server(), hs_complete, frame);
                    default:
                        return error::Error("invalid packet type for crypto frame");
                }
            }

            error::Error recv(PacketType type, const frame::Frame& frame) {
                if (frame.type.type_detail() == FrameType::CRYPTO) {
                    return recv_crypto(type, static_cast<const frame::CryptoFrame&>(frame));
                }
                else if (frame.type.type_detail() == FrameType::HANDSHAKE_DONE) {
                    if (type != PacketType::OneRTT) {
                        return QUICError{
                            .msg = "HANDHSHAKE_DONE on non OneRTT packet",
                            .transport_error = TransportError::PROTOCOL_VIOLATION,
                            .packet_type = type,
                            .frame_type = FrameType::HANDSHAKE_DONE,
                        };
                    }
                    return recv_handshake_done();
                }
                return error::none;
            }

            constexpr bool is_server() const {
                return flag & flag_server;
            }

            // TLS handshake completed
            constexpr bool handshake_complete() const {
                return flag & flag_handshake_complete;
            }

            // client: HANDSHAKE_DONE received
            // server: HANDSHAKE_DONE acked
            // not strictly same as handshake_confirmed
            // use handshake_confirmed() instead of this
            constexpr bool handshake_done() const {
                return flag & flag_handshake_done;
            }

            // QUIC handshake confirmed
            // this means, peer address is validated
            constexpr bool handshake_confirmed() const {
                if (is_server()) {
                    return handshake_complete();
                }
                return handshake_done();
            }
        };
    }  // namespace fnet::quic::crypto
}  // namespace utils
