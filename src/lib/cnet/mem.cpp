/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/mem.h"
#include <cassert>
#include "../../include/wrap/light/enum.h"

namespace utils {
    namespace cnet {
        namespace mem {
            enum class Strategy {
                none = 0x0,
                fixed = 0x2,
            };

            DEFINE_ENUM_FLAGOP(Strategy)

            enum class State {
                // |r| |w| |e|
                pre_loop,
                // |w| |r| |e|
                on_loop,
                // | | |o|r|s|w| | |e|
                after_loop,

                // state
                // pre_loop -w reaches e-> on_loop -w reaches r-> after_loop
                //                                |  -r reaches s-> pre_loop
                //                                |
                //                                | r reaches e
                //                                -> pre_loop
                // pre_loop -r reaches w-> block
                // on_loop -r reaches e-> pre_loop
            };

            struct MemoryBuffer {
                Strategy strategy = Strategy::none;
                char* area = nullptr;
                State state = State::pre_loop;
                size_t size_ = 0;    // e - end of sequence
                size_t wpos = 0;     // w - writer
                size_t rpos = 0;     // r - reader
                size_t over = 0;     // o - overtaken place
                size_t sub_end = 0;  // s - sub sequence end

                void* (*alloc)(size_t) = nullptr;
                void* (*realloc)(void*, size_t) = nullptr;
                void (*deleter)(void*) = nullptr;
            };

            MemoryBuffer* STDCALL new_buffer(void* (*alloc)(size_t), void* (*realloc)(void*, size_t), void (*deleter)(void*)) {
                auto ret = new MemoryBuffer{};
                ret->alloc = alloc;
                ret->realloc = realloc;
                ret->deleter = deleter;
                return ret;
            }

            void STDCALL delete_buffer(MemoryBuffer* buf) {
                if (!buf) return;
                if (buf->deleter) {
                    buf->deleter(buf->area);
                }
                delete buf;
            }

            void* allocate(MemoryBuffer* b, size_t s) {
                if (!b->alloc) {
                    return nullptr;
                }
                return b->alloc(s);
            }

            void* reallocate(MemoryBuffer* b, void* old, size_t s) {
                if (!b->realloc) {
                    return nullptr;
                }
                return b->realloc(old, s);
            }

            bool alloc_memory(MemoryBuffer* b, size_t s, Strategy strategy) {
                if (!b->area) {
                    if (s == 0) return false;
                    b->area = static_cast<char*>(allocate(b, s + s));
                    if (!b->area) {
                        return false;
                    }
                    if (!b->area) {
                        return 0;
                    }
                    b->size_ = s + s;
                    b->strategy = strategy;
                }
                return true;
            }

            bool expand_memory(MemoryBuffer* b, size_t new_size) {
                if (any(b->strategy & Strategy::fixed)) {
                    return false;
                }
                assert(new_size > b->size_);
                auto new_area = static_cast<char*>(reallocate(b, b->area, new_size));
                if (!new_area) {
                    return false;
                }
                b->area = new_area;
                b->size_ = new_size;
                return true;
            }

            bool write_increment(MemoryBuffer* b) {
                b->wpos++;
                if (b->state == State::pre_loop) {
                    if (b->wpos == b->size_) {
                        b->wpos = 0;
                        b->state = State::on_loop;
                    }
                }
                if (b->state == State::on_loop) {
                    if (b->wpos == b->rpos) {
                        b->over = b->wpos;
                        b->sub_end = b->size_;
                        if (!expand_memory(b, b->size_ + b->size_)) {
                            return false;
                        }
                        b->wpos = b->sub_end;
                        b->state = State::after_loop;
                    }
                }
                if (b->state == State::after_loop) {
                    if (b->wpos == b->size_) {
                        if (!expand_memory(b, b->size_ + b->size_)) {
                            return false;
                        }
                    }
                }
                return true;
            }

            bool read_increment(MemoryBuffer* b) {
                b->rpos++;
                if (b->state == State::pre_loop) {
                    if (b->rpos == b->wpos) {
                        return false;
                    }
                }
            }

            size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s) {
                if (!m) {
                    return 0;
                }
                if (!alloc_memory(b, s, b->strategy)) {
                    return 0;
                }
                auto v = static_cast<const char*>(m);
                size_t i = 0, offset = 0;
                for (; i < s; i++) {
                }

                return i;
            }

            char* get_rpos(MemoryBuffer* b) {
            }

            size_t STDCALL remove(MemoryBuffer* b, void* m, size_t s) {
                if (!m) {
                    return 0;
                }
                auto v = static_cast<char*>(m);
                size_t count = 0;
                while (true) {
                    if (count == s) {
                        break;
                    }
                    auto r = get_rpos(b);
                    if (!r) {
                        break;
                    }
                    v[count] = *r;
                    *r = 0;
                    count++;
                }
                return count;
            }

        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
