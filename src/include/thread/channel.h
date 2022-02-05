/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// channel - channel between threads
#pragma once

#include "lite_lock.h"
#include "../wrap/lite/queue.h"
#include "../wrap/lite/enum.h"
#include "../wrap/lite/smart_ptr.h"

namespace utils {
    namespace thread {

        enum class ChanStateValue {
            enable,
            closed,
            empty,
            full,
            blocked,
        };

        enum class ChanDisposePolicy {
            dispose_new,
            dispose_back,
            dispose_front,
        };

        using ChanState = wrap::EnumWrap<ChanStateValue, ChanStateValue::enable, ChanStateValue::closed>;

        template <class T, template <class...> class Que = wrap::queue>
        struct ChanBuffer {
           private:
            Que<T> que;
            size_t limit = ~0;
            LiteLock lock_;
            std::atomic_flag closed;
            LiteLock read_blocking;
            LiteLock write_blocking;
            ChanDisposePolicy policy = ChanDisposePolicy::dispose_new;

            bool check_limit() {
                if (limit == 0) {
                    return false;
                }
                while (que.size() >= limit) {
                    if (policy == ChanDisposePolicy::dispose_front) {
                        que.pop_front();
                    }
                    else if (policy == ChanDisposePolicy::dispose_back) {
                        que.pop_back();
                    }
                    else {
                        return false;
                    }
                }
                return true;
            }

            void unlock_ioblocking() {
                write_blocking.unlock();
                read_blocking.unlock();
            }

            bool check_close() {
                if (closed.test()) {
                    unlock_ioblocking();
                    return false;
                }
                return true;
            }

            ChanState unlock_load(T& t) {
                if (!check_close()) {
                    return ChanStateValue::closed;
                }
                if (que.size() == 0) {
                    return ChanStateValue::empty;
                }
                t = std::move(que.front());
                que.pop_front();
                write_blocking.unlock();
                return true;
            }

            ChanState unlock_store(T&& t) {
                if (!check_close()) {
                    return ChanStateValue::closed;
                }
                if (!check_limit()) {
                    return ChanStateValue::full;
                }
                que.push_back(std::move(t));
                read_blocking.unlock();
                return true;
            }

           public:
            ChanBuffer(size_t limit = ~0, ChanDisposePolicy policy = ChanDisposePolicy::dispose_new)
                : limit(limit), policy(policy) {
            }

            void change_limit(size_t limit) {
                lock_.lock();
                this->limit = limit;
                unlock_ioblocking();
                lock_.unlock();
            }

            void change_policy(ChanDisposePolicy policy) {
                lock_.lock();
                this->policy = policy;
                unlock_ioblocking();
                lock_.unlock();
            }

            ChanState try_store(T&& t) {
                if (lock_.try_lock()) {
                    auto res = unlock_store(std::move(t));
                    lock_.unlock();
                    return res;
                }
                return ChanStateValue::blocked;
            }

            ChanState try_load(T& t) {
                if (lock_.try_lock()) {
                    auto res = unlock_load(t);
                    lock_.unlock();
                    return res;
                }
                return ChanStateValue::blocked;
            }

            ChanState store(T&& t) {
                lock_.lock();
                auto res = unlock_store(std::move(t));
                lock_.unlock();
                return res;
            }

            ChanState load(T& t) {
                lock_.lock();
                auto res = unlock_load(t);
                lock_.unlock();
                return res;
            }

            ChanState blocking_store(T&& t) {
                while (true) {
                    lock_.lock();
                    auto res = unlock_store(std::move(t));
                    if (res == ChanStateValue::full) {
                        write_blocking.flag.test_and_set();
                        lock_.unlock();
                        write_blocking.flag.wait(true);
                        continue;
                    }
                    lock_.unlock();
                    return res;
                }
            }

            ChanState blocking_load(T& t) {
                while (true) {
                    lock_.lock();
                    auto res = unlock_load(t);
                    if (res == ChanStateValue::empty) {
                        read_blocking.flag.test_and_set();
                        lock_.unlock();
                        read_blocking.flag.wait(true);
                        continue;
                    }
                    lock_.unlock();
                    return res;
                }
            }

            bool close() {
                if (closed.test_and_set()) {
                    return false;
                }
                unlock_ioblocking();
                lock_.unlock();
                return true;
            }

            bool is_closed() const {
                return closed.test();
            }
        };

        enum class BlockLevel {
            normal,
            force_block,
            mustnot,
        };

        template <class T, template <class...> class Que>
        struct ChanBase {
           protected:
            using buffer_t = wrap::shared_ptr<ChanBuffer<T, Que>>;
            buffer_t buffer;
            BlockLevel blocking = BlockLevel::normal;

           public:
            constexpr ChanBase() {}
            ChanBase(buffer_t& in)
                : buffer(in) {}

            void set_blocking(bool flag) {
                blocking = flag ? BlockLevel::force_block : BlockLevel::normal;
            }

            void set_blocking(BlockLevel level) {
                blocking = level;
            }

            void change_limit(size_t limit) {
                if (buffer) {
                    buffer->change_limit(limit);
                }
            }

            void change_policy(ChanDisposePolicy policy) {
                if (buffer) {
                    buffer->change_policy(policy);
                }
            }

            bool is_closed() const {
                return buffer ? buffer->is_closed() : true;
            }

            bool close() {
                return buffer ? buffer->close() : true;
            }
        };

        template <class T, template <class...> class Que = wrap::queue>
        struct RecvChan : ChanBase<T, Que> {
            using ChanBase<T, Que>::ChanBase;

            ChanState operator>>(T& t) {
                if (!this->buffer) {
                    return false;
                }
                if (this->blocking == BlockLevel::force_block) {
                    return this->buffer->blocking_load(t);
                }
                else if (this->blocking == BlockLevel::mustnot) {
                    return this->buffer->try_load(t);
                }
                else {
                    return this->buffer->load(t);
                }
            }
        };

        template <class T, template <class...> class Que = wrap::queue>
        struct SendChan : ChanBase<T, Que> {
            using ChanBase<T, Que>::ChanBase;

            ChanState operator<<(T&& t) {
                if (!this->buffer) {
                    return false;
                }
                if (this->blocking == BlockLevel::force_block) {
                    return this->buffer->blocking_store(std::move(t));
                }
                else if (this->blocking == BlockLevel::mustnot) {
                    return this->buffer->try_store(std::move(t));
                }
                else {
                    return this->buffer->store(std::move(t));
                }
            }
        };

        template <class T, template <class...> class Que = wrap::queue>
        std::pair<SendChan<T, Que>, RecvChan<T, Que>> make_chan(size_t limit = ~0, ChanDisposePolicy policy = ChanDisposePolicy::dispose_new) {
            auto buffer = wrap::make_shared<ChanBuffer<T, Que>>(limit, policy);
            return {SendChan<T, Que>(buffer), RecvChan<T, Que>(buffer)};
        }

    }  // namespace thread
}  // namespace utils
