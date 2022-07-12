/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// event - event
#pragma once
#include <ctime>

namespace utils {
    namespace quic {
        namespace core {
            enum class EventType {
                // idle event do nothing
                idle,
                // io_wait event waits io completion/io bound operation
                io_wait,
                // memory_exhausted event occur when allocation fail
                memory_exhausted,
                // normal event is cpu bound operation
                normal,
            };

            struct QUIC;
            struct EventLoop;

            using QueState = bool;
            constexpr QueState que_enque = false;
            constexpr QueState que_remove = true;

            struct EventArg {
                EventType type;
                void* ptr;
                void* prev;
                std::time_t timer;

                template <class T>
                T* to() {
                    return static_cast<T*>(ptr);
                }
            };

            using EventCallback = QueState (*)(EventLoop*, QUIC* q, EventArg arg);

            struct Event {
                EventArg arg;
                EventCallback callback;
            };

            Event event(EventType type, auto t, auto prev, std::time_t timer,
                        EventCallback callback) {
                return Event{
                    .arg = {
                        .type = type,
                        .ptr = t,
                        .prev = prev,
                        .timer = timer,
                    },
                    .callback = callback,
                };
            }

        }  // namespace core
    }      // namespace quic
}  // namespace utils
