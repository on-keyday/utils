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

            struct MemoryBuffer {
                Strategy strategy = Strategy::none;
                char* area = nullptr;
                size_t size_ = 0;
                size_t physical_rpos = 0;
                size_t logical_wpos = 0;
                size_t physical_wpos = 0;
                size_t locking_place = 0;
                size_t passed_place = 0;
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
                    b->locking_place = b->size_;
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
                b->locking_place = new_size;
                b->passed_place = new_size;
                return true;
            }

            bool expand_loop(MemoryBuffer* b, size_t s) {
                auto w_remain = b->size_ - b->physical_wpos;
                auto r_remain = b->physical_rpos;
                auto remain = w_remain + r_remain;
                if (s < remain) {
                    return true;
                }
                // if memory write(w) and read(r) and end(e) are like below
                // |r| |w| |e|
                // or like below
                // |r|p| |w|e|
                // we can expand this simply
                // |r| |w| | | | | | |e|
                if (b->physical_rpos < b->physical_wpos) {
                    const auto new_size = b->size_ + s;
                    return expand_memory(b, new_size);
                }
                // if memory write and read and end are like below
                // |w| |r| |e|
                // we must save previous end(p) and expand area
                // |w| |r| |p| | | | |e|
                else {
                    const auto new_size = b->size_ + s - remain;
                    const auto lock = b->locking_place;
                    const auto passed = b->passed_place;
                    if (!expand_memory(b, new_size)) {
                        return false;
                    }
                    b->locking_place = lock;
                    b->passed_place = passed;
                    return true;
                }
                // when reader reached loop point and pass loop
                // |r|w| | |p| | | | |e|
                // we have to move objects
                // |r|w| | | | | | | |e|
            }

            void carry_wpos(MemoryBuffer* b) {
                if (b->physical_wpos == b->size_) {
                    // here is like below (o=we)
                    // | | |r| |p| | | | |o|
                    // so jump to first point
                    // |w| |r| |p| | | | |e|
                    b->physical_wpos = 0;
                }
                if (b->physical_wpos == b->physical_rpos) {
                    // here is like below (b=wr)
                    // | | |b| |p| | | | |e|
                    // so jump to after loop point
                    // and save passed place(s) (u=sr)
                    // | | |u| |p|w| | | |e|
                    b->passed_place = b->physical_wpos;
                    b->physical_wpos = b->locking_place + 1;
                }
            }

            size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s) {
                if (!m) {
                    return 0;
                }
                if (!alloc_memory(b, s, b->strategy)) {
                    return 0;
                }
                if (!expand_loop(b, s)) {
                    return 0;
                }
                auto v = static_cast<const char*>(m);
                size_t i = 0, offset = 0;
                for (; i < s; i++) {
                    b->area[b->physical_wpos] = v[i];
                    b->physical_wpos++;
                    carry_wpos(b);
                }
                b->logical_wpos += i;
                return i;
            }

            char* get_rpos(MemoryBuffer* b) {
                if (b->physical_rpos == b->locking_place) {
                    // here is like below (b=wp)
                    // | | |w| |b| | | | |e|
                    // | | |s| |b| |w| | |e|
                    // so jump to first
                    // |r| |w| |p| | | | |e|
                    // |r| |s| |p| |w| | |e|
                    b->physical_rpos = 0;
                    // if like below
                    // |r| |w| |p| | | | |e| -> |r| |w| | | | | | |e|
                    if (b->physical_wpos < b->locking_place) {
                        auto start = b->area + b->locking_place;
                        ::memmove(start, start + 1, b->size_ - b->locking_place);
                    }
                    // else like below
                    // |r| |s| |p| |w| | |e| -> |r| |w| | | | | | |e|
                    else {
                        auto src = b->area + b->locking_place + 1;
                        auto dst = b->area + b->passed_place;
                        ::memmove(dst, src, b->size_ - b->locking_place);
                    }
                }
                else {
                    b->physical_rpos++;
                }
                if (b->physical_rpos == b->physical_wpos) {
                    b->physical_rpos--;
                    return nullptr;
                }
                return b->area + b->physical_rpos;
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
                }
                return count;
            }

        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
