/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "ack_lost_record.h"
#include "../frame/ack.h"
#include "../../std/vector.h"

namespace utils {
    namespace fnet::quic::ack {

        // using ACKWaiters = slib::vector<std::weak_ptr<ACKLostRecord>>;
        using ACKRangeVec = slib::vector<frame::ACKRange>;

    }  // namespace fnet::quic::ack
}  // namespace utils
