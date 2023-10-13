/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/windows/dllexport_header.h>
#include <cstdint>
#include <utility>
#include <binary/flags.h>
#include <wrap/light/char.h>
#include <wrap/light/string.h>
#include <error/error.h>
#include <helper/expected.h>
#include <unicode/utf/convert.h>

namespace utils::file {

    enum class IOMode : std::uint8_t {
        read = 0,
        write = 1,
        read_write = 2,
    };

    struct Perm {
       private:
        binary::flags_t<std::uint16_t, 16 - 9, 1, 1, 1, 1, 1, 1, 1, 1, 1> flags;

       public:
        constexpr Perm() = default;
        constexpr Perm(std::uint16_t value)
            : flags(value & 0777) {}

        constexpr operator std::uint16_t() const {
            return flags.as_value();
        }

        bits_flag_alias_method(flags, 1, owner_read);
        bits_flag_alias_method(flags, 2, owner_write);
        bits_flag_alias_method(flags, 3, owner_execute);
        bits_flag_alias_method(flags, 4, group_read);
        bits_flag_alias_method(flags, 5, group_write);
        bits_flag_alias_method(flags, 6, group_execute);
        bits_flag_alias_method(flags, 7, other_read);
        bits_flag_alias_method(flags, 8, other_write);
        bits_flag_alias_method(flags, 9, other_execute);

        constexpr void set_write(bool value) {
            set_owner_write(value);
            set_group_write(value);
            set_other_write(value);
        }

        constexpr void set_read(bool value) {
            set_owner_read(value);
            set_group_read(value);
            set_other_read(value);
        }

        constexpr void set_execute(bool value) {
            set_owner_execute(value);
            set_group_execute(value);
            set_other_execute(value);
        }
    };

    constexpr auto full_perm = Perm(0777);
    constexpr auto rw_perm = Perm(0666);
    constexpr auto r_perm = Perm(0444);

    struct Mode {
       private:
        binary::flags_t<std::uint32_t, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9> flags;

        bits_flag_alias_method(flags, 14, reserved);

       public:
        bits_flag_alias_method(flags, 0, directory);
        bits_flag_alias_method(flags, 1, append);
        bits_flag_alias_method(flags, 2, exclusive);
        bits_flag_alias_method(flags, 3, temporary);
        bits_flag_alias_method(flags, 4, symlink);
        bits_flag_alias_method(flags, 5, device);
        bits_flag_alias_method(flags, 6, pipe);
        bits_flag_alias_method(flags, 7, socket);
        bits_flag_alias_method(flags, 8, uid);
        bits_flag_alias_method(flags, 9, gid);
        bits_flag_alias_method(flags, 10, char_device);
        bits_flag_alias_method(flags, 11, sticky);
        bits_flag_alias_method(flags, 12, irregular);
        bits_flag_alias_method_with_enum(flags, 15, perm, Perm);
        // extension
        // if use Mode as argument of open, set terminal to false
        bits_flag_alias_method(flags, 13, terminal);

        constexpr bool regular() const {
            return !directory() && !symlink() && !device() && !pipe() && !socket();
        }

        void disable_extension() {
            set_terminal(false);
        }

        constexpr Mode()
            : flags(rw_perm) {}

        constexpr Mode(std::uint32_t value)
            : flags(value) {
            set_reserved(0);
        }

        constexpr Mode(Perm perm)
            : flags(perm) {
            set_reserved(0);
        }

        constexpr operator std::uint32_t() const {
            return flags.as_value();
        }
    };

    // NOTE: if you want to use O_TMPFILE, you should use set_temporary() of Mode
    struct Flag {
       private:
        binary::flags_t<std::uint32_t, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 32 - 18> flags;

        bits_flag_alias_method(flags, 17, reserved);

       public:
        bits_flag_alias_method(flags, 0, create);
        bits_flag_alias_method(flags, 1, append);
        bits_flag_alias_method(flags, 2, truncate);
        bits_flag_alias_method(flags, 3, exclusive);
        bits_flag_alias_method(flags, 4, sync);
        bits_flag_alias_method(flags, 5, share_read);
        bits_flag_alias_method(flags, 6, share_write);
        bits_flag_alias_method(flags, 7, close_on_exec);
        bits_flag_alias_method(flags, 8, non_block);
        bits_flag_alias_method(flags, 9, async);
        bits_flag_alias_method(flags, 10, no_ctty);
        bits_flag_alias_method(flags, 11, direct);
        bits_flag_alias_method(flags, 12, no_follow);
        bits_flag_alias_method(flags, 13, no_access_time);
        bits_flag_alias_method(flags, 14, dsync);
        bits_flag_alias_method_with_enum(flags, 15, io, IOMode);
        bits_flag_alias_method(flags, 15, share_delete);

        constexpr Flag() noexcept = default;

        constexpr Flag(std::uint32_t value)
            : flags(value) {
            set_reserved(0);
        }

        constexpr operator std::uint32_t() const {
            return flags.as_value();
        }
    };

