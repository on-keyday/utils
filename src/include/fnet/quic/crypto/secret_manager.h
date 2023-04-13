/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "secret.h"
#include "update.h"

namespace utils {
    namespace fnet::quic::crypto {
        struct HandshakeSecrets {
           private:
            Secret rsecret;
            HP rhp;
            Secret wsecret;
            HP whp;

           public:
            void reset() {
                rsecret.discard();
                rhp.clear();
                wsecret.discard();
                whp.clear();
            }

            void install_wsecret(view::rvec secret, const tls::TLSCipher& cipher) {
                wsecret.install(secret, cipher);
                whp.clear();
            }

            void install_rsecret(view::rvec secret, const tls::TLSCipher& cipher) {
                rsecret.install(secret, cipher);
                rhp.clear();
            }

            std::pair<EncDecSuite, error::Error> enc_suite(std::uint32_t version) {
                auto [keyiv, err] = wsecret.keyiv(version);
                if (err) {
                    return {{}, err};
                }
                err = wsecret.hp_if_zero(whp, version);
                if (err) {
                    return {{}, err};
                }
                EncDecSuite suite;
                suite.keyiv = keyiv;
                suite.hp = &whp;
                suite.cipher = wsecret.cipher();
                return {suite, error::none};
            }

            std::pair<EncDecSuite, error::Error> dec_suite(std::uint32_t version) {
                auto [keyiv, err] = rsecret.keyiv(version);
                if (err) {
                    return {{}, err};
                }
                err = rsecret.hp_if_zero(rhp, version);
                if (err) {
                    return {{}, err};
                }
                EncDecSuite suite;
                suite.keyiv = keyiv;
                suite.hp = &rhp;
                suite.cipher = rsecret.cipher();
                return {suite, error::none};
            }

            void drop() {
                wsecret.discard();
                whp.clear();
                rsecret.discard();
                rhp.clear();
            }

            constexpr bool is_installed(bool w) const {
                return w ? wsecret.is_installed() : rsecret.is_installed();
            }
        };

        struct SecretManager {
           private:
            HandshakeSecrets initial, handshake, zerortt;
            KeyUpdater onertt;

           public:
            void reset() {
                initial.reset();
                handshake.reset();
                zerortt.reset();
                onertt.reset();
            }

            bool install_initial_secrets(std::uint32_t version, bool is_server, view::rvec clientDstID) {
                byte client_secret[32], server_secret[32];
                if (!make_initial_secret(client_secret, version, clientDstID, true) ||
                    !make_initial_secret(server_secret, version, clientDstID, false)) {
                    return false;
                }
                if (is_server) {
                    initial.install_wsecret(server_secret, {});
                    initial.install_rsecret(client_secret, {});
                }
                else {
                    initial.install_wsecret(client_secret, {});
                    initial.install_rsecret(server_secret, {});
                }
                view::wvec(client_secret).force_fill(0);
                view::wvec(server_secret).force_fill(0);
                return true;
            }

            bool install_rsecret(EncryptionLevel level, view::rvec secret, const tls::TLSCipher& cipher) {
                switch (level) {
                    case EncryptionLevel::handshake:
                        handshake.install_rsecret(secret, cipher);
                        return true;
                    case EncryptionLevel::early_data:
                        zerortt.install_rsecret(secret, cipher);
                    case EncryptionLevel::application:
                        return onertt.install_onertt_rsecret(secret, cipher);
                    default:
                        return false;
                }
            }

            bool install_wsecret(EncryptionLevel level, view::rvec secret, const tls::TLSCipher& cipher) {
                switch (level) {
                    case EncryptionLevel::handshake:
                        handshake.install_wsecret(secret, cipher);
                        return true;
                    case EncryptionLevel::early_data:
                        zerortt.install_rsecret(secret, cipher);
                    case EncryptionLevel::application:
                        return onertt.install_onertt_wsecret(secret, cipher);
                    default:
                        return false;
                }
            }

            std::pair<EncDecSuite, error::Error> enc_suite(std::uint32_t version, PacketType type, KeyMode mode) {
                switch (type) {
                    case PacketType::Initial:
                        return initial.enc_suite(version);
                    case PacketType::ZeroRTT:
                        return zerortt.enc_suite(version);
                    case PacketType::Handshake:
                        return handshake.enc_suite(version);
                    case PacketType::OneRTT:
                        return onertt.enc_suite(version, mode);
                    default:
                        return {{}, error::Error("invalid packet type")};
                }
            }

            std::pair<EncDecSuite, error::Error> dec_suite(PacketType type, std::uint32_t version, KeyMode mode) {
                switch (type) {
                    case PacketType::Initial:
                        return initial.dec_suite(version);
                    case PacketType::ZeroRTT:
                        return zerortt.dec_suite(version);
                    case PacketType::Handshake:
                        return handshake.dec_suite(version);
                    case PacketType::OneRTT:
                        return onertt.dec_suite(version, mode);
                    default:
                        return {{}, error::Error("unexpect packet type for hp")};
                }
            }

            void drop_initial() {
                initial.drop();
            }

            void drop_handshake() {
                handshake.drop();
            }

            bool is_installed(PacketType type, bool w) const {
                switch (type) {
                    case PacketType::Initial:
                        return initial.is_installed(w);
                    case PacketType::ZeroRTT:
                        return zerortt.is_installed(w);
                    case PacketType::Handshake:
                        return handshake.is_installed(w);
                    case PacketType::OneRTT:
                        return onertt.is_installed(w);
                    default:
                        return false;
                }
            }

            bool maybe_use_prev_secret(packetnum::Value pn) const {
                return onertt.maybe_use_prev_secret(pn);
            }

            bool key_update_allowed() const {
                return onertt.key_update_allowed();
            }

            error::Error on_key_update(std::uint32_t version) {
                return onertt.on_key_update(version);
            }

            void on_onertt_packet_received(packetnum::Value pn) {
                onertt.on_packet_received(pn);
            }

            void on_onertt_packet_sent(packetnum::Value pn) {
                onertt.on_packet_sent(pn);
            }
        };
    }  // namespace fnet::quic::crypto
}  // namespace utils
