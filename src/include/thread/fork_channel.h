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
           private:
            LiteLock lock_;
            Map<size_t, SendChan<T, Que>> litener;
            size_t index = 0;
            size_t limit = ~0;

           public:
            void subscribe(SendChan<T, Que>&& w, size_t& id) {
                lock_.lock();
                if (w.is_closed()) {
                    return;
                }
                index++;
                SendChan<T, Que>& place = listener[index];
                place = std::move(w);
                place.set_blocking(false);
                id = index;
                lock_.unlock();
            }

            void dispose(size_t id) {
                listener.erase(id);
            }

            void store(T&& t) {
                lock_.lock();
                T copy(std::move(t));
                std::erase_if(listner.begin(), litener.end(), [&](auto& v) {
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
                lock_.unlock();
            }
        };
    }  // namespace thread
}  // namespace utils