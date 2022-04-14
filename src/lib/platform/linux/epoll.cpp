/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#ifdef __linux__
#include "../../../include/platform/linux/epoll.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>

#include "../../../include/wrap/light/vector.h"

namespace utils {
    namespace platform {
        namespace linuxd {
            static int epoll_fd = ~0;

            void init_epoll() {
                epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
                assert(epoll_fd != -1);
            }

            void delete_epoll() {
                ::close(epoll_fd);
            }

            int Epoller::register_handle(int fd, std::uint32_t eventflag, void* userdata) {
                epoll_event event;
                event.events = eventflag;
                event.data.fd = fd;
                event.data.ptr = userdata;
                return ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
            }

            void Epoller::remove_handle(int fd) {
                ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            }

            template <class Fn>
            void wait_event(Fn&& fn, int maxcount, int wait) {
                epoll_event events[64];
                auto nfd = ::epoll_wait(epoll_fd, events, maxcount, wait);
                for (auto i = 0; i < nfd; i++) {
                    fn(events[i].data.ptr, events[i].events);
                }
            }

            wrap::vector<EpollCallback> callbacks;

            void wait_callback(std::uint32_t maxcount, int wait) {
                if (maxcount > 64) {
                    maxcount = 64;
                }
                wait_event(
                    [](void* ptr, std::uint32_t events) {
                        for (auto& callback : callbacks) {
                            if (callback.call(ptr, events)) {
                                return;
                            }
                        }
                    },
                    maxcount, wait);
            }

            void Epoller::register_callback_impl(EpollCallback cb) {
                callbacks.push_back(std::move(cb));
            }

            Epoller::Epoller() {
                init_epoll();
            }

            Epoller::~Epoller() {
                delete_epoll();
            }

            Epoller* get_poller() {
                static Epoller poller;
                return &poller;
            }
        }  // namespace linuxd
    }      // namespace platform
}  // namespace utils
#endif
