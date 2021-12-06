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
            ForkBuffer(size_t limit)
                : limit(limit) {}

            void change_limit(size_t limit) {
                lock_.lock();
                this->limit = limit;
                lock_.unlock();
            }

            bool subscribe(SendChan<T, Que>&& w, size_t& id) {
                lock_.lock();
                if (w.is_closed()) {
                    lock_.unlock();
                    return false;
                }
                if (listener.size() >= limit) {
                    lock_.unlock();
                    return false;
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

        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        struct Subscriber {
           private:
            wrap::shared_ptr<ForkBuffer<T, Que, Map>> buffer;
            size_t id = 0;

           public:
            Subscriber(size_t id, wrap::shared_ptr<ForkBuffer<T, Que, Map>>& buf)
                : id(id), buffer(buf) {}

            ~Subscriber() {
                if (buffer) {
                    buffer->dispose(id);
                }
            }
        };

        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        struct ForkChan {
           private:
            wrap::shared_ptr<ForkBuffer<T, Que, Map>> buffer;

           public:
            ForkChan(wrap::shared_ptr<ForkBuffer<T, Que, Map>>& buf)
                : buffer(buf) {}

            bool operator<<(T&& t) {
                if (buffer) {
                    buffer->store(std::move(t));
                    return true;
                }
                return false;
            }

            void change_limit(size_t limit) {
                if (buffer) {
                    buffer->change_limit(limit);
                }
            }

            [[nodiscard]] wrap::shared_ptr<Subscriber<T, Que, Map>> subscribe(SendChan<T, Que>&& w) {
                if (buffer) {
                    size_t id;
                    if (!buffer->subscribe(std::move(w), id)) {
                        return nullptr;
                    }
                    return wrap::make_shared<Subscriber<T, Que, Map>>(id, buffer);
                }
                return nullptr;
            }
        };
        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        ForkChan<T, Que, Map> make_forkchan(size_t limit = ~0) {
            auto buf = wrap::make_shared<ForkBuffer<T, Que, Map>>(limit);
            return ForkChan<T, Que, Map>(buf);
        }
    }  // namespace thread
}  // namespace utils
