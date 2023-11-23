/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../frame/path.h"
#include "../frame/writer.h"
#include "../ioresult.h"
#include "../resend/ack_handler.h"
#include "../transport_error.h"
#include "../../std/deque.h"
#include "../../std/set.h"
#include "../../std/list.h"
#include "../../std/hash_map.h"
#include "../status/rtt.h"
#include "../connid/external/random.h"
#include "path.h"

namespace utils {
    namespace fnet::quic::path {

        struct PathProbeInfo {
            PathID id = unknown_path;
            time::Timer probe_timer;
            std::uint64_t probe_data = 0;
            resend::ACKHandler wait;
            bool force_probe = false;
        };

        struct PathVerifier {
           private:
            slib::deque<std::uint64_t> recv_data;

            // PATH_CHALLANGE
            slib::list<PathProbeInfo> probes;
            std::uint64_t num_probe_to_send = 0;

            slib::set<std::uint32_t> valid_path;
            PathID active_path = original_path;
            PathID migrate_path = original_path;

            // using for packet creation time
            PathID writing_path = original_path;

           public:
            void reset(PathID hs_path) {
                recv_data.clear();
                valid_path.clear();
                num_probe_to_send = 0;
                migrate_path = hs_path;
                active_path = hs_path;
                writing_path = hs_path;
            }

            constexpr void set_writing_path_to_active_path() {
                writing_path = active_path;
            }

            constexpr void set_writing_path(PathID id) {
                writing_path = id;
            }

            PathID get_writing_path() const {
                return writing_path;
            }

            void recv_path_challenge(const frame::PathChallengeFrame& resp) {
                recv_data.push_back(resp.data);
            }

            IOResult send_path_response(frame::fwriter& w) {
                if (recv_data.size() == 0) {
                    return IOResult::no_data;
                }
                if (w.remain().size() < 9) {
                    return IOResult::no_capacity;
                }
                frame::PathResponseFrame path;
                path.data = recv_data[0];
                recv_data.pop_front();
                return w.write(path) ? IOResult::ok : IOResult::fatal;
            }

           private:
            bool detect_dead_probe(PathProbeInfo& info, const time::Clock& clock) {
                if (info.probe_timer.timeout(clock)) {
                    if (!info.wait.is_ack() && !info.wait.is_lost()) {
                        return false;
                    }
                    return true;
                }
                if (info.wait.is_lost()) {
                    if (!valid_path.contains(info.id)) {
                        if (!info.force_probe && valid_path.contains(info.id)) {
                            return true;
                        }
                        // add new probe
                        // old probe is alive until PATH_RESPONSE or timeout
                        probes.push_back(PathProbeInfo{
                            .id = info.id,
                            .probe_timer = info.probe_timer,
                        });
                        num_probe_to_send++;  // increment
                    }
                }
                // wait until PATH_RESPONSE or timeout
                return false;
            }

           public:
            void detect_path_probe_timeout(const time::Clock& clock) {
                for (auto it = probes.begin(); it != probes.end();) {
                    if (it->wait.not_confirmed()) {
                        if (detect_dead_probe(*it, clock)) {
                            it = probes.erase(it);
                            continue;
                        }
                        it++;
                        continue;
                    }
                }
            }

            std::pair<IOResult, PathID> send_path_challenge(frame::fwriter& w, auto&& observer, connid::Random& random, time::Time probe_deadline, const time::Clock& clock) {
                for (auto it = probes.begin(); it != probes.end();) {
                    if (it->wait.not_confirmed()) {
                        if (detect_dead_probe(*it, clock)) {
                            it = probes.erase(it);
                            continue;
                        }
                        it++;
                        continue;
                    }
                    if (w.remain().size() < 9) {
                        return {IOResult::no_capacity, unknown_path};
                    }
                    frame::PathChallengeFrame ch;
                    byte data[8]{};
                    if (!random.gen_random(data, connid::RandomUsage::path_challenge)) {
                        return {IOResult::fatal, unknown_path};
                    }
                    binary::reader r{data};
                    binary::read_num(r, it->probe_data);
                    ch.data = it->probe_data;
                    if (!w.write(ch)) {
                        return {IOResult::fatal, unknown_path};
                    }
                    if (it->probe_timer.not_working()) {
                        it->probe_timer.set_deadline(probe_deadline);
                    }
                    it->wait.wait(observer);
                    num_probe_to_send--;  // decrement
                    it++;
                    // detect tail
                    while (it != probes.end()) {
                        if (it->wait.not_confirmed()) {
                            if (detect_dead_probe(*it, clock)) {
                                it = probes.erase(it);
                                continue;
                            }
                        }
                        it++;  // skip probing data
                    }
                    return {IOResult::ok, it->id};
                }
                return {IOResult::no_data, unknown_path};  // no probe needed
            }

            constexpr error::Error recv(const frame::Frame& frame) {
                if (frame.type.type_detail() == FrameType::PATH_CHALLENGE) {
                    recv_path_challenge(static_cast<const frame::PathChallengeFrame&>(frame));
                    return error::none;
                }
                else if (frame.type.type_detail() == FrameType::PATH_RESPONSE) {
                    return recv_path_response(static_cast<const frame::PathResponseFrame&>(frame));
                }
                return error::none;
            }

            void request_path_validation(PathID id) {
                probes.push_back(PathProbeInfo{.id = id});
                num_probe_to_send++;  // increment
            }

            void maybe_peer_migrated_path(PathID id, bool is_non_probe_packet) {
                if (id != active_path) {
                    if (is_non_probe_packet) {
                        migrate_path = id;  // migrated
                    }
                    if (valid_path.contains(id)) {
                        if (is_non_probe_packet) {
                            active_path = id;  // migrate complete
                        }
                        return;  // needless validation
                    }
                    request_path_validation(id);
                }
            }

            error::Error recv_path_response(const frame::PathResponseFrame& f) {
                for (auto it = probes.begin(); it != probes.end(); it++) {
                    if (it->probe_data == f.data) {
                        valid_path.emplace(it->id);
                        if (it->id == migrate_path) {
                            active_path = it->id;  // migration complete
                        }
                        probes.erase(it);
                        return error::none;
                    }
                }
                // TODO(on-keyday): when we implement recv packet handler,
                //                  this does not detect sprious PATH_RESPONSE
                //                  but currently, do
                return QUICError{
                    .msg = "PathResponse contains not issued data",
                    .transport_error = TransportError::PROTOCOL_VIOLATION,
                    .frame_type = FrameType::PATH_RESPONSE,
                };
            }

            bool has_probe_to_send() const {
                return num_probe_to_send != 0;
            }

            bool non_probe_is_allowed() const {
                return migrate_path == active_path;
            }
        };
    }  // namespace fnet::quic::path
}  // namespace utils
