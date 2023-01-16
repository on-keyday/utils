/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// event - event
#pragma once
#include <ctime>
#include "../mem/shared_ptr.h"

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

            using EventCallback = mem::CBS<QueState, EventLoop*>;

            struct Event {
                EventArg arg;
                EventType type;
                // event callback
                // should contain context around callstack
                EventCallback callback;
            };

            template <class F>
            EventCallback make_event_cb(allocate::Alloc* a, F&& f) {
                return mem::make_cb<QueState, EventLoop*>(a, std::forward<F>(f));
            }
            /*
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
            }*/

        }  // namespace core
    }      // namespace quic
}  // namespace utils
