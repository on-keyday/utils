/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/file/file_view.h"

#ifdef _WIN32
#define _fseek(file, pos, origin) ::_fseeki64(file, static_cast<long long>(pos), origin)
#else
#define _fseek(file, pos, origin) ::fseek(file, pos, origin)
#endif

namespace utils {
    namespace file {
        bool View::open(const wrap::path_char* path) {
            return this->info.open(path);
        }

        size_t View::size() const {
            return this->info.stat.st_size;
        }

        void View::close() {
            this->info.close();
        }

        bool View::is_open() const {
            return this->info.is_open();
        }

        std::uint8_t View::operator[](size_t position) const {
            if (position >= info.stat.st_size) {
                return 0;
            }
            if (info.mapptr) {
                return info.mapptr[position];
            }
            else if (info.file) {
                if (_fseek(info.file, position, SEEK_SET) != 0) {
                    return 0;
                }
                return fgetc(info.file);
            }
            return 0;
        }
    }  // namespace file
}  // namespace utils
