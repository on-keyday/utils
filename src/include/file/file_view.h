/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// file_view - file view
// need to link libutils
#pragma once
#include "platform.h"
#include "../wrap/light/string.h"
#include "../utf/convert.h"

namespace utils {
    namespace file {
        class DLL View {
            mutable platform::ReadFileInfo info;

           public:
            constexpr View() {}
            View(const View&) = delete;
            View(View&& v) {
                info = std::move(v.info);
                v.info = platform::ReadFileInfo{};
            }

            bool open(const wrap::path_char* path);

            template <class Tmpbuf = wrap::path_string, class String>
            bool open(String&& str) {
                Tmpbuf result;
                if (!utf::convert(str, result)) {
                    return false;
                }
                return open(static_cast<const wrap::path_char*>(result.c_str()));
            }

            bool is_open() const;

            size_t size() const;

            std::uint8_t operator[](size_t position) const;

            void close();
        };
    }  // namespace file
}  // namespace utils
