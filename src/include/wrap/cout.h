/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cout - wrapper of cout
// need to link libfutils
#pragma once
#include "../platform/windows/dllexport_header.h"
#include "light/char.h"
#include "light/string.h"
#include "../thread/lite_lock.h"
#include "pack.h"
#include "light/stream.h"
#include <file/file.h>

namespace futils {
    namespace wrap {

        struct UtfOut;

        using write_hook_fn = file::file_result<void> (*)(UtfOut&, view::rvec);

        namespace internal {
            template <class T, class C = byte>
            concept is_path_convertible = requires(T t) {
                { t } -> view::internal::is_const_data<C>;
                { std::size(t) } -> std::convertible_to<size_t>;
            };
        }

        struct futils_DLL_EXPORT UtfOut {
           private:
            stringstream ss;
            thread::LiteLock lock;
            const file::File& file;
            write_hook_fn hook_write = nullptr;

           public:
            UtfOut(const file::File& out)
                : file(out) {}

            template <class T>
            UtfOut& operator<<(T&& t) {
                return WriteWrapper::write(*this, std::forward<T>(t), ss, &lock);
            }

            template <class T>
                requires(internal::is_path_convertible<T, path_char>)
            UtfOut& operator<<(const T& t) {
                write(t);
                return *this;
            }

            template <class T>
                requires(!std::is_same_v<wrap::path_char, char> && internal::is_path_convertible<T>)
            UtfOut& operator<<(const T& t) {
                write(t);
                return *this;
            }

            // write utf8 string to console. if console is not utf8, convert to utf16
            template <class U16Buf = u16string, class T>
                requires(!std::is_same_v<wrap::path_char, char> && internal::is_path_convertible<T>)
            file::file_result<void> write_no_hook(const T& p) {
                if (file.is_tty()) {
                    if constexpr (sizeof(path_char) != 1) {
                        U16Buf s;
                        utf::convert<1, 2>(p, s);
                        return file.write_console(s).transform([](auto) {});
                    }
                    else {
                        return file.write_console(p).transform([](auto) {});
                    }
                }
                return file.write_file(p).transform([](auto) {});
            }

            // write utf16 string to console. if console is not utf16, convert to utf8
            template <class U8Buf = string, class T>
                requires(internal::is_path_convertible<T, path_char>)
            file::file_result<void> write_no_hook(const T& p) {
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

            // write utf8 string to console. if console is not utf8, convert to utf16
            template <class U16String = u16string, class T>
                requires(!std::is_same_v<wrap::path_char, char> && internal::is_path_convertible<T>)
            file::file_result<void> write(const T& p) {
                if (hook_write) {
                    return hook_write(*this, p);
                }
                return write_no_hook<U16String>(p);
            }

            // write utf16 string to console. if console is not utf16, convert to utf8
            template <class U8String = string, class T>
                requires(internal::is_path_convertible<T, path_char>)
            file::file_result<void> write(const T& p) {
                if (hook_write) {
                    if constexpr (sizeof(path_char) != 1) {
                        U8String s;
                        utf::convert<2, 1>(p, s);
                        return hook_write(*this, s);
                    }
                    else {
                        return hook_write(*this, p);
                    }
                }
                return write_no_hook<U8String>(p);
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

            // thread unsafe. must be called before any output
            write_hook_fn set_hook_write(write_hook_fn hook) {
                return std::exchange(hook_write, hook);
            }

            write_hook_fn get_hook_write() const {
                return hook_write;
            }
        };

        futils_DLL_EXPORT UtfOut& STDCALL cout_wrap();
        futils_DLL_EXPORT UtfOut& STDCALL cerr_wrap();

    }  // namespace wrap
}  // namespace futils
