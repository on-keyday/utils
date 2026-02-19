/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// fork_channel - multi in multi out channel
#pragma once

#include "lite_lock.h"
#include "../wrap/light/map.h"
#include "channel.h"
#include <algorithm>

namespace futils {
    namespace thread {

        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        struct ForkBuffer {
           private:
            LiteLock lock_;
            Map<size_t, SendChan<T, Que>> listener;
            size_t index = 0;
            size_t limit = ~0;
            std::atomic_flag closed;
            BlockLevel blocking = BlockLevel::normal;

           public:
            ForkBuffer(size_t limit)
                : limit(limit) {}

            void change_limit(size_t limit) {
                lock_.lock();
                this->limit = limit;
                lock_.unlock();
            }

            void set_blocking(bool blocking) {
                auto v = blocking ? BlockLevel::force_block : BlockLevel::normal;
                lock_.lock();
                this->blocking = v;
                lock_.unlock();
            }

            void set_blocking(BlockLevel level) {
                lock_.lock();
                this->blocking = level;
                lock_.unlock();
            }

            bool is_closed() const {
                return closed.test();
            }

            bool subscribe(SendChan<T, Que>&& w, size_t& id) {
                lock_.lock();
                if (is_closed()) {
                    lock_.unlock();
                    return false;
                }
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
                return true;
            }

            void dispose(size_t id) {
                lock_.lock();
                listener.erase(id);
                lock_.unlock();
            }

            bool store(T&& t) {
                lock_.lock();
                if (is_closed()) {
                    lock_.unlock();
                    return false;
                }
                T copy(std::move(t));
                std::erase_if(listener, [&](auto& v) {
                    SendChan<T, Que>& c = get<1>(v);
                    if (c.is_closed()) {
                        return true;
                    }
                    auto tomov = copy;
                    c.set_blocking(blocking);
                    if ((c << std::move(tomov)) == ChanStateValue::closed) {
                        return true;
                    }
                    return false;
                });
                lock_.unlock();
                return true;
            }

            void close() {
                if (closed.test_and_set()) {
                    return;
                }
                lock_.lock();
                for (auto& l : listener) {
                    get<1>(l).close();
                }
                listener.clear();
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
                    return buffer->store(std::move(t));
                }
                return false;
            }

            void set_blocking(bool flag) {
                if (buffer) {
                    buffer->set_blocking(flag);
                }
            }

            void set_blocking(BlockLevel level) {
                if (buffer) {
                    buffer->set_blocking(level);
                }
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

            bool is_closed() const {
                if (buffer) {
                    return buffer->is_closed();
                }
                return true;
            }

            void close() {
                if (buffer) {
                    buffer->close();
                }
            }
        };

        template <class T, template <class...> class Que = wrap::queue, template <class...> class Map = wrap::map>
        ForkChan<T, Que, Map> make_forkchan(size_t limit = ~0) {
            auto buf = wrap::make_shared<ForkBuffer<T, Que, Map>>(limit);
            return ForkChan<T, Que, Map>(buf);
        }
    }  // namespace thread
}  // namespace futils
