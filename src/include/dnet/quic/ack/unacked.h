/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// unacked - unacked packet numbers
#pragma once
#include "../../std/vector.h"
#include "../packet_number.h"
#include "../frame/ack.h"
#include "pn_space.h"

namespace utils {
    namespace dnet::quic::ack {

        struct UnackedPacket {
            slib::vector<packetnum::Value> unacked[3]{};
            ACKRangeVec buffer;

            constexpr ACKRangeVec* gen_ragnes_from_recved(PacketNumberSpace space) {
                buffer.clear();
                auto& recved = unacked[space_to_index(space)];
                const auto size = recved.size();
                if (size == 0) {
                    return nullptr;
                }
                if (size == 1) {
                    auto res = recved[0].value;
                    buffer.push_back(frame::ACKRange{res, res});
                    return &buffer;
                }
                std::sort(recved.begin(), recved.end(), std::greater<>{});
                frame::ACKRange cur;
                cur.largest = size_t(recved[0]);
                cur.smallest = cur.largest;
                for (size_t i = 1; i < size; i++) {
                    if (recved[i - 1] == recved[i] + 1) {
                        cur.smallest = recved[i];
                    }
                    else {
                        buffer.push_back(std::move(cur));
                        cur.largest = recved[i];
                        cur.smallest = cur.largest;
                    }
                }
                buffer.push_back(std::move(cur));
                return &buffer;
            }

            void clear(PacketNumberSpace space) {
                auto& recved = unacked[space_to_index(space)];
                recved.clear();
            }

            void add(PacketNumberSpace space, packetnum::Value val) {
                unacked[space_to_index(space)].push_back(val);
            }
        };
    }  // namespace dnet::quic::ack
}  // namespace utils
