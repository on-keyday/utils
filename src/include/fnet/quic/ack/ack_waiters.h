/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "ack_lost_record.h"
#include "../frame/ack.h"
#include "../../std/vector.h"

namespace futils {
    namespace fnet::quic::ack {

        // using ACKWaiters = slib::vector<std::weak_ptr<ACKLostRecord>>;
        using ACKRangeVec = slib::vector<frame::ACKRange>;

    }  // namespace fnet::quic::ack
}  // namespace futils
