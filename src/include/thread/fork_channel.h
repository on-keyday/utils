/*license*/

// fork_channel - multi in multi out channel
#pragma once

#include "lite_lock.h"

namespace utils {
    namespace thread {
        struct ForkBuffer {
            LiteLock lock_;
        };
    }  // namespace thread
}  // namespace utils