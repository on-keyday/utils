/*license*/

// writer - write binary following the endian rule
#pragma once

#include "endian.h"

namespace utils {
    namespace endian {

        template <class T>
        struct Writer {
            using raw_type = T;
            using unref_type = std::remove_reference_t<T>;

            raw_type buf;

            template <class... Args>
            Writer(Args&&... args)
                : buf(std::forward<Args>(args)...) {}

            template <class T>
            void write(const T& t) {
                const char* ptr = reinterpret_cast<const char*>(&t);
                for (auto i = 0; i < sizeof(T); i++) {
                    buf.push_back(ptr[i]);
                }
            }

            template <class T>
            void write_hton(const T& t) {
                return write(to_network(&t));
            }
        };
    }  // namespace endian
}  // namespace utils