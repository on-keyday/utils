/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// channel - channel between threads
#pragma once

#include "lite_lock.h"
#include "../wrap/light/queue.h"
#include "../wrap/light/enum.h"
#include "../wrap/light/smart_ptr.h"

namespace futils {
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

        struct ContainerHandler {
            template <class Que>
            bool remove(Que& que, ChanDisposePolicy policy) {
                if (policy == ChanDisposePolicy::dispose_front) {
                    que.pop_front();
                }
                else if (policy == ChanDisposePolicy::dispose_back) {
                    que.pop_back();
                }
                else {
                    return false;
                }
                return true;
            }

            template <class Que, class T>
            void push(Que& que, T&& t) {
                que.push_back(std::move(t));
            }

            template <class Que, class T>
            void pop(Que& que, T& t) {
                t = std::move(que.front());
                que.pop_front();
            }
        };

        struct PriorityHandler {
            template <class Que>
            bool remove(Que& que, ChanDisposePolicy policy) {
                return false;
            }

            template <class Que, class T>
            void push(Que& que, T&& t) {
                que.push(std::move(t));
            }

            template <class Que, class T>
            void pop(Que& que, T& t) {
                t = std::move(const_cast<T&>(que.top()));
                que.pop();
            }
        };

        template <class T, template <class...> class Que = wrap::queue, class Handler = ContainerHandler, class Lock = LiteLock>
        struct ChanBuffer {
           private:
            Que<T> que;
            size_t limit = ~0;
            Lock lock_;
            std::atomic_flag closed;
            LiteLock read_blocking;
            LiteLock write_blocking;
            ChanDisposePolicy policy = ChanDisposePolicy::dispose_new;
            Handler handler;

            bool check_limit() {
                if (limit == 0) {
                    return false;
                }
                while (que.size() >= limit) {
                    if (!handler.remove(que, policy)) {
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
                if (que.size() == 0) {
                    if (!check_close()) {
                        return ChanStateValue::closed;
                    }
                    write_blocking.unlock();
                    return ChanStateValue::empty;
                }
                handler.pop(que, t);
                write_blocking.unlock();
                return true;
            }

            ChanState unlock_store(T&& t) {
                if (!check_close()) {
                    return ChanStateValue::closed;
                }
                if (!check_limit()) {
                    read_blocking.unlock();
                    return ChanStateValue::full;
                }
                handler.push(que, std::move(t));
                read_blocking.unlock();
                return true;
            }

           public:
            ChanBuffer(size_t limit = ~0, ChanDisposePolicy policy = ChanDisposePolicy::dispose_new)
                : limit(limit), policy(policy) {
            }

            size_t peek_queue() const {
                return que.size();
            }

            template <class Fn>
            bool set_handler(Fn&& fn) {
                lock_.lock();
                fn(this->handler);
                lock_.unlock();
                return true;
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
                    return true;
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

        template <class T, template <class...> class Que, class Handler, class Lock>
        struct ChanBase {
           protected:
            using buffer_t = wrap::shared_ptr<ChanBuffer<T, Que, Handler, Lock>>;
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

            size_t peek_queue() const {
                if (buffer) {
                    return buffer->peek_queue();
                }
                return 0;
            }

            bool is_closed() const {
                return buffer ? buffer->is_closed() : true;
            }

            bool close() {
                return buffer ? buffer->close() : true;
            }

            template <class Fn>
            bool set_handler(Fn&& fn) {
                return buffer ? buffer->set_handler(fn) : false;
            }
        };

        template <class T, template <class...> class Que = wrap::queue, class Handler = ContainerHandler, class Lock = LiteLock>
        struct RecvChan : ChanBase<T, Que, Handler, Lock> {
            using ChanBase<T, Que, Handler, Lock>::ChanBase;

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

        template <class T, template <class...> class Que = wrap::queue, class Handler = ContainerHandler, class Lock = LiteLock>
        struct SendChan : ChanBase<T, Que, Handler, Lock> {
            using ChanBase<T, Que, Handler, Lock>::ChanBase;

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

        template <class T, template <class...> class Que = wrap::queue, class Handler = ContainerHandler, class Lock = LiteLock>
        std::pair<SendChan<T, Que, Handler, Lock>, RecvChan<T, Que, Handler, Lock>> make_chan(size_t limit = ~0, ChanDisposePolicy policy = ChanDisposePolicy::dispose_new) {
            auto buffer = wrap::make_shared<ChanBuffer<T, Que, Handler, Lock>>(limit, policy);
            return {SendChan<T, Que, Handler, Lock>(buffer), RecvChan<T, Que, Handler, Lock>(buffer)};
        }

    }  // namespace thread
}  // namespace futils
