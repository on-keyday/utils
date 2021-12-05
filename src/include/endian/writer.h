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
            void write_big(const T& t) {
                write(to_big(&t));
            }

            template <class T>
            void write_little(const T& t) {
                write(to_little(&t));
            }

            template <class T>
            void write_hton(const T& t) {
                write(to_network(&t));
            }
#define DEFINE_WRITE_SEQ(FUNC, SUFFIX)          \
    template <class T>                          \
    void write_##SUFFIX(const T& seq) {         \
        for (auto& s : seq) {                   \
            FUNC(s);                            \
        }                                       \
    }                                           \
                                                \
    template <class Begin, class End>           \
    void write_##SUFFIX(Begin begin, End end) { \
        for (auto i = begin; i != end; i++) {   \
            FUNC(*i);                           \
        }                                       \
    }
            DEFINE_WRITE_SEQ(write, seq)
            DEFINE_WRITE_SEQ(write_big, seq_big)
            DEFINE_WRITE_SEQ(write_little, seq_little)
            DEFINE_WRITE_SEQ(write_hton, seq_hton)
        };
#undef DEFINE_WRITE_SEQ
    }  // namespace endian
}  // namespace utils
