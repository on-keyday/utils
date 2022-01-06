/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// pack - packing object for cout wrapper
#pragma once

#include <sstream>
#include "../helper/sfinae.h"
#include "../thread/lite_lock.h"
#include "lite/string.h"
#include "../utf/convert.h"
#include "lite/stream.h"

namespace utils {
    namespace wrap {

        struct WriteWrapper {
            SFINAE_BLOCK_T_BEGIN(is_string, (std::enable_if_t<sizeof(std::declval<T>()[0]) <= 4>)0)
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
                Pack&& packln(Args&&... args) {
                    return pack(std::forward<Args>(args)..., "\n");
                }

                template <class T>
                Pack&& operator<<(T&& t) {
                    stringstream ss;
                    impl.pack_impl(ss, std::forward<T>(t));
                    return std::move(*this);
                }

                Pack&& operator<<(const path_string& s) {
                    impl.result += s;
                    return std::move(*this);
                }

                Pack&& operator<<(Pack&& in) {
                    impl.result += in.impl.result;
                    return std::move(*this);
                }

                void clear() {
                    impl.result.clear();
                }

                bool empty() const {
                    return impl.result.empty();
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
    }  // namespace wrap
}  // namespace utils
