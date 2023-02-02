/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// frame_event - event system implementation
#pragma once
#include <memory>
#include "../../../io/writer.h"
#include "../frame/base.h"
#include "../packet_number.h"
#include "../ack/ack_lost_record.h"
#include "../packet/summary.h"
#include "../../std/list.h"
#include "../../std/vector.h"
#include "../../error.h"
#include "../frame/writer.h"

namespace utils {
    namespace dnet::quic::event {
        enum class Status {
            reorder,  // reorder
            fatal,    // can't continue connection
        };

        using ACKWaitVec = slib::vector<std::weak_ptr<ack::ACKLostRecord>>;

        enum class kind : byte {
            ack = 0,
            crypto = 1,
            conn_id = 2,
            streams = 3,
        };

        template <class Event>
        struct Events {
            static constexpr size_t num_events = 4;
            Event events[num_events];

            Event& operator[](kind k) {
                return events[int(k)];
            }

            Event* begin() {
                return events;
            }

            Event* end() {
                return events + num_events;
            }
        };

        struct FrameWriteEvent {
            std::weak_ptr<void> arg;
            std::pair<Status, error::Error> (*on_event)(const packet::PacketSummary&, ACKWaitVec& vec, frame::fwriter& w, std::shared_ptr<void>& arg);
        };

        struct FrameRecvEvent {
            std::weak_ptr<void> arg;
            std::pair<Status, error::Error> (*on_event)(const packet::PacketSummary&, frame::Frame& f, std::shared_ptr<void>& arg);
        };

        struct EventList {
            Events<FrameWriteEvent> on_write;
            Events<FrameRecvEvent> on_read;

            error::Error send(const packet::PacketSummary& packet, ACKWaitVec& vec, frame::fwriter& w, bool only_ack) {
                for (auto& event : on_write) {
                    if (!event.on_event) {
                        continue;
                    }
                    auto arg = event.arg.lock();
                    if (!arg) {
                        continue;
                    }
                    auto [status, err] = event.on_event(packet, vec, w, arg);
                    if (status == Status::fatal) {
                        return err ? err : error::Error("unknown fatal error");
                    }
                }
                return error::none;
            }

            error::Error recv(const packet::PacketSummary& packet, frame::Frame& frame) {
                for (auto& event : on_read) {
                    if (!event.on_event) {
                        continue;
                    }
                    auto arg = event.arg.lock();
                    if (!arg) {
                        continue;
                    }
                    auto [status, err] = event.on_event(packet, frame, arg);
                    if (status == Status::fatal) {
                        return err ? err : error::Error("unknown fatal error");
                    }
                }
                return error::none;
            }
        };

    }  // namespace dnet::quic::event
}  // namespace utils
