/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/dll/glheap.h>

namespace utils {
    namespace dnet {
        static Allocs tmphold;
        void set_allocs(Allocs set) {
            tmphold = set;
        }

        Allocs get_() {
            auto s = tmphold;
            if (!s.alloc_ptr || !s.realloc_ptr || !s.free_ptr) {
                s.alloc_ptr = simple_heap_alloc;
                s.realloc_ptr = simple_heap_realloc;
                s.free_ptr = simple_heap_free;
            }
            return s;
        }

        Allocs* get_alloc() {
            static Allocs alloc = get_();
            return &alloc;
        }
    }  // namespace dnet
}  // namespace utils
