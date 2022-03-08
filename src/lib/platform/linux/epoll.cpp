/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/linux/epoll.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <cassert>
#include <cstdint>
#include <cerrno>

namespace utils {
    namespace platform {
        namespace linux {
            static int epoll_fd = ~0;

            void init_epoll() {
                epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
                assert(epoll_fd != -1);
            }

            void delete_epoll() {
                ::close(epoll_fd);
            }

            int regiseter_handle(int fd, std::uint32_t eventflag, void* userdata) {
                epoll_event event;
                event.events = eventflag;
                event.data.fd = fd;
                event.data.ptr = userdata;
                return ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
            }

            void remove_handle(int fd) {
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
        }  // namespace linux
    }      // namespace platform
}  // namespace utils
