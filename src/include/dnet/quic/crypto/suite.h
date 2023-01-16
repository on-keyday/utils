/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// suite - crypto suite
#pragma once
#include "../../storage.h"
#include "../../tls.h"
#include "../ack/ack_lost_record.h"
#include "../frame/crypto.h"
#include "../packet_number.h"
#include "../../std/vector.h"
#include "../../std/hash_map.h"
#include "../../../helper/condincr.h"
#include <algorithm>
#include "../transport_error.h"
#include "../ioresult.h"
#include "enc_level.h"
#include "keys.h"
#include "encdec_suite.h"
#include "secret.h"
#include "../frame/writer.h"

namespace utils {
    namespace dnet::quic::crypto {

        struct CryptoWriteFragment {
            flex_storage fragment;
            std::uint64_t offset = 0;
            std::shared_ptr<ack::ACKLostRecord> wait;
        };

        struct CryptoRecvFragment {
            storage fragment;
            std::uint64_t offset = 0;
        };

        template <class T>
        concept has_version = requires(T t) {
                                  { t.version };
                              };

        struct CryptoData {
            Secret rsecret;
            Secret wsecret;

            using Vec = slib::vector<CryptoRecvFragment>;
            Vec recv_data;
            size_t read_offset = 0;

            flex_storage write_data;
            size_t write_offset = 0;
            using Map = slib::hash_map<packetnum::Value, CryptoWriteFragment>;
            Map fragments;

            static bool save_new_fragment(auto& frag, packetnum::Value pn, auto&& observer_vec, frame::CryptoFrame frame) {
                auto [it, ok2] = frag.try_emplace(pn, CryptoWriteFragment{});
                if (!ok2) {
                    return false;
                }
                it->second.fragment = frame.crypto_data;
                it->second.offset = frame.offset;
                it->second.wait = ack::make_ack_wait();
                observer_vec.push_back(it->second.wait);
                return true;
            }

           private:
            IOResult send_retransmit(packetnum::Value pn, auto&& observer_vec, frame::fwriter& w) {
                bool removed = false;
                auto incr = helper::cond_incr(removed);
                Map tmp_storage;
                for (auto it = fragments.begin(); it != fragments.end(); incr(it)) {
                    if (it->second.wait->is_ack()) {
                        it = fragments.erase(it);
                        removed = true;
                        continue;
                    }
                    if (it->second.wait->is_lost()) {
                        auto [frame, ok] = frame::make_fit_size_Crypto(w.remain().size(), it->second.offset, it->second.fragment.rvec());
                        if (!ok) {
                            continue;
                        }
                        if (!w.write(frame)) {
                            return IOResult::fatal;
                        }
                        if (!save_new_fragment(tmp_storage, pn, observer_vec, frame)) {
                            return IOResult::fatal;
                        }
                        if (it->second.fragment.size() != frame.crypto_data.size()) {
                            size_t new_offset = frame.offset + frame.length;
                            view::rvec remain = it->second.fragment.rvec().substr(new_offset);
                            it->second.offset = new_offset;
                            it->second.fragment = flex_storage(remain);
                        }
                        else {
                            it = fragments.erase(it);
                            removed = true;
                        }
                    }
                }
                return IOResult::ok;
            }

            IOResult send_crypto(packetnum::Value pn, auto&& observer_vec, frame::fwriter& w) {
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
                return save_new_fragment(fragments, pn, observer_vec, frame)
                           ? IOResult::ok
                           : IOResult::fatal;
            }

           public:
            IOResult send(packetnum::Value pn, auto&& observer_vec, frame::fwriter& w) {
                if (write_data.size() - write_offset == 0) {
                    return send_retransmit(pn, observer_vec, w);
                }
                return send_crypto(pn, observer_vec, w);
            }
        };

        struct CryptoSuite {
            CryptoData crypto_data[4];
            TLS tls;
            bool is_server = false;
            // TLS handshake completed
            bool handshake_complete = false;
            // client: HANDSHAKE_DONE received
            // server: HANDSHAKE_DONE acked
            // not strictly same as handshake_confirmed
            // use handshake_confirmed() instead of this
            bool handshake_done = false;
            byte alert_code = 0;
            bool alert = false;

            std::shared_ptr<ack::ACKLostRecord> wait_for_done;

            IOResult send(PacketType type, packetnum::Value pn, auto&& observer_vec, frame::fwriter& w) {
                if (!is_CryptoPacket(type)) {
                    return IOResult::invalid_data;
                }
                if (is_server && handshake_complete && !handshake_done && type == PacketType::OneRTT) {
                    if (!wait_for_done || wait_for_done->is_lost()) {
                        if (w.remain().size() == 0) {
                            return IOResult::no_capacity;
                        }
                        frame::HandshakeDoneFrame frame;
                        w.write(frame);
                        if (!wait_for_done) {
                            wait_for_done = ack::make_ack_wait();
                        }
                        else {
                            wait_for_done->wait();
                        }
                        observer_vec.push_back(wait_for_done);
                    }
                    else if (wait_for_done->is_ack()) {
                        handshake_done = true;
                        wait_for_done = nullptr;
                    }
                }
                return crypto_data[level_to_index(type_to_level(type))].send(pn, observer_vec, w);
            }

            error::Error recv_handshake_done() {
                if (is_server) {
                    return QUICError{
                        .msg = "recv HANDSHAKE_DONE on server",
                        .transport_error = TransportError::PROTOCOL_VIOLATION,
                        .frame_type = FrameType::HANDSHAKE_DONE,
                    };
                }
                handshake_done = true;
                return error::none;
            }

