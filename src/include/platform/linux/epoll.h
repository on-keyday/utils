/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// epoll - epoll descriptor
#pragma once

#include <cstdint>
#include "epollcb.h"

namespace utils {
    namespace platform {
        namespace linuxd {
            template <class Fn>
            struct EpollCall {
                Fn f;
                bool call(void* ptr, std::uint32_t events) {
                    return f(ptr, events);
                }
            };

            struct Epoller {
                int register_handle(int fd, std::uint32_t eventflag, void* userdata);
                void remove_handle(int fd);

                template <class Fn>
                void register_callback(Fn&& fn) {
                    register_callback_impl(EpollCall<Fn>{std::forward<Fn>(fn)});
                }

                void wait_callback(std::uint32_t maxcount, int wait);

               private:
                Epoller();
                ~Epoller();
                void register_callback_impl(EpollCallback cb);
                friend Epoller* get_poller();
            };

            Epoller* get_poller();
        }  // namespace linux
    }      // namespace platform
}  // namespace utils
