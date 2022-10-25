/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// boxbytelen - boxed byte len
#pragma once
#include "bytelen.h"
#include "../dll/glheap.h"
#include <utility>

namespace utils {
    namespace dnet {
        namespace quic {
            struct BoxByteLen {
               private:
                ByteLen b;

               public:
                constexpr BoxByteLen() = default;

                constexpr BoxByteLen(ByteLen src) {
                    if (src.valid()) {
                        auto dstData = (byte*)alloc_normal(src.len, DNET_DEBUG_MEMORY_LOCINFO(true, src.len));
                        if (!dstData) {
                            return;
                        }
                        for (size_t i = 0; i < src.len; i++) {
                            dstData[i] = src.data[i];
                        }
                        b = {dstData, src.len};
                    }
                }

                constexpr BoxByteLen(BoxByteLen&& in) {
                    b.data = std::exchange(in.b.data, nullptr);
                    b.len = std::exchange(in.b.len, 0);
                }

                constexpr BoxByteLen& operator=(BoxByteLen&& in) {
                    if (this == &in) {
                        return *this;
                    }
                    this->~BoxByteLen();
                    b.data = std::exchange(in.b.data, nullptr);
                    b.len = std::exchange(in.b.len, 0);
                    return *this;
                }

                constexpr ByteLen unbox() const {
                    return b;
                }

                constexpr bool valid() const {
                    return b.valid();
                }

                constexpr size_t len() const {
                    return b.len;
                }

                ~BoxByteLen() {
                    if (b.data) {
                        free_normal(b.data, DNET_DEBUG_MEMORY_LOCINFO(true, b.len));
                    }
                }
            };

        }  // namespace quic
    }      // namespace dnet
}  // namespace utils
