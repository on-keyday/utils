/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "pn_space.h"
#include "pto.h"
#include "handshake.h"
#include "rtt.h"
#include "pn_manage.h"

namespace utils {
    namespace fnet::quic::status {
        enum class LossTimerState {
            no_timer,
            wait_for_loss,
            at_anti_amplification_limit,
            wait_for_pto,
        };

        constexpr const char* to_string(LossTimerState state) {
            switch (state) {
                case LossTimerState::no_timer:
                    return "no timer";
                case LossTimerState::wait_for_loss:
                    return "wait for loss";
                case LossTimerState::at_anti_amplification_limit:
                    return "at anti amplification limit";
                case LossTimerState::wait_for_pto:
                    return "wait for pto";
            }
        }

        enum class LostReason {
            not_lost,
            time_threshold,
            packet_order_threshold,
            invalid,
        };

        constexpr bool no_ack_eliciting_in_flight(const PacketNumberIssuer (&issuers)[3]) {
            return issuers[0].no_ack_eliciting_in_flight() &&
                   issuers[1].no_ack_eliciting_in_flight() &&
                   issuers[2].no_ack_eliciting_in_flight();
        }

        struct LossTimer {
           private:
            time::Time loss_time[3]{};
            time::Timer timer;
            LossTimerState state = LossTimerState::no_timer;
            PacketNumberSpace timer_space = PacketNumberSpace::no_space;

            constexpr std::pair<time::Time, PacketNumberSpace> get_loss_time_and_space() const {
                PacketNumberSpace space;
                time::Time time = 0;
                for (auto i = 0; i < 3; i++) {
                    if (time == 0 || loss_time[i] < time) {
                        time = loss_time[i];
                        space = PacketNumberSpace(i);
                    }
                }
                return {time, space};
            }

            static constexpr std::pair<time::Time, PacketNumberSpace> get_pto_and_space(
                const InternalConfig& config, const HandshakeStatus& hs,
                const PTOStatus& pto, const RTT& rtt, const PacketNumberIssuer (&issuers)[3]) {
                auto duration = pto.probe_timeout_duration(config, rtt);
                if (duration < 0) {
                    return {time::invalid, PacketNumberSpace::initial};
                }
                if (no_ack_eliciting_in_flight(issuers)) {
                    if (hs.peer_completed_address_validation()) {
                        return {time::invalid, PacketNumberSpace::initial};
                    }
                    if (hs.handshake_packet_is_sent()) {
                        return {config.clock.now() + duration, PacketNumberSpace::handshake};
                    }
                    else {
                        return {config.clock.now() + duration, PacketNumberSpace::initial};
                    }
                }
                time::Time pto_timeout = time::invalid;
                PacketNumberSpace pto_space = PacketNumberSpace::no_space;
                for (auto i = 0; i < 3; i++) {
                    auto& issuer = issuers[i];
                    if (issuer.no_ack_eliciting_in_flight()) {
                        continue;
                    }
                    if (i == space_to_index(PacketNumberSpace::application)) {
                        if (!hs.handshake_confirmed()) {
                            return {pto_timeout, pto_space};
                        }
                        duration += rtt.max_ack_delay() * pto.pto_exponent();
                    }
                    auto t = issuer.last_ack_eliciting_packet_sent_time() + duration;
                    if (pto_timeout == time::invalid || t < pto_timeout) {
                        pto_timeout = t;
                        pto_space = PacketNumberSpace(i);
                    }
                }
                return {pto_timeout, pto_space};
            }

           public:
            constexpr void reset() {
                loss_time[0] = 0;
                loss_time[1] = 0;
                loss_time[2] = 0;
                timer.cancel();
                timer_space = PacketNumberSpace::no_space;
                state = LossTimerState::no_timer;
            }

            constexpr LossTimerState current_state() const {
                return state;
            }

            constexpr PacketNumberSpace current_space() const {
                return timer_space;
            }

            constexpr bool timeout(const InternalConfig& config) const {
                return timer.timeout(config.clock);
            }

            constexpr time::Time get_deadline() const {
                return timer.get_deadline();
            }

            constexpr void reset_loss_time(PacketNumberSpace space) {
                loss_time[space_to_index(space)] = 0;
            }

            constexpr void update_loss_time(PacketNumberSpace space, time::Time time) {
                auto& loss = loss_time[space_to_index(space)];
                if (loss == 0 || time < loss) {
                    loss = time;
                }
            }

            constexpr void on_packet_number_space_discard(PacketNumberSpace space) {
                reset_loss_time(space);
            }

            constexpr void on_retry_received() {
                reset();
            }

            constexpr void set_loss_detection_timer(const InternalConfig& config, const PTOStatus& pto, const RTT& rtt,
                                                    const HandshakeStatus& hs, const PacketNumberIssuer (&issuers)[3],
                                                    time::Timer& ping_timer) {
                auto set_ping = [&] {
                    if (config.ping_duration != 0) {
                        ping_timer.set_timeout(config.clock, config.clock.to_clock_granurarity(config.ping_duration));
                    }
                };
                auto [earliest_loss_time, space_1] = get_loss_time_and_space();
                if (earliest_loss_time != 0) {
                    timer.set_deadline(earliest_loss_time);
                    state = LossTimerState::wait_for_loss;
                    timer_space = space_1;
                    ping_timer.cancel();
                    return;
                }
                if (hs.is_at_anti_amplification_limit()) {
                    timer.cancel();
                    state = LossTimerState::at_anti_amplification_limit;
                    timer_space = PacketNumberSpace::no_space;
                    ping_timer.cancel();
                    return;
                }
                if (no_ack_eliciting_in_flight(issuers) &&
                    hs.peer_completed_address_validation()) {
                    timer.cancel();
                    state = LossTimerState::no_timer;
                    timer_space = PacketNumberSpace::no_space;
                    set_ping();
                    return;
                }
                auto [timeout, space_2] = get_pto_and_space(config, hs, pto, rtt, issuers);
                if (timeout == time::invalid) {
                    timer.cancel();
                    state = LossTimerState::no_timer;
                    timer_space = PacketNumberSpace::no_space;
                    set_ping();
                    return;
                }
                timer.set_deadline(timeout);
                state = LossTimerState::wait_for_pto;
                timer_space = space_2;
            }
        };
    }  // namespace fnet::quic::status
}  // namespace utils
