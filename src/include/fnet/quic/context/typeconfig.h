/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// typeconfig - typeconfig of context::Context
#pragma once
#include "../ack/recv_history.h"
#include "../ack/sent_history.h"
#include "../stream/typeconfig.h"
#include "../connid/typeconfig.h"
#include "../dgram/datagram.h"

namespace utils {
    namespace fnet::quic::context {
        template <class CtxLock,
                  class StreamTypeConfig = stream::TypeConfig<CtxLock>,
                  class ConnIDTypeConfig = connid::TypeConfig<>,
                  class CongestionAlgorithm = status::NewReno,
                  class RecvPacketHistory = ack::RecvPacketHistory,
                  class SentPacketHistory = ack::SentPacketHistory,
                  class DatagramDrop = datagram::DatagramDropNull,
                  template <class...> class VersionsVec = slib::vector>
        struct TypeConfig {
            using context_lock = CtxLock;
            using stream_type_config = StreamTypeConfig;
            using connid_type_config = ConnIDTypeConfig;
            using congestion_algorithm = CongestionAlgorithm;
            using recv_packet_history = RecvPacketHistory;
            using sent_packet_history = SentPacketHistory;
            using datagram_drop = DatagramDrop;
            struct user_defined_types_config {
                congestion_algorithm algorithm;
                datagram_drop dgram_drop;
            };
            template <class T>
            using versions_vec = VersionsVec<T>;
        };
    }  // namespace fnet::quic::context
}  // namespace utils
