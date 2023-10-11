/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <platform/detect.h>
#include <file/file.h>
#ifdef UTILS_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace utils::file {
    // from golang syscall.Open
    NativeFile NativeFile::open(const wrap::path_char* filename, Flag flag, Mode mode) {
        std::uint32_t access = 0;
        switch (flag.io()) {
            case IOMode::read:
                access = GENERIC_READ;
                break;
            case IOMode::write:
                access = GENERIC_WRITE;
                break;
            case IOMode::read_write:
                access = GENERIC_READ | GENERIC_WRITE;
                break;
            default:
                break;
        }
        if (flag.create()) {
            access |= GENERIC_WRITE;
        }
        if (flag.append()) {
            access &= ~GENERIC_WRITE;
            access |= FILE_APPEND_DATA;
        }
        std::uint32_t share = 0;
        if (!flag.not_share_read()) {
            share |= FILE_SHARE_WRITE;
        }
        if (!flag.not_share_write()) {
            share |= FILE_SHARE_READ;
        }
        std::uint32_t create = 0;
        if (flag.create() && flag.exclusive()) {
            create = CREATE_NEW;
        }
        else if (flag.create() && flag.truncate()) {
            create = CREATE_ALWAYS;
        }
        else if (flag.create()) {
            create = OPEN_ALWAYS;
        }
        else if (flag.truncate()) {
            create = TRUNCATE_EXISTING;
        }
        else {
            create = OPEN_EXISTING;
        }
        std::uint32_t attr = FILE_ATTRIBUTE_NORMAL;
        if (!mode.perm().owner_write()) {
            attr = FILE_ATTRIBUTE_READONLY;
        }
        return {};
    }

}  // namespace utils::file
