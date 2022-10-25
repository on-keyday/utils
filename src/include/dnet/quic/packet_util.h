/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// packet_util - packet utility
#pragma once
#include "../bytelen.h"
#include "packet.h"

namespace utils {
    namespace dnet {
        namespace quic {
            // cb is void(Packet&,bool err,bool valid_type)
            bool parse_packet(ByteLen& b, auto&& cb) {
                if (!b.enough(1)) {
                    return false;
                }
                auto invoke_cb = [&](auto& packet, bool err, bool valid_type) {
                    cb(packet, err, valid_type);
                    return true;
                };
                auto flags = PacketFlags{b.data};
                if (flags.is_long()) {
                    std::uint32_t version = 0;
                    if (!b.forward(1).as(version)) {
                        Packet packet;
                        packet.flags = flags;
                        invoke_cb(packet, true, false);
                        return false;
                    }
                    if (version == 0) {
                        VersionNegotiationPacket verneg;
                        if (!verneg.parse(b)) {
                            invoke_cb(verneg, true, true);
                            return false;
                        }
                        return invoke_cb(verneg, false, true);
                    }
                    auto type = flags.type(version);
                }
            }
        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
