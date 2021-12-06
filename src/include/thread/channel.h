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
           private:
            Que<T> que;
            size_t limit = ~0;
            LiteLock lock_;
            std::atomic_flag closed;
            LiteLock blocking;
            ChanDisposePolicy policy = ChanDisposePolicy::dispose_new;

            bool check_limit() {
                if (que.size() >= limit) {
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

            bool check_close() {
                if (closed.test()) {
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
                blocking.unlock();
                return true;
            }

           public:
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

            ChanState blocking_load(T& t) {
                while (true) {
                    lock_.lock();
                    auto res = unlock_load(t);
                    if (res == ChanStateValue::empty) {
                        blocking.flag.test_and_set();
                        lock_.unlock();
                        blocking.flag.wait(true);
                        continue;
                    }
                    lock_.unlock();
                    return res;
                }
            }

            bool close() {
                lock_.lock();
                auto res = closed.test_and_set();
                blocking.unlock();
                lock_.unlock();
                return res;
            }

            bool is_closed() const {
                return closed.test();
            }
        };
    }  // namespace thread
}  // namespace utils