            error::Error recv(PacketType type, const frame::CryptoFrame& frame) {
                if (!is_CryptoPacket(type)) {
                    return error::Error("invalid packet type for crypto");
                }
                auto level = type_to_level(type);
                auto& data = crypto_data[level_to_index(level)];
                if (frame.offset < data.read_offset) {
                    return error::none;  // duplicated recv
                }
                if (data.read_offset == frame.offset) {  // offset matched
                    auto stdata = frame.crypto_data;
                    if (auto err = tls.provide_quic_data(level, stdata.data(), stdata.size())) {
                        return err;
                    }
                    data.read_offset += stdata.size();
                    if (data.recv_data.size() > 1) {
                        std::sort(data.recv_data.begin(), data.recv_data.end(), [](auto& a, auto& b) {
                            return a.offset > b.offset;
                        });
                    }
                    while (data.recv_data.size() &&
                           data.read_offset == data.recv_data.back().offset) {
                        auto& src = data.recv_data.back();
                        if (auto err = tls.provide_quic_data(level, src.fragment.data(), src.fragment.size())) {
                            return err;
                        }
                        data.read_offset += src.fragment.size();
                        data.recv_data.pop_back();
                    }
                    if (handshake_complete) {
                        if (auto err = tls.progress_quic()) {
                            return err;
                        }
                    }
                    else {
                        if (is_server) {
                            if (auto err = tls.accept(); err) {
                                if (isTLSBlock(err)) {
                                    return error::none;
                                }
                                return err;  // error
                            }
                        }
                        else {
                            if (auto err = tls.connect(); err) {
                                if (isTLSBlock(err)) {
                                    return error::none;
                                }
                                return err;  // blocked or error
                            }
                        }
                        handshake_complete = true;
                    }
                }
                else {
                    CryptoRecvFragment frag;
                    frag.fragment = make_storage(frame.crypto_data);
                    frag.offset = frame.offset;
                    data.recv_data.push_back(std::move(frag));
                }
                return error::none;
            }

            auto decrypt_callback(std::uint32_t version, auto&& get_largest_pn) {
                return [=, this](DecryptSuite* suite, PacketType type, auto&& packet) -> error::Error {
                    if (!is_CryptoPacket(type)) {
                        return error::Error("invalid packet type");
                    }
                    std::uint32_t curver = version;
                    if constexpr (has_version<decltype(packet)>) {
                        curver = packet.version;
                    }
                    auto& data = this->crypto_data[level_to_index(type_to_level(type))];
                    auto [keys, err] = data.rsecret.keys(curver);
                    if (err) {
                        return err;
                    }
                    suite->keys = keys;
                    suite->cipher = data.rsecret.cipher();
                    suite->largest_pn = get_largest_pn(type);
                    return error::none;
                };
            }

            std::pair<EncryptSuite, error::Error> encrypt_suite(std::uint32_t version, PacketType type) {
                if (!is_CryptoPacket(type)) {
                    return {{}, error::Error("invalid packet type")};
                }
                auto& data = this->crypto_data[level_to_index(type_to_level(type))];
                EncryptSuite suite;
                auto [keys, err] = data.wsecret.keys(version);
                if (err) {
                    return {{}, err};
                }
                suite.keys = keys;
                suite.cipher = data.wsecret.cipher();
                return {suite, error::none};
            }

            bool install_initial_secret(std::uint32_t version, view::rvec clientDstID) {
                auto& data = this->crypto_data[level_to_index(EncryptionLevel::initial)];
                byte client_secret[32], server_secret[32];
                if (!make_initial_secret(client_secret, version, clientDstID, true) ||
                    !make_initial_secret(server_secret, version, clientDstID, false)) {
                    return false;
                }
                if (is_server) {
                    data.wsecret.install(server_secret, {});
                    data.rsecret.install(client_secret, {});
                }
                else {
                    data.wsecret.install(client_secret, {});
                    data.rsecret.install(server_secret, {});
                }
                view::wvec(client_secret).force_fill(0);
                view::wvec(server_secret).force_fill(0);
                return true;
            }

            bool write_installed(PacketType type) {
                if (!is_CryptoPacket(type)) {
                    return false;
                }
                return crypto_data[level_to_index(type_to_level(type))].wsecret.is_installed();
            }

            bool read_installed(PacketType type) {
                if (!is_CryptoPacket(type)) {
                    return false;
                }
                return crypto_data[level_to_index(type_to_level(type))].rsecret.is_installed();
            }

            // drop initial packet
            // client: on first write of handshake packet
            // server: on first read of handshake packet
            bool maybe_drop_initial() {
                auto& data = crypto_data[level_to_index(EncryptionLevel::handshake)];
                if (data.rsecret.is_installed() &&
                    data.wsecret.is_installed() &&
                    read_installed(PacketType::Initial)) {
                    auto& to_discard = crypto_data[level_to_index(EncryptionLevel::initial)];
                    to_discard.rsecret.discard();
                    to_discard.wsecret.discard();
                    return true;
                }
                return false;
            }

            constexpr bool handshake_confirmed() const {
                if (is_server) {
                    return handshake_complete;
                }
                return handshake_done;
            }

            bool maybe_drop_handshake() {
                auto& data = crypto_data[level_to_index(EncryptionLevel::handshake)];
                if (handshake_confirmed() &&
                    (data.wsecret.is_installed() || data.rsecret.is_installed())) {
                    data.wsecret.discard();
                    data.rsecret.discard();
                    return true;
                }
                return false;
            }
        };
    }  // namespace dnet::quic::crypto
}  // namespace utils