    namespace internal {
        constexpr Flag make_spec_flag(auto&& fn) {
            Flag flag;
            fn(flag);
            return flag;
        }

        template <class T>
        concept has_c_str_for_path = requires(T t) {
            { t.c_str() } -> std::convertible_to<const wrap::path_char*>;
        };
    }  // namespace internal

    constexpr auto O_CREAT = internal::make_spec_flag([](Flag& flag) {
        flag.set_create(true);
    });
    constexpr auto O_APPEND = internal::make_spec_flag([](Flag& flag) {
        flag.set_append(true);
    });
    constexpr auto O_TRUNC = internal::make_spec_flag([](Flag& flag) {
        flag.set_truncate(true);
    });
    constexpr auto O_EXCL = internal::make_spec_flag([](Flag& flag) {
        flag.set_exclusive(true);
    });
    constexpr auto O_SYNC = internal::make_spec_flag([](Flag& flag) {
        flag.set_sync(true);
    });
    constexpr auto O_SHARE_READ = internal::make_spec_flag([](Flag& flag) {
        flag.set_share_read(true);
    });
    constexpr auto O_SHARE_WRITE = internal::make_spec_flag([](Flag& flag) {
        flag.set_share_write(true);
    });
    constexpr auto O_CLOSE_ON_EXEC = internal::make_spec_flag([](Flag& flag) {
        flag.set_close_on_exec(true);
    });
    constexpr auto O_NON_BLOCK = internal::make_spec_flag([](Flag& flag) {
        flag.set_non_block(true);
    });
    constexpr auto O_ASYNC = internal::make_spec_flag([](Flag& flag) {
        flag.set_async(true);
    });
    constexpr auto O_NO_CTTY = internal::make_spec_flag([](Flag& flag) {
        flag.set_no_ctty(true);
    });
    constexpr auto O_DIRECT = internal::make_spec_flag([](Flag& flag) {
        flag.set_direct(true);
    });
    constexpr auto O_NO_FOLLOW = internal::make_spec_flag([](Flag& flag) {
        flag.set_no_follow(true);
    });
    constexpr auto O_NO_ACCESS_TIME = internal::make_spec_flag([](Flag& flag) {
        flag.set_no_access_time(true);
    });
    constexpr auto O_DSYNC = internal::make_spec_flag([](Flag& flag) {
        flag.set_dsync(true);
    });
    constexpr auto O_READ = internal::make_spec_flag([](Flag& flag) {
        flag.set_io(IOMode::read);
    });
    constexpr auto O_WRITE = internal::make_spec_flag([](Flag& flag) {
        flag.set_io(IOMode::write);
    });
    constexpr auto O_READ_WRITE = internal::make_spec_flag([](Flag& flag) {
        flag.set_io(IOMode::read_write);
    });

    constexpr auto O_READ_DEFAULT = Flag(O_READ | O_SHARE_READ | O_SHARE_WRITE | O_CLOSE_ON_EXEC);
    constexpr auto O_WRITE_DEFAULT = Flag(O_WRITE | O_SHARE_WRITE | O_SHARE_READ | O_CLOSE_ON_EXEC);

    utils_DLL_EXPORT void STDCALL format_os_error(helper::IPushBacker<> pb, std::int64_t code);
    enum class ErrorCode {
        already_open,
    };

    utils_DLL_EXPORT std::int64_t STDCALL map_os_error_code(ErrorCode code);

    struct FileError {
        const char* method = nullptr;
        std::int64_t err_code = 0;

        void error(auto&& pb) const {
            strutil::appends(pb, "FileError: ", method, " failed with code ");
            number::to_string(pb, err_code);
            strutil::appends(pb, ": ");
            format_os_error(pb, err_code);
        }

        auto code() const {
            return err_code;
        }

        error::Category category() const {
            return error::Category::os;
        }
    };

    struct Time {
        std::uint64_t sec = 0;
        std::uint32_t nsec = 0;
    };

    struct Stat {
        Mode mode;
        Flag flag;
        std::uint64_t size = 0;
        Time create_time;
        Time access_time;
        Time mod_time;
    };

    template <class T>
    using file_result = helper::either::expected<T, FileError>;

    struct utils_DLL_EXPORT MMap {
       private:
        byte* ptr = nullptr;
        std::uintptr_t os_spec = 0;
        std::size_t file_len = 0;
        Mode mode;
        friend struct utils_DLL_EXPORT File;
        constexpr MMap(byte* ptr, std::uintptr_t spec, std::size_t f_len, Mode m)
            : ptr(ptr), os_spec(spec), file_len(f_len), mode(m) {}

       public:
        constexpr MMap() = default;

        constexpr MMap(MMap&& other)
            : ptr(std::exchange(other.ptr, nullptr)),
              os_spec(std::exchange(other.os_spec, 0)),
              file_len(std::exchange(other.file_len, 0)),
              mode(std::exchange(other.mode, Mode())) {}

