/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cout - wrapper of cout
// need to link libutils
#pragma once
#include "../platform/windows/dllexport_header.h"
#include <iosfwd>
#include "light/char.h"
#include "light/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "light/stream.h"
#include <file/file.h>

namespace utils {
    namespace wrap {

        struct utils_DLL_EXPORT UtfOut {
           private:
            stringstream ss;
            thread::LiteLock lock;
            const file::File& file;

           public:
            UtfOut(const file::File& out)
                : file(out) {}

            template <class T>
            UtfOut& operator<<(T&& t) {
                return WriteWrapper::write(*this, std::forward<T>(t), ss, &lock);
            }

            template <class T>
                requires(file::internal::has_c_str_for_path<T>)
            UtfOut& operator<<(const T& t) {
                write(t);
                return *this;
            }

            template <class U8Buf = string, class T>
                requires(file::internal::has_c_str_for_path<T>)
            file::file_result<void> write(const T& p) {
                if (file.is_tty()) {
                    return file.write_console(p).transform([](auto) {});
                }
                if constexpr (sizeof(path_char) != 1) {
                    U8Buf s;
                    utf::convert<2, 1>(p, s);
                    return file.write_file(s).transform([](auto) {});
                }
                else {
                    return file.write_file(p).transform([](auto) {});
                }
            }

            UtfOut& operator<<(internal::Pack&& pack) {
                write(pack.impl.result);
                return *this;
            }

            bool is_tty() const {
                return file.is_tty();
            }

            file::file_result<void> set_virtual_terminal(bool enable) {
                return file.set_virtual_terminal_output(enable);
            }

            const file::File& get_file() const {
                return file;
            }
        };

        utils_DLL_EXPORT UtfOut& STDCALL cout_wrap();
        utils_DLL_EXPORT UtfOut& STDCALL cerr_wrap();

    }  // namespace wrap
}  // namespace utils
