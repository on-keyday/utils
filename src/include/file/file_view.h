/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// file_view - file view
// need to link libfutils
#pragma once
#include "file.h"

namespace futils {
    namespace file {
        class View {
            File f;
            Stat info;
            MMap mmap;
            // MMap は futils_DLL_EXPORT が付いているので、read_view() のような
            // 自明なメンバでも DLL 越しの呼び出しになる。1 バイト読むたびに
            // 呼ぶと 25 ns/byte 掛かった (生ポインタなら 0.13 ns/byte)。
            // 開いた時点で 1 回だけ受け取っておく。
            view::rvec mapped;

           public:
            constexpr View() {}
            View(const View&) = delete;
            View(View&&) = default;

            file_result<void> open(auto&& path) {
                if (f) {
                    return helper::either::unexpected(FileError{.method = "View::open", .err_code = map_os_error_code(ErrorCode::already_open)});
                }
                auto p = File::open(path, O_READ | O_SHARE_READ | O_CLOSE_ON_EXEC);
                if (!p) {
                    return helper::either::unexpected(p.error());
                }
                f = std::move(p.value());
                auto s = f.stat();
                if (!s) {
                    return helper::either::unexpected(s.error());
                }
                info = std::move(s.value());
                auto m = f.mmap(r_perm);
                if (!m) {
                    return {};  // ignore and fallback
                }
                mmap = std::move(*m);
                mapped = mmap.read_view();
                return {};
            }

            bool is_open() const {
                return f.is_open();
            }

            size_t size() const {
                return info.size;
            }

            std::uint8_t operator[](size_t position) const {
                if (position >= size()) {
                    return 0;
                }
                // mmap が読めていれば、ここは生ポインタの添字だけになる。
                if (!mapped.null()) {
                    return mapped[position];
                }
                // fallback
                if (!f.seek(position, SeekPoint::begin)) {
                    return 0;
                }
                std::uint8_t result = 0;
                auto res = f.read_file(view::wvec(&result, 1));
                if (!res) {
                    return 0;
                }
                if (res->size() != 1) {
                    return 0;
                }
                return result;
            }

            file_result<void> close() {
                if (mmap) {
                    mapped = view::rvec();  // unmap した先を指したままにしない
                    auto res = mmap.unmap();
                    if (!res) {
                        return helper::either::unexpected(res.error());
                    }
                }
                return f.close();
            }

            const byte* data() const {
                return mapped.data();
            }

            ~View() {
                close();
            }
        };
    }  // namespace file
}  // namespace futils
