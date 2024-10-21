/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// qpack_header
#pragma once
#include <core/byte.h>
#include <core/strlen.h>
#include <cstdint>
#include <utility>
#include <view/iovec.h>

namespace futils {
    namespace qpack {
        namespace header {
            namespace internal {
                // see https://datatracker.ietf.org/doc/html/rfc9204#appendix-A
                // Appendix A. Static Table
                // clang-format off
                constexpr byte header_text[] = R"(
:authority
:path$/
age$0
content-disposition
content-length$0
cookie
date
etag
if-modified-since
if-none-match
last-modified
link
location
referer
set-cookie
:method$CONNECT$DELETE$GET$HEAD$OPTIONS$POST$PUT
:scheme$http$https
:status$103$200$304$404$503
accept$*/*$application/dns-message
accept-encoding$gzip, deflate, br
accept-ranges$bytes
access-control-allow-headers$cache-control$content-type
access-control-allow-origin$*
cache-control$max-age=0$max-age=2592000$max-age=604800$no-cache$no-store$public, max-age=31536000
content-encoding$br$gzip
content-type$application/dns-message$application/javascript$application/json$application/x-www-form-urlencoded$image/gif$image/jpeg$image/png$text/css$text/html; charset=utf-8$text/plain$text/plain;charset=utf-8
range$bytes=0-
strict-transport-security$max-age=31536000$max-age=31536000; includesubdomains$max-age=31536000; includesubdomains; preload
vary$accept-encoding$origin
x-content-type-options$nosniff
x-xss-protection$1; mode=block
:status$100$204$206$302$400$403$421$425$500
accept-language
access-control-allow-credentials$FALSE$TRUE
access-control-allow-headers$*
access-control-allow-methods$get$get, post, options$options
access-control-expose-headers$content-length
access-control-request-headers$content-type
access-control-request-method$get$post
alt-svc$clear
authorization
content-security-policy$script-src 'none';object-src 'none';base-uri 'none'
early-data$1
expect-ct
forwarded
if-range
origin
purpose$prefetch
server
timing-allow-origin$*
upgrade-insecure-requests$1
user-agent
x-forwarded-for
x-frame-options$deny$sameorigin
)";
                // clang-format on

                template <class C>
                constexpr auto fill_null() {
                    struct L {
                        C data[sizeof(header_text) - 1];
                    } l;
                    for (auto i = 0; i < sizeof(header_text) - 1; i++) {
                        if (header_text[i] == '\n' || header_text[i] == '$') {
                            l.data[i] = 0;
                        }
                        else {
                            l.data[i] = header_text[i];
                        }
                    }
                    return l;
                }

                constexpr auto offset_set() {
                    struct L {
                        struct {
                            std::uint16_t head_offset = 0;
                            std::uint16_t value_offset = 0;
                        } offset[99];
                    } l;
                    auto ofs = 1;
                    bool has_value = false;
                    auto read = [&](std::uint16_t& offset) {
                        offset = ofs;
                        while (header_text[ofs] != '\n' && header_text[ofs] != '$') {
                            ofs++;
                        }
                        has_value = header_text[ofs] == '$';
                        ofs++;
                    };
                    for (auto i = 0; i < 99;) {
                        auto cur_i = i;
                        read(l.offset[cur_i].head_offset);
                        if (!has_value) {
                            i++;
                        }
                        while (has_value) {
                            l.offset[i].head_offset = l.offset[cur_i].head_offset;
                            read(l.offset[i].value_offset);
                            i++;
                        }
                    }
                    return l;
                }

                template <class C>
                constexpr auto cvt = fill_null<C>();
                constexpr auto ofs = offset_set();

                template <class C>
                constexpr auto gen_tables() {
                    struct L {
                        std::pair<const C*, const C*> table[99];
                    } l;
                    auto& ref = cvt<C>;
                    for (auto i = 0; i < 99; i++) {
                        l.table[i].first = ref.data + ofs.offset[i].head_offset;
                        l.table[i].second = ref.data + ofs.offset[i].value_offset;
                    }
                    return l;
                }

            }  // namespace internal

            constexpr auto predefined_header_count = 99;
            template <class C = byte>
            constexpr auto predefined_headers = internal::gen_tables<C>();

            constexpr auto max_static_header_value_len = [] {
                size_t hdr_size = 0;
                size_t val_size = 0;
                for (auto i = 0; i < predefined_header_count; i++) {
                    auto hdr_len = futils::strlen(predefined_headers<byte>.table[i].first);
                    auto val_len = futils::strlen(predefined_headers<byte>.table[i].second);
                    if (hdr_len > hdr_size) {
                        hdr_size = hdr_len;
                    }
                    if (val_len > val_size) {
                        val_size = val_len;
                    }
                }
                return std::pair{hdr_size, val_size};
            }();

            namespace internal {
                template <size_t row, size_t col, class C>
                constexpr auto gen_hash_table(auto&& hash) {
                    struct W {
                        struct {
                            byte table[col]{};
                            size_t i = 0;
                        } r[row];
                    } m;
                    struct L {
                        byte table[row][col]{};
                        byte row_ = row;
                        byte col_ = col;
                    } l;
                    for (auto i = 0; i < 99; i++) {
                        auto [idx, dup] = hash(predefined_headers<C>.table[i], i);
                        if (dup) {
                            continue;
                        }
                        auto& tb = m.r[idx];
                        tb.table[tb.i] = i;
                        tb.i++;
                    }
                    for (auto i = 0; i < row; i++) {
                        auto& b = m.r[i];
                        for (auto k = 0; k < b.i; k++) {
                            l.table[i][k] = b.table[k] + 1;
                        }
                    }
                    return l;
                }

                // candidate:
                // 20,10
                // 33,8
                // 39,7
                // 41,7
                // 46,5 <
                constexpr auto frow = 48;

                template <class T>
                concept to_rvec = requires(T t) {
                    { view::rvec(t) } -> std::convertible_to<view::rvec>;
                };

                template <class T>
                concept has_iter = requires(T t) {
                    { t.begin() };
                    { t.end() };
                    { std::declval<decltype(t.begin())>() != std::declval<decltype(t.end())>() };
                    { *t.begin() } -> std::convertible_to<byte>;
                    { std::declval<decltype(t.begin())>()++ };
                };

                template <class T>
                concept index_array_like = requires(T t) {
                    { t[1] } -> std::convertible_to<size_t>;
                    { t.size() } -> std::convertible_to<size_t>;
                };

                template <size_t scan_limit>
                constexpr view::rvec scan_for_hash(auto&& x, byte buf[scan_limit + 1]) {
                    if constexpr (to_rvec<decltype(x)>) {
                        return view::rvec(x);
                    }
                    else if constexpr (has_iter<decltype(x)>) {
                        auto it = x.begin();
                        auto end = x.end();
                        size_t i = 0;
                        for (; i < scan_limit + 1 && it != end; i++, it++) {
                            buf[i] = *it;
                        }
                        return view::rvec(buf, i);
                    }
                    else if constexpr (index_array_like<decltype(x)>) {
                        size_t i = 0;
                        for (; i < scan_limit + 1 && i < x.size(); i++) {
                            buf[i] = x[i];
                        }
                        return view::rvec(buf, i);
                    }
                    else {
                        return view::rvec(x);
                    }
                }

                constexpr auto field_hash_index(auto&& head, auto&& value) {
                    std::uint64_t h = 0;
                    byte buf[max_static_header_value_len.second * 2 + 2]{};
                    auto hdr = scan_for_hash<max_static_header_value_len.first>(head, buf);
                    auto val_ = scan_for_hash<max_static_header_value_len.second>(value, buf + hdr.size());
                    view::hash(hdr, &h);
                    view::hash(val_, &h);
                    return h % frow;
                }
                constexpr auto field_hash_table = gen_hash_table<frow, 5, byte>([](std::pair<const byte*, const byte*> ptr, size_t i) {
                    return std::pair{field_hash_index(view::rvec(ptr.first), view::rvec(ptr.second)), false};
                });

                constexpr auto hrow = 38;
                constexpr auto header_hash_index(auto&& head) {
                    std::uint64_t h = 0;
                    byte buf[max_static_header_value_len.first + 1]{};
                    auto hdr = scan_for_hash<max_static_header_value_len.first>(head, buf);
                    view::hash(hdr, &h);
                    return h % hrow;
                }
                constexpr auto header_hash_table = gen_hash_table<hrow, 4, byte>([](std::pair<const byte*, const byte*> ptr, size_t i) {
                    if (i != 0 && ptr.first == predefined_headers<byte>.table[i - 1].first) {
                        return std::pair{0, true};
                    }
                    return std::pair{int(header_hash_index(view::rvec(ptr.first))), false};
                });
            }  // namespace internal
        }  // namespace header
    }  // namespace qpack
}  // namespace futils
