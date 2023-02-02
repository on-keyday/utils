/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../types.h"
#include "../varint.h"

namespace utils {
    namespace dnet::quic::frame {

        struct Frame {
            FrameFlags type;  // only parse
            constexpr bool parse(io::reader& r) noexcept {
                auto [data, ok] = varint::read(r);
                if (!ok || !varint::is_min_len(data)) {
                    return false;
                }
                type = {data};
                return true;
            }

            constexpr bool parse_check(io::reader& r, FrameType ty) noexcept {
                return parse(r) && type.type() == ty;
            }

            static constexpr size_t type_minlen(FrameFlags flags) {
                return varint::min_len(flags.value);
            }

            static constexpr bool type_minwrite(io::writer& w, FrameFlags flags) noexcept {
                return varint::write(w, varint::Value(flags.value));
            }

            constexpr size_t len(bool = false) const noexcept {
                return type_minlen(type);
            }

            constexpr bool render(io::writer& w) noexcept {
                return type_minwrite(w, type);
            }

            static constexpr size_t rvec_field_count() {
                return 0;
            }

            constexpr void visit_rvec(auto&& cb) const noexcept {}
            constexpr void visit_rvec(auto&& cb) noexcept {}
        };

        namespace test {
            constexpr bool do_test(auto&& frame, auto&& cb) {
                byte data[100]{};
                io::writer w{data};
                auto prev = frame.len(false);
                FrameType ty = frame.get_type(false);
                bool ok1 = frame.render(w);
                if (!ok1) {
                    return false;
                }
                io::reader r{w.written()};
                bool ok2 = frame.parse(r);
                if (!ok2) {
                    return false;
                }
                bool ok3 = r.read().size() == frame.len(true) &&
                           w.written().size() == prev;
                if (!ok3) {
                    return false;
                }
                bool ok4 = frame.get_type(false) == ty;
                if (!ok4) {
                    return false;
                }
                return cb();
            }
        }  // namespace test
    }      // namespace dnet::quic::frame
}  // namespace utils
