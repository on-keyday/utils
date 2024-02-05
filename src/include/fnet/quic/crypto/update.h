/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "secret.h"
#include "crypto.h"
#include "encdec_suite.h"

namespace futils {
    namespace fnet::quic::crypto {

        enum class KeyMode {
            prev,
            current,
            next,
        };

        constexpr bool key_bit(std::uint64_t key_phase) noexcept {
            return key_phase % 2 == 1;
        }

        struct KeyUpdater {
           private:
            HP whp;
            HP rhp;
            Secret prev_rsecret;
            Secret cur_rsecret;
            Secret next_rsecret;
            Secret cur_wsecret;
            Secret next_wsecret;
            std::uint64_t key_phase = 0;

            packetnum::Value first_sent = packetnum::infinity;
            std::uint64_t num_sent = 0;
            packetnum::Value first_recv = packetnum::infinity;
            std::uint64_t num_recv = 0;
            packetnum::Value highest_recv = packetnum::infinity;

           public:
            void reset() {
                prev_rsecret.discard();
                cur_rsecret.discard();
                next_rsecret.discard();
                cur_wsecret.discard();
                next_wsecret.discard();
                key_phase = 0;
                first_sent = packetnum::infinity;
                num_sent = 0;
                first_recv = packetnum::infinity;
                num_recv = 0;
                highest_recv = packetnum::infinity;
                whp.clear();
                rhp.clear();
            }

            bool install_onertt_rsecret(view::rvec secret, const tls::TLSCipher& cipher) {
                if (key_phase != 0 || cur_rsecret.is_installed()) {
                    return false;
                }
                cur_rsecret.install(secret, cipher);
                return true;
            }

            bool install_onertt_wsecret(view::rvec secret, const tls::TLSCipher& cipher) {
                if (key_phase != 0 || cur_wsecret.is_installed()) {
                    return false;
                }
                cur_wsecret.install(secret, cipher);
                return true;
            }

            constexpr bool is_installed(bool w) const {
                return w ? cur_wsecret.is_installed() : cur_rsecret.is_installed();
            }

            error::Error prepare_next_keys(std::uint32_t version) {
                if (next_wsecret.is_installed() && next_rsecret.is_installed()) {
                    return error::none;
                }
                byte tmp[32]{};
                auto buf = view::wvec(tmp);
                const auto f = helper::defer([&] {
                    buf.force_fill(0);
                });
                auto [wupdated, werr] = cur_wsecret.update(buf, version);
                if (werr) {
                    return werr;
                }
                next_wsecret.install(wupdated, *cur_wsecret.cipher());
                auto [rupdated, rerr] = cur_rsecret.update(buf, version);
                if (rerr) {
                    return rerr;
                }
                next_rsecret.install(rupdated, *cur_rsecret.cipher());
                return error::none;
            }

            error::Error on_key_update(std::uint32_t version) {
                if (auto err = prepare_next_keys(version)) {
                    return err;
                }
                prev_rsecret = std::move(cur_rsecret);
                cur_rsecret = std::move(next_rsecret);
                cur_wsecret = std::move(next_wsecret);
                num_sent = 0;
                num_recv = 0;
                first_recv = packetnum::infinity;
                first_sent = packetnum::infinity;
                key_phase++;
                return error::none;
            }

            bool maybe_use_prev_secret(packetnum::Value pn) const {
                return key_phase > 0 && first_recv == packetnum::infinity ||
                       pn < first_recv;
            }

            bool key_update_allowed() const {
                return key_phase == 0 || first_sent != packetnum::infinity;
            }

            void on_packet_sent(packetnum::Value pn) {
                if (first_sent == packetnum::infinity) {
                    first_sent = pn;
                }
                num_sent++;
            }

            void on_packet_received(packetnum::Value pn) {
                if (first_recv == packetnum::infinity) {
                    first_recv = pn;
                }
                num_recv++;
                if (pn > highest_recv) {
                    highest_recv = pn;
                }
            }

            std::pair<EncDecSuite, error::Error> enc_suite(std::uint32_t version, KeyMode mode) {
                EncDecSuite suite;
                error::Error err;
                std::uint64_t phase;
                switch (mode) {
                    case KeyMode::current:
                        std::tie(suite.keyiv, err) = cur_wsecret.keyiv(version);
                        phase = key_phase;
                        break;
                    case KeyMode::next:
                        err = prepare_next_keys(version);
                        if (err) {
                            return {{}, std::move(err)};
                        }
                        std::tie(suite.keyiv, err) = next_wsecret.keyiv(version);
                        phase = key_phase + 1;
                        break;
                    default:
                        return {{}, error::Error("invalid key mode for encryption suite", error::Category::lib, error::fnet_quic_implementation_bug)};
                }
                if (err) {
                    return {{}, std::move(err)};
                }
                if (key_phase == 0) {
                    err = cur_wsecret.hp_if_zero(whp, version);
                    if (err) {
                        return {{}, std::move(err)};
                    }
                }
                suite.hp = &whp;
                suite.cipher = cur_wsecret.cipher();
                suite.phase = key_bit(phase) ? KeyPhase::one : KeyPhase::zero;
                return {suite, error::none};
            }

            std::pair<EncDecSuite, error::Error> dec_suite(std::uint32_t version, KeyMode mode) {
                EncDecSuite suite;
                error::Error err;
                std::uint64_t phase;
                switch (mode) {
                    case KeyMode::prev:
                        std::tie(suite.keyiv, err) = prev_rsecret.keyiv(version);
                        phase = key_phase - 1;
                        break;
                    case KeyMode::current:
                        std::tie(suite.keyiv, err) = cur_rsecret.keyiv(version);
                        phase = key_phase;
                        break;
                    case KeyMode::next:
                        err = prepare_next_keys(version);
                        if (err) {
                            return {{}, std::move(err)};
                        }
                        std::tie(suite.keyiv, err) = next_rsecret.keyiv(version);
                        phase = key_phase + 1;
                        break;
                    default:
                        return {{}, error::Error("invalid key mode for encryption suite", error::Category::lib, error::fnet_quic_implementation_bug)};
                }
                if (err) {
                    return {{}, std::move(err)};
                }
                if (key_phase == 0) {
                    err = cur_rsecret.hp_if_zero(rhp, version);
                    if (err) {
                        return {{}, std::move(err)};
                    }
                }
                suite.hp = &rhp;
                suite.cipher = cur_rsecret.cipher();
                suite.phase = key_bit(phase) ? KeyPhase::one : KeyPhase::zero;
                return {suite, error::none};
            }
        };
    }  // namespace fnet::quic::crypto
}  // namespace futils
