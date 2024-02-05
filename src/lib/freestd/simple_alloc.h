/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <freestd/cstdint>
#include <view/iovec.h>
#include <freestd/string.h>
#include <freestd/stdio.h>

namespace futils::freestd {
    struct FreeChunk {
        FreeChunk* next = nullptr;
        size_t size = 0;  // size of data
        char data[0];     // data of chunk

        FreeChunk* split(size_t this_size) {
            // algin to uintptr_t
            this_size = (this_size + alignof(uintptr_t) - 1) & ~(alignof(uintptr_t) - 1);
            if (this_size >= size ||                     // requested size is bigger than chunk size
                size - this_size < sizeof(FreeChunk)) {  // not enough space for new chunk header
                return nullptr;
            }
            FreeChunk* new_chunk = (FreeChunk*)((uintptr_t)this->data + this_size);
            new_chunk->size = size - this_size - sizeof(FreeChunk);
            new_chunk->next = next;
            assert(this_size + sizeof(FreeChunk) + new_chunk->size == size);
            next = new_chunk;
            size = this_size;
            assert((this->data + size) == (char*)new_chunk);
            return new_chunk;
        }

        bool merge(FreeChunk* chunk) {
            if ((uintptr_t)this->data + size == (uintptr_t)chunk) {
                size += chunk->size + sizeof(FreeChunk);
                next = chunk->next;
                return true;
            }
            return false;
        }

        bool enough_space(size_t user_size) {
            return size >= user_size;
        }
    };

    struct ChunkList {
        futils::view::wvec heap_range;
        FreeChunk* free_list = nullptr;
        size_t split_count = 0;
        size_t alloc_count = 0;

        bool init_heap(futils::view::wvec range) {
            if ((uintptr_t)range.begin() % alignof(uintptr_t) != 0) {
                return false;
            }
            heap_range = range;
            FreeChunk* chunk = (FreeChunk*)heap_range.begin();
            chunk->size = heap_range.size() - sizeof(FreeChunk);
            chunk->next = nullptr;
            free_list = chunk;
            return true;
        }

        bool in_range(FreeChunk* chunk) {
            return chunk >= (FreeChunk*)heap_range.begin() && chunk < (FreeChunk*)heap_range.end();
        }

        FreeChunk* alloc_chunk(size_t size) {
            FreeChunk* prev = nullptr;
            FreeChunk* chunk = free_list;
            while (chunk) {
                if (chunk->enough_space(size)) {
                    if (chunk->size > size + sizeof(FreeChunk)) {
                        auto ch = chunk->split(size);
                        assert(ch && chunk->next == ch);
                        split_count++;
                    }
                    if (prev) {
                        prev->next = chunk->next;
                    }
                    else {
                        free_list = chunk->next;
                    }
                    chunk->next = nullptr;
                    alloc_count++;
                    return chunk;
                }
                prev = chunk;
                chunk = chunk->next;
            }
            return nullptr;
        }

        void sort() {
            FreeChunk* prev = nullptr;
            FreeChunk* chunk = free_list;
            while (chunk) {
                if (prev && prev > chunk) {
                    if (chunk == free_list) {
                        free_list = prev;
                    }
                    prev->next = chunk->next;
                    chunk->next = prev;
                    if (prev == free_list) {
                        free_list = chunk;
                    }
                    prev = nullptr;
                    chunk = free_list;
                }
                else {
                    prev = chunk;
                    chunk = chunk->next;
                }
            }
        }

        void merge() {
            sort();
            FreeChunk* prev = nullptr;
            FreeChunk* chunk = free_list;
            while (chunk) {
                if (prev && prev->merge(chunk)) {
                    split_count--;
                    chunk = prev->next;
                }
                else {
                    prev = chunk;
                    chunk = chunk->next;
                }
            }
        }

        void free_chunk(FreeChunk* chunk) {
            if (!in_range(chunk)) {
                return;
            }
            chunk->next = free_list;
            free_list = chunk;
            alloc_count--;
            if (alloc_count < split_count) {
                merge();
            }
        }

        void* malloc(size_t size) {
            FreeChunk* chunk = alloc_chunk(size);
            if (chunk) {
                return chunk->data;
            }
            return nullptr;
        }

        void free(void* ptr) {
            FreeChunk* chunk = (FreeChunk*)((uintptr_t)ptr - sizeof(FreeChunk));
            free_chunk(chunk);
        }

        void* calloc(size_t nmemb, size_t size) {
            size_t total_size = nmemb * size;
            void* ptr = malloc(total_size);
            if (ptr) {
                memset(ptr, 0, total_size);
            }
            return ptr;
        }

        void* realloc(void*, size_t) {
            // not implemented
            return nullptr;
        }
    };
}  // namespace futils::freestd
