/*license*/

// cout - wrapper of cout
// need to link libutils
#pragma once

#include <iosfwd>
#include <sstream>
#include "lite/char.h"
#include "lite/string.h"
#include "../helper/sfinae.h"
#include "../core/buffer.h"
#include "../utf/convert.h"
#include "../thread/lite_lock.h"

namespace utils {
    namespace wrap {
#ifdef _WIN32
        using ostream = std::wostream;
        using stringstream = std::wstringstream;
#else
        using ostream = std::ostream;
        using stringstream = std::stringstream;
#endif

        struct WriteWrapper {
            SFINAE_BLOCK_T_BEGIN(is_string, std::declval<T>()[0])
            template <class Out>
            static Out& invoke(Out& out, T&& t, stringstream&, thread::LiteLock*) {
                path_string tmp;
                utf::convert<true>(t, tmp);
                out.write(tmp);
                return out;
            }
            SFINAE_BLOCK_T_ELSE(is_string)
            template <class Out>
            static Out& invoke(Out& out, T&& t, stringstream& ss, thread::LiteLock* lock) {
                if (lock) {
                    lock->lock();
                }
                ss.str(path_string());
                ss << t;
                auto tmp = ss.str();
                if (lock) {
                    lock->unlock();
                }
                out.write(tmp);
                return out;
            }
            SFINAE_BLOCK_T_END()

            template <class Out, class T>
            static Out& write(Out& out, T&& t, stringstream& ss, thread::LiteLock* lock) {
                return is_string<T>::invoke(out, std::forward<T>(t), ss, lock);
            }
        };

        struct UtfOut;
        namespace internal {
            struct PackImpl {
                path_string result;
                void write(const path_string& str) {
                    result += str;
                }

                void pack_impl(stringstream& ss) {}

                template <class T, class... Args>
                void pack_impl(stringstream& ss, T&& t, Args&&... args) {
                    WriteWrapper::write(*this, std::forward<T>(t), ss, nullptr);
                    pack_impl(ss, std::forward<Args>(args)...);
                }
            };

            struct Pack {
               private:
                friend struct wrap::UtfOut;
                PackImpl impl;

               public:
                template <class... Args>
                Pack&& pack(Args&&... args) {
                    stringstream ss;
                    impl.pack_impl(ss, std::forward<Args>(args)...);
                    return std::move(*this);
                }

                template <class... Args>
                Pack(Args&&... args) {
                    pack(std::forward<Args>(args)...);
                }
            };

        }  // namespace internal

        template <class... Args>
        internal::Pack pack(Args&&... args) {
            return internal::Pack(std::forward<Args>(args)...);
        }

        struct UtfOut {
           private:
            ostream& out;
            stringstream ss;
            ::FILE* std_handle = nullptr;
            thread::LiteLock lock;

           public:
            UtfOut(ostream& out);

            template <class T>
            UtfOut& operator<<(T&& t) {
                return WriteWrapper::write(*this, std::forward<T>(t), ss, &lock);
            }

            void write(const path_string&);

            UtfOut& operator<<(internal::Pack&& pack);
        };
        extern int stdinmode;
        extern int stdoutmode;
        extern int stderrmode;
        extern bool sync_stdio;

        UtfOut& cout_wrap();
        UtfOut& cerr_wrap();
    }  // namespace wrap
}  // namespace utils
