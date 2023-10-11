/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// suite - crypto suite
#pragma once
#include "handshaker.h"
#include "update.h"
#include "secret_manager.h"
#include "../packet/crypto.h"

namespace utils {
    namespace fnet::quic::crypto {
        template <class T>
        concept has_version = requires(T t) {
            { t.version };
        };

        struct CryptoSuite {
           private:
            friend int qtls_callback(CryptoSuite* suite, MethodArgs args);
            CryptoHandshakers handshaker;
            SecretManager secrets;

           public:
            tls::TLS& tls() {
                return handshaker.get_tls();
            }

            void reset(tls::TLS&& new_tls, bool is_server) {
                handshaker.reset(std::move(new_tls), is_server);
                secrets.reset();
            }

            IOResult send(PacketType type, packetnum::Value pn, auto&& observer, frame::fwriter& w) {
                return handshaker.send(type, w, observer);
            }

            error::Error recv(PacketType type, const frame::Frame& frame) {
                return handshaker.recv(type, frame);
            }

           private:
            error::Error decrypt_onertt(std::uint32_t version, EncDecSuite& dec, packet::CryptoPacket& packet) {
                auto phase = PacketFlags{packet.src[0]}.key_phase() ? KeyPhase::one : KeyPhase::zero;
                error::Error err;
                KeyMode mode = KeyMode::current;
                if (dec.pharse != phase) {
                    if (secrets.maybe_use_prev_secret(packet.packet_number)) {
                        std::tie(dec, err) = secrets.dec_suite(PacketType::OneRTT, version, KeyMode::prev);
                        if (err) {
                            return err;
                        }
                        mode = KeyMode::prev;
                    }
                    else {
                        std::tie(dec, err) = secrets.dec_suite(PacketType::OneRTT, version, KeyMode::next);
                        if (err) {
                            return err;
                        }
                        mode = KeyMode::next;
                    }
                }
                err = decrypt_payload(*dec.keyiv, *dec.cipher, packet);
                if (err) {
                    return err;
                }
                if (mode == KeyMode::next) {
                    if (!secrets.key_update_allowed()) {
                        return QUICError{
                            .msg = "key update not allowed",
                            .transport_error = TransportError::KEY_UPDATE_ERROR,
                        };
                    }
                    err = secrets.on_key_update(version);
                    if (err) {
                        return err;
                    }
                }
                else if (mode == KeyMode::current) {
                    secrets.on_onertt_packet_received(packet.packet_number);
                }
                return error::none;
            }

           public:
            auto decrypt_callback(std::uint32_t version, auto&& get_largest_pn) {
                return [=, this](PacketType type, auto&& base, packet::CryptoPacket& packet) -> error::Error {
                    if (handshaker.is_server() && type == PacketType::Initial &&
                        !secrets.is_installed(PacketType::Initial, false)) {
                        view::rvec dstID = base.dstID;
                        if (!secrets.install_initial_secrets(version, true, dstID)) {
                            return error::Error("failed to generate initial secret", error::Category::lib, error::fnet_quic_implementation_bug);
                        }
                    }
                    auto [dec, err] = secrets.dec_suite(type, version, KeyMode::current);
                    if (err) {
                        return err;
                    }
                    auto largest_pn = get_largest_pn(type);
                    err = decrypt_header(*dec.hp, *dec.cipher, packet, largest_pn);
                    if (err) {
                        return err;
                    }
                    if (type == PacketType::OneRTT) {
                        err = decrypt_onertt(version, dec, packet);
                    }
                    else {
                        err = decrypt_payload(*dec.keyiv, *dec.cipher, packet);
                    }
                    if (err) {
                        return err;
                    }
                    return error::none;
                };
            }

            std::pair<EncDecSuite, error::Error> enc_suite(std::uint32_t version, PacketType type, KeyMode mode) {
                return secrets.enc_suite(version, type, mode);
            }

            // called when
            bool install_initial_secret(std::uint32_t version, view::rvec clientDstID) {
                return secrets.install_initial_secrets(version, handshaker.is_server(), clientDstID);
            }

            bool write_installed(PacketType type) const {
                return secrets.is_installed(type, true);
            }

            bool read_installed(PacketType type) const {
                return secrets.is_installed(type, false);
            }

            // drop initial secret
            // client: on first write of handshake packet
            // server: on first read of handshake packet
            bool maybe_drop_initial() {
                if (read_installed(PacketType::Handshake) &&
                    write_installed(PacketType::Handshake) &&
                    read_installed(PacketType::Initial)) {
                    secrets.drop_initial();
                    return true;
                }
                return false;
            }

            constexpr bool handshake_confirmed() const {
                return handshaker.handshake_confirmed();
            }

            constexpr bool handshake_complete() const {
                return handshaker.handshake_complete();
            }

            // drop handshake secret
            // client/server: on handshake confirmed
            bool maybe_drop_handshake() {
                if (handshake_confirmed() &&
                    (write_installed(PacketType::Handshake) ||
                     read_installed(PacketType::Handshake))) {
                    secrets.drop_handshake();
                    return true;
                }
                return false;
            }

            // drop 0-RTT secret
            // client: on write key of 1-RTT is installed
            // server: on read key of 1-RTT is installed, or after a moment from 1-RTT read key installation.
            bool maybe_drop_0rtt() {
                if ((handshaker.is_server()
                         ? read_installed(PacketType::OneRTT)
                         : write_installed(PacketType::OneRTT)) &&
                    (write_installed(PacketType::ZeroRTT) ||
                     read_installed(PacketType::ZeroRTT))) {
                    secrets.drop_0rtt();
                    return true;
                }
                return false;
            }

            void on_packet_sent(PacketType type, packetnum::Value pn) {
                if (type == PacketType::OneRTT) {
                    secrets.on_onertt_packet_sent(pn);
                }
            }
        };
    }  // namespace fnet::quic::crypto
}  // namespace utils
