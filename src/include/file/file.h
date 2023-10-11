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
            : flags(value) {}

        constexpr operator std::uint16_t() const {
            return flags.as_value();
        }

        bits_flag_alias_method(flags, 0, owner_read);
        bits_flag_alias_method(flags, 1, owner_write);
        bits_flag_alias_method(flags, 2, owner_execute);
        bits_flag_alias_method(flags, 3, group_read);
        bits_flag_alias_method(flags, 4, group_write);
        bits_flag_alias_method(flags, 5, group_execute);
        bits_flag_alias_method(flags, 6, other_read);
        bits_flag_alias_method(flags, 7, other_write);
        bits_flag_alias_method(flags, 8, other_execute);
    };

    struct Mode {
       private:
        binary::flags_t<std::uint32_t, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 10, 9> flags;

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
        bits_flag_alias_method_with_enum(flags, 14, perm, Perm);

        constexpr Mode() = default;
        constexpr Mode(std::uint32_t value)
            : flags(value) {}

        constexpr operator std::uint32_t() const {
            return flags.as_value();
        }
    };

    struct Flag {
       private:
        binary::flags_t<std::uint16_t, 1, 1, 1, 1, 1, 1, 1, 2> flags;

       public:
        bits_flag_alias_method(flags, 0, create);
        bits_flag_alias_method(flags, 1, append);
        bits_flag_alias_method(flags, 2, truncate);
        bits_flag_alias_method(flags, 3, exclusive);
        bits_flag_alias_method(flags, 4, sync);
        bits_flag_alias_method(flags, 5, not_share_read);
        bits_flag_alias_method(flags, 6, not_share_write);
        bits_flag_alias_method_with_enum(flags, 7, io, IOMode);
    };

    struct utils_DLL_EXPORT NativeFile {
       private:
        std::uintptr_t handle = ~0;

        constexpr NativeFile(std::uintptr_t handle)
            : handle(handle) {}

       public:
        constexpr NativeFile() = default;

        constexpr NativeFile(NativeFile&& other)
            : handle(std::exchange(other.handle, ~0)) {}

        constexpr NativeFile& operator=(NativeFile&& other) {
            if (this == &other) {
                return *this;
            }
            handle = std::exchange(other.handle, ~0);
            return *this;
        }

        constexpr explicit operator bool() const {
            return handle != ~0;
        }

        static NativeFile open(const wrap::path_char* filename, Flag flag, Mode mode);
    };

    struct File {
        NativeFile file;
    };
}  // namespace utils::file
