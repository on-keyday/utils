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
#include "../../../helper/condincr.h"
#include "../packet/summary.h"
#include "../../std/list.h"
#include "../../std/vector.h"
#include "../../error.h"

namespace utils {
    namespace dnet::quic::event {
        enum class Status {
            discard,  // discard from list
            reorder,  // reorder
            fatal,    // can't continue connection
        };

        using ACKWaitVec = slib::vector<std::weak_ptr<ack::ACKLostRecord>>;

        enum class priority : byte {
            conn_close = 0,
            ping = 1,
            ack = 2,
            crypto = 3,
            conn_id = 4,
            highest = 10,
            normal = 25,
            lowest = 255,
        };

        constexpr auto operator<=>(const priority& a, const priority& b) noexcept {
            return byte(a) <=> byte(b);
        }

        struct FrameWriteEvent {
            priority prio = priority::normal;
            std::weak_ptr<void> arg;
            std::pair<Status, error::Error> (*on_event)(const packet::PacketSummary&, ACKWaitVec& vec, frame::fwriter& w, priority& prio, std::shared_ptr<void>& arg);
        };

        struct FrameRecvEvent {
            std::weak_ptr<void> arg;
            std::pair<Status, error::Error> (*on_event)(const packet::PacketSummary&, frame::Frame& f, std::shared_ptr<void>& arg);
        };

        // TODO(on-keyday): to reduce waste callback call,
        // use map for read list and write list
        struct EventList {
            using WriteList = slib::list<FrameWriteEvent>;
            using ReadList = slib::list<FrameRecvEvent>;
            WriteList on_write;
            ReadList on_recv;
            error::Error send(const packet::PacketSummary& packet, ACKWaitVec& vec, frame::fwriter& w) {
                on_write.sort([](auto&& a, auto&& b) { return a.prio < b.prio; });
                bool rem = false;
                auto incr = helper::cond_incr(rem);
                for (auto it = on_write.begin(); it != on_write.end(); incr(it)) {
                    if (!it->on_event) {
                        it = on_write.erase(it);
                        rem = true;
                        continue;
                    }
                    auto arg = it->arg.lock();
                    if (!arg) {
                        it = on_write.erase(it);
                        rem = true;
                        continue;
                    }
                    auto [status, err] = it->on_event(packet, vec, w, it->prio, arg);
                    if (status == Status::fatal) {
                        return err ? err : error::Error("unknown fatal error");
                    }
                    if (status == Status::discard) {
                        it = on_write.erase(it);
                        rem = true;
                    }
                }
                return error::none;
            }

            error::Error recv(const packet::PacketSummary& packet, frame::Frame& frame) {
                bool rem = false;
                auto incr = helper::cond_incr(rem);
                for (auto it = on_recv.begin(); it != on_recv.end(); incr(it)) {
                    if (!it->on_event) {
                        it = on_recv.erase(it);
                        rem = true;
                        continue;
                    }
                    auto arg = it->arg.lock();
                    if (!arg) {
                        it = on_recv.erase(it);
                        rem = true;
                        continue;
                    }
                    auto [status, err] = it->on_event(packet, frame, arg);
                    if (status == Status::fatal) {
                        return err;
                    }
                    if (status == Status::discard) {
                        it = on_recv.erase(it);
                        rem = true;
                    }
                }
                return error::none;
            }
        };

    }  // namespace dnet::quic::event
}  // namespace utils