        constexpr MMap& operator=(MMap&& other) {
            if (this == &other) {
                return *this;
            }
            unmap();  // ignore result
            ptr = std::exchange(other.ptr, nullptr);
            os_spec = std::exchange(other.os_spec, 0);
            file_len = std::exchange(other.file_len, 0);
            mode = std::exchange(other.mode, Mode());
            return *this;
        }

        file_result<void> unmap();

        ~MMap() {
            unmap();  // ignore result
        }

        constexpr explicit operator bool() const {
            return ptr != nullptr;
        }

        view::wvec write_view() const {
            if (!mode.perm().owner_write()) {
                return view::wvec();
            }
            return view::wvec(ptr, file_len);
        }

        view::rvec read_view() const {
            if (!mode.perm().owner_read()) {
                return view::rvec();
            }
            return view::rvec(ptr, file_len);
        }
    };

    enum class SeekPoint {
        begin,
        current,
        end,
    };

    // NOTE(on-keyday): this is reinterpreted as OVERLAPPED if windows
    //                  on linux, this is not used yet
    struct utils_DLL_EXPORT NonBlockContext {
       private:
        alignas(alignof(std::uintptr_t)) byte data[sizeof(std::uintptr_t) * 4]{};

       public:
        bool complete() const;
        size_t transferred() const;
    };

    struct ConsoleBuffer {
       private:
        std::uintptr_t handle = ~0;

        friend struct utils_DLL_EXPORT File;
        std::uint32_t base_flag = 0;
        byte rec[2]{};  // for linux
        bool zero_input = false;
        constexpr ConsoleBuffer(std::uintptr_t handle, std::uint32_t base_flag, byte a = 0, byte b = 0)
            : handle(handle), base_flag(base_flag) {
            rec[0] = a;
            rec[1] = b;
        }

        constexpr ConsoleBuffer(std::uintptr_t handle)
            : handle(handle) {}

       public:
        file_result<void> interact(void*, void (*callback)(wrap::path_char, void*));
        ~ConsoleBuffer();
    };

    struct utils_DLL_EXPORT File {
       private:
        std::uintptr_t handle = ~0;

        constexpr File(std::uintptr_t handle)
            : handle(handle) {}

       public:
        constexpr File() = default;

        constexpr File(File&& other)
            : handle(std::exchange(other.handle, ~0)) {}

        constexpr File& operator=(File&& other) {
            if (this == &other) {
                return *this;
            }
            handle = std::exchange(other.handle, ~0);
            return *this;
        }

        constexpr explicit operator bool() const {
            return handle != ~0;
        }

        constexpr bool is_open() const {
            return handle != ~0;
        }

        static file_result<File> open(const wrap::path_char* filename, Flag flag = O_READ_DEFAULT, Mode mode = r_perm);

        template <class T>
            requires internal::has_c_str_for_path<T>
        static file_result<File> open(T&& filename, Flag flag = O_READ_DEFAULT, Mode mode = r_perm) {
            return open(static_cast<const wrap::path_char*>(filename.c_str()), flag, mode);
        }

        template <class Buffer = wrap::path_string, class T>
        static file_result<File> open(T&& filename, Flag flag = O_READ_DEFAULT, Mode mode = r_perm) {
            Buffer buffer;
            utf::convert<0, sizeof(wrap::path_char)>(filename, buffer);
            return open(buffer.c_str(), flag, mode);
        }

        file_result<void> close();

        ~File() {
            close();  // ignore result
        }

        file_result<Stat> stat() const;

        bool is_directory() const;
        bool is_tty() const;

        size_t size() const;

        // use m.temporary() as private mapping flag
        file_result<MMap> mmap(Mode m) const;

        file_result<view::rvec> write_file(view::rvec w, NonBlockContext* n = nullptr) const;
        file_result<view::basic_rvec<wrap::path_char>> write_console(view::basic_rvec<wrap::path_char> w) const;

        file_result<view::wvec> read_file(view::wvec w, NonBlockContext* n = nullptr) const;
        file_result<view::basic_wvec<wrap::path_char>> read_console(view::basic_wvec<wrap::path_char> w) const;

        file_result<void> seek(std::int64_t offset, SeekPoint point) const;

        size_t pos() const;

        // on Windows, use DeviceIoControl
        // on linux, use ioctl.use code and in_arg only
        file_result<void> ioctl(std::uint64_t code, void* in_arg, std::size_t in_len = 0, void* out_arg = nullptr, std::size_t out_len = 0) const;

        file_result<ConsoleBuffer> interactive_console_read() const;

        static const File& stdin_file();
        static const File& stdout_file();
        static const File& stderr_file();

        // these methods are only guideline, not guarantee
        file_result<bool> can_non_block_read() const;
        file_result<bool> can_non_block_write() const;

        // set ansi escape sequence
        file_result<void> set_virtual_terminal_output(bool flag = true) const;
        file_result<void> set_virtual_terminal_input(bool flag = true) const;
    };

}  // namespace utils::file
