/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// interface - common interface class
#pragma once
#include <cstdint>
#include <space/space.h>
#include <helper/pushbacker.h>

namespace utils {
    namespace parser {
        namespace expr {
            using PushBacker = helper::IPushBacker<>;

            struct ErrorStack {
               private:
                void* ptr = nullptr;
                void (*pusher)(void* ptr, const char* type, const char* msg, const char* loc, size_t line, size_t begin, size_t end, void* aditional) = nullptr;
                void (*poper)(void* ptr, size_t index) = nullptr;
                size_t (*indexer)(void* ptr) = nullptr;

                template <class T>
                static void push_fn(void* ptr, const char* type, const char* msg, const char* loc, size_t line, size_t begin, size_t end, void* aditional) {
                    static_cast<T*>(ptr)->push(type, msg, loc, line, begin, end, aditional);
                }

                template <class T>
                static void pop_fn(void* ptr, size_t index) {
                    static_cast<T*>(ptr)->pop(index);
                }

                template <class T>
                static size_t index_fn(void* ptr) {
                    return static_cast<T*>(ptr)->index();
                }

               public:
                ErrorStack() {}
                ErrorStack(const ErrorStack& b)
                    : ptr(b.ptr),
                      pusher(b.pusher),
                      poper(b.poper),
                      indexer(b.indexer) {}

                template <class T>
                ErrorStack(T& pb)
                    : ptr(std::addressof(pb)),
                      pusher(push_fn<T>),
                      poper(pop_fn<T>),
                      indexer(index_fn<T>) {}

                bool push(const char* type, const char* msg, size_t begin, size_t end, const char* loc, size_t line, void* additional = nullptr) {
                    if (pusher) {
                        pusher(ptr, type, msg, loc, line, begin, end, additional);
                    }
                    return false;
                }

                bool pop(size_t index) {
                    if (poper) {
                        poper(ptr, index);
                    }
                    return true;
                }

                size_t index() {
                    return indexer(ptr);
                }
            };

            template <class Str>
            struct StackObj {
                Str type;
                Str msg;
                Str loc;
                size_t line;
                size_t begin;
                size_t end;
                void* additional;
            };

            template <class String, template <class...> class Vec>
            struct Errors {
                Vec<StackObj<String>> stack;
                void push(auto... v) {
                    stack.push_back(StackObj<String>{v...});
                }

                void pop(size_t) {}
                size_t index() {
                    return 0;
                }
            };

#define PUSH_ERROR(stack, type, msg, begin, end) stack.push(type, msg, begin, end, __FILE__, __LINE__, nullptr);
#define PUSH_ERRORA(stack, type, msg, begin, end, additional) stack.push(type, msg, begin, end, __FILE__, __LINE__, additional);

            struct StartPos {
                size_t start;
                size_t pos;
                bool reset;
                size_t* ptr;
                constexpr StartPos() {}
                StartPos(StartPos&& p)
                    : start(p.start), pos(p.pos), reset(p.reset), ptr(p.ptr) {
                    p.reset = false;
                    p.ptr = nullptr;
                }

                bool ok() {
                    reset = false;
                    return true;
                }

                bool fatal() {
                    reset = false;
                    return false;
                }

                bool err() {
                    reset = false;
                    *ptr = start;
                    return false;
                }

                ~StartPos() {
                    if (reset && ptr) {
                        err();
                    }
                }
            };
            template <class T>
            [[nodiscard]] StartPos save(Sequencer<T>& seq) {
                StartPos pos;
                pos.start = seq.rptr;
                pos.reset = true;
                pos.pos = seq.rptr;
                pos.ptr = &seq.rptr;
                return pos;
            }

            template <class T>
            [[nodiscard]] StartPos save_and_space(Sequencer<T>& seq, bool line = true) {
                StartPos pos;
                pos.start = seq.rptr;
                pos.reset = true;
                space::consume_space(seq, line);
                pos.pos = seq.rptr;
                pos.ptr = &seq.rptr;
                return pos;
            }

            template <class T>
            auto bind_space(Sequencer<T>& seq) {
                return [&](bool line = true) {
                    return space::consume_space(seq, line);
                };
            }

        }  // namespace expr
    }      // namespace parser
}  // namespace utils
