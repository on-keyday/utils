/*license*/

// fork_channel - multi in multi out channel
#pragma once

#include "lite_lock.h"
#include "../wrap/lite/map.h"
#include "channel.h"
#include <algorithm>

namespace utils {
    namespace thread {

        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        struct ForkBuffer {
            LiteLock lock_;
            Map<size_t, SendChan<T, Que>> litenr;
            void store(T&& t) {
                lock_.lock();
                T copy(std::move(t));
                std::erase_if(listner.begin(), litenr.end(), [&](auto& v) {
                    SendChan<T, Que>& c = std::get<1>(v);
                    if (c.is_closed()) {
                        return true;
                    }
                    auto tomov = copy;
                    if ((c << std::move(tomov)) == ChanStateValue::closed) {
                        return true;
                    }
                    return false;
                });
            }
        };
    }  // namespace thread
}  // namespace utils