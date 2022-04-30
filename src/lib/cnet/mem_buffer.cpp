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
                // pre_loop -w reaches e-> on_loop -w reaches r-> after_loop  -r reaches s-> pre_loop
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

                size_t virtual_size = 0;

                CustomAllocator a;
            };

            struct MallocContext {
                void* (*alloc)(size_t) = nullptr;
                void* (*realloc)(void*, size_t) = nullptr;
                void (*deleter)(void*) = nullptr;
            };

            MemoryBuffer* STDCALL new_buffer(CustomAllocator allocs) {
                auto ret = new MemoryBuffer{};
                ret->a = allocs;
                return ret;
            }

            MemoryBuffer* STDCALL new_buffer(void* (*alloc)(size_t), void* (*realloc)(void*, size_t), void (*deleter)(void*)) {
                CustomAllocator a;
                a.alloc = [](void* p, size_t size) {
                    auto ctx = static_cast<MallocContext*>(p);
                    return ctx->alloc ? ctx->alloc(size) : nullptr;
                };
                a.realloc = [](void* p, void* old, size_t size) {
                    auto ctx = static_cast<MallocContext*>(p);
                    return ctx->realloc ? ctx->realloc(old, size) : nullptr;
                };
                a.deleter = [](void* p, void* ptr) {
                    auto ctx = static_cast<MallocContext*>(p);
                    return ctx->deleter ? ctx->deleter(ptr) : (void)0;
                };
                a.ctxdeleter = [](void* p) {
                    auto ctx = static_cast<MallocContext*>(p);
                    delete ctx;
                };
                a.ctx = new MallocContext{.alloc = alloc, .realloc = realloc, .deleter = deleter};
                return new_buffer(a);
            }

            void STDCALL delete_buffer(MemoryBuffer* buf) {
                if (!buf) return;
                if (buf->a.deleter) {
                    buf->a.deleter(buf->a.ctx, buf->area);
                }
                if (buf->a.ctxdeleter) {
                    buf->a.ctxdeleter(buf->a.ctx);
                }
                delete buf;
            }

            size_t STDCALL size(const MemoryBuffer* b) {
                return b->virtual_size;
            }

            size_t STDCALL capacity(const MemoryBuffer* b) {
                return b->size_;
            }

            static void* allocate(MemoryBuffer* b, size_t s) {
                if (!b->a.alloc) {
                    return nullptr;
                }
                return b->a.alloc(b->a.ctx, s);
            }

            static void* reallocate(MemoryBuffer* b, void* old, size_t s) {
                if (!b->a.realloc) {
                    return nullptr;
                }
                return b->a.realloc(b->a.ctx, old, s);
            }

            bool STDCALL clear_and_allocate(MemoryBuffer* b, size_t new_size, bool fixed) {
                if (b->a.deleter) {
                    b->a.deleter(b->a.ctx, b->area);
                }
                b->area = nullptr;
                b->area = static_cast<char*>(allocate(b, new_size));
                if (!b->area) {
                    return false;
                }
                if (fixed) {
                    b->strategy = Strategy::fixed;
                }
                else {
                    b->strategy = Strategy::none;
                }
                b->size_ = new_size;
                b->wpos = 0;
                b->rpos = 0;
                b->over = 0;
                b->sub_end = 0;
                b->state = State::pre_loop;
                return true;
            }

            static bool alloc_memory(MemoryBuffer* b, size_t s, Strategy strategy) {
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

            static bool expand_memory(MemoryBuffer* b, size_t new_size) {
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

            static bool write_increment(MemoryBuffer* b) {
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

            static bool read_increment(MemoryBuffer* b) {
                b->rpos++;
                if (b->state == State::pre_loop) {
                    if (b->rpos == b->wpos) {
                        return false;
                    }
                }
                if (b->state == State::on_loop) {
                    if (b->rpos == b->size_) {
                        b->rpos = 0;
                        b->state = State::pre_loop;
                        if (b->rpos == b->wpos) {
                            return false;
                        }
                    }
                }
                if (b->state == State::after_loop) {
                    if (b->rpos == b->sub_end) {
                        b->rpos = 0;
                        b->state = State::pre_loop;
                        // d = data
                        // |r|d|o| |s|d|d|w| |e|
                        // on s the data also exists so
                        // |r|d|o| |d|d|d|w| |e|
                        // memmove make data compaction
                        // |r|d|d|d|d|w| | | |e|
                        auto dst = b->area + b->over;
                        auto src = b->area + b->sub_end;
                        auto size = b->wpos - b->sub_end;
                        ::memmove(dst, src, size);
                        b->wpos = b->over + size;
                        if (b->rpos == b->wpos) {
                            return false;
                        }
                    }
                }
                return true;
            }

            size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s) {
                if (!m) {
                    return 0;
                }
                if (!alloc_memory(b, s, b->strategy)) {
                    return 0;
                }
                if (b->state == State::on_loop && b->rpos == b->wpos) {
                    return 0;
                }
                else if (b->state == State::after_loop && b->wpos == b->size_) {
                    if (!expand_memory(b, b->size_ + b->size_)) {
                        return 0;
                    }
                }
                auto v = static_cast<const char*>(m);
                size_t i = 0, offset = 0;
                for (; i < s; i++) {
                    b->area[b->wpos] = v[i];
                    if (!write_increment(b)) {
                        i++;
                        break;
                    }
                }
                b->virtual_size += i;
                return i;
            }

            size_t STDCALL remove(MemoryBuffer* b, void* m, size_t s) {
                if (!m) {
                    return 0;
                }
                if (b->state == State::pre_loop && b->rpos == b->wpos) {
                    return 0;
                }
                auto v = static_cast<char*>(m);
                size_t count = 0;
                for (; count < s; count++) {
                    v[count] = b->area[b->rpos];
                    b->area[b->rpos] = 0;
                    if (!read_increment(b)) {
                        count++;
                        break;
                    }
                }
                b->virtual_size -= count;
                return count;
            }

        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
