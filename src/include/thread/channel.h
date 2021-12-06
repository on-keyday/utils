/*license*/

// channel - channel between thread
#pragma once

#include "lite_lock.h"
#include "../wrap/lite/queue.h"
#include "../wrap/lite/enum.h"

namespace utils {
    namespace thread {

        enum class ChanStateValue {
            enable,
            closed,
            empty,
            full,
        };

        enum class ChanDisposePolicy {
            dispose_new,
            dispose_back,
            dispose_front,
        };

        using ChanState = wrap::EnumWrap<ChanStateValue, ChanStateValue::enable, ChanStateValue::closed>;

        template <class T, template <class...> class Que = wrap::queue>
        struct ChanBuffer {
            Que<T> que;
            size_t limit = ~0;
            LiteLock lock_;
            ChanDisposePolicy policy = ChanDisposePolicy::dispose_new;

            ChanState store(T&& t) {
                lock_.lock();
                if (que.size() >= limit) {
                    if (policy == ChanDisposePolicy::dispose_front) {
                        que.pop_front();
                    }
                    else if (policy == ChanDisposePolicy::dispose_back) {
                        que.pop_back();
                    }
                    else {
                        return ChanStateValue::full;
                    }
                }
            }
        };
    }  // namespace thread
}  // namespace utils