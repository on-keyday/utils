/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// mem - memory buffer
#pragma once
#include "cnet.h"
#include <cstring>
#include <wrap/light/smart_ptr.h>

namespace utils {
    namespace cnet {
        namespace mem {
            // buffer module
            struct MemoryBuffer;
            struct CustomAllocator {
                void* (*alloc)(void*, size_t) = nullptr;
                void* (*realloc)(void*, void*, size_t) = nullptr;
                void (*deleter)(void*, void*) = nullptr;
                void (*ctxdeleter)(void*) = nullptr;
                void* ctx = nullptr;
            };
            DLL MemoryBuffer* STDCALL new_buffer(CustomAllocator allocs);
            inline MemoryBuffer* new_buffer(void* (*alloc)(void*, size_t), void* (*realloc)(void*, void*, size_t), void (*deleter)(void*, void*),
                                            void* ctx, void (*ctxdeleter)(void*)) {
                CustomAllocator a;
                a.alloc = alloc;
                a.realloc = realloc;
                a.deleter = deleter;
                a.ctx = ctx;
                a.ctxdeleter = ctxdeleter;
                return new_buffer(a);
            }
            DLL MemoryBuffer* STDCALL new_buffer(void* (*alloc)(size_t), void* (*realloc)(void*, size_t), void (*deleter)(void*));
            DLL void STDCALL delete_buffer(MemoryBuffer* buf);
            DLL size_t STDCALL append(MemoryBuffer* b, const void* m, size_t s);
            inline size_t append(MemoryBuffer* b, const char* m) {
                return append(b, m, m ? ::strlen(m) : 0);
            }
            DLL size_t STDCALL remove(MemoryBuffer* b, void* m, size_t s);

            DLL size_t STDCALL size(const MemoryBuffer* b);

            DLL size_t STDCALL capacity(const MemoryBuffer* b);

            DLL bool STDCALL clear_and_allocate(MemoryBuffer* b, size_t new_size, bool fixed);

            // cnet interface

            struct LockedIO;

            enum BufferConfig {
                io_pair,
                mem_buffer_set,
            };

            struct LockPair {
                wrap::shared_ptr<LockedIO> io;
                bool set = false;
            };

            inline bool set_buffer(CNet* ctx, MemoryBuffer* b) {
                return set_ptr(ctx, mem_buffer_set, b);
            }

            inline bool set_link(CNet* ctx, wrap::shared_ptr<LockedIO> io) {
                LockPair pair;
                pair.set = true;
                pair.io = std::move(io);
                return set_ptr(ctx, io_pair, &pair);
            }

            inline wrap::shared_ptr<LockedIO> get_link(CNet* ctx) {
                LockPair pair;
                if (!set_ptr(ctx, io_pair, &pair)) {
                    return nullptr;
                }
                return pair.io;
            }

            DLL CNet* STDCALL create_mem();

            inline void make_pair(CNet*& mem1, CNet*& mem2, auto... allocs) {
                mem1 = mem::create_mem();
                mem2 = mem::create_mem();
                auto make = [&]() { return mem::new_buffer(allocs...); };
                mem::set_buffer(mem1, make());
                mem::set_buffer(mem2, make());
                mem::set_link(mem1, mem::get_link(mem2));
                mem::set_link(mem2, mem::get_link(mem1));
            }
        }  // namespace mem
    }      // namespace cnet
}  // namespace utils
