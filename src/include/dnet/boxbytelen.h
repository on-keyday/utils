/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// boxbytelen - boxed byte len
#pragma once
#include "bytelen.h"
#include "dll/glheap.h"
#include <utility>

namespace utils {
    namespace dnet {

        template <class T>
        struct BoxByteLenBase {
           private:
            ByteLenBase<T> b;
            static_assert(sizeof(T) == 1, "sizeof(T) of BoxByteLenBase must be 1");
            static_assert(!std::is_const_v<T>, "T of BoxByteLenBase must be non-const");

           public:
            constexpr BoxByteLenBase() = default;
            template <class B>
            constexpr BoxByteLenBase(ByteLenBase<B> src) {
                if (src.valid()) {
                    T* dstData = (T*)alloc_normal(src.len, alignof(byte), DNET_DEBUG_MEMORY_LOCINFO(true, src.len, alignof(byte)));
                    if (!dstData) {
                        return;
                    }
                    for (size_t i = 0; i < src.len; i++) {
                        dstData[i] = src.data[i];
                    }
                    b = {dstData, src.len};
                }
            }

            constexpr explicit BoxByteLenBase(size_t size) {
                if (size == 0) {
                    return;
                }
                auto data = (byte*)alloc_normal(size, alignof(byte), DNET_DEBUG_MEMORY_LOCINFO(true, size, alignof(byte)));
                if (!data) {
                    return;
                }
                b = {data, size};
            }

            constexpr BoxByteLenBase(BoxByteLenBase&& in) {
                b.data = std::exchange(in.b.data, nullptr);
                b.len = std::exchange(in.b.len, 0);
            }

            constexpr BoxByteLenBase& operator=(BoxByteLenBase&& in) {
                if (this == &in) {
                    return *this;
                }
                this->~BoxByteLenBase();
                b.data = std::exchange(in.b.data, nullptr);
                b.len = std::exchange(in.b.len, 0);
                return *this;
            }

            constexpr ByteLenBase<T> unbox() const {
                return b;
            }

            constexpr bool valid() const {
                return b.valid();
            }

            constexpr size_t len() const {
                return b.len;
            }

            constexpr T* data() const {
                return b.data;
            }

            constexpr ~BoxByteLenBase() {
                if (b.data) {
                    free_normal(b.data, DNET_DEBUG_MEMORY_LOCINFO(true, b.len, alignof(byte)));
                }
                b.data = nullptr;
                b.len = 0;
            }
        };

        using BoxByteLen = BoxByteLenBase<byte>;

    }  // namespace dnet
}  // namespace utils
