/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <utility>
#include <array>

namespace utils {
    namespace hpack {

        using sheader_t = std::pair<const char*, const char*>;

        constexpr size_t predefined_header_size = 62;

        // clang-format off
        constexpr std::array<sheader_t, predefined_header_size> predefined_header = {
            sheader_t("INVALIDINDEX", "INVALIDINDEX"),
            sheader_t(":authority", ""),
            sheader_t(":method", "GET"),
            sheader_t(":method", "POST"),
            sheader_t(":path", "/"),
            sheader_t(":path", "/index.html"),
            sheader_t(":scheme", "http"),
            sheader_t(":scheme", "https"),
            sheader_t(":status", "200"),
            sheader_t(":status", "204"),
            sheader_t(":status", "206"),
            sheader_t(":status", "304"),
            sheader_t(":status", "400"),
            sheader_t(":status", "404"),
            sheader_t(":status", "500"),
            sheader_t("accept-charset", ""),
            sheader_t("accept-encoding", "gzip, deflate"),
            sheader_t("accept-language", ""),
            sheader_t("accept-ranges", ""),
            sheader_t("accept", ""),
            sheader_t("access-control-allow-origin", ""),
            sheader_t("age", ""),
            sheader_t("allow", ""),
            sheader_t("authorization", ""),
            sheader_t("cache-control", ""),
            sheader_t("content-disposition", ""),
            sheader_t("content-encoding", ""),
            sheader_t("content-language", ""),
            sheader_t("content-length", ""),
            sheader_t("content-location", ""),
            sheader_t("content-range", ""),
            sheader_t("content-type", ""),
            sheader_t("cookie", ""),
            sheader_t("date", ""),
            sheader_t("etag", ""),
            sheader_t("expect", ""),
            sheader_t("expires", ""),
            sheader_t("from", ""),
            sheader_t("host", ""),
            sheader_t("if-match", ""),
            sheader_t("if-modified-since", ""),
            sheader_t("if-none-match", ""),
            sheader_t("if-range", ""),
            sheader_t("if-unmodified-since", ""),
            sheader_t("last-modified", ""),
            sheader_t("link", ""),
            sheader_t("location", ""),
            sheader_t("max-forwards", ""),
            sheader_t("proxy-authenticate", ""),
            sheader_t("proxy-authorization", ""),
            sheader_t("range", ""),
            sheader_t("referer", ""),
            sheader_t("refresh", ""),
            sheader_t("retry-after", ""),
            sheader_t("server", ""),
            sheader_t("set-cookie", ""),
            sheader_t("strict-transport-security", ""),
            sheader_t("transfer-encoding", ""),
            sheader_t("user-agent", ""),
            sheader_t("vary", ""),
            sheader_t("via", ""),
            sheader_t("www-authenticate", ""),
        };
        // clang-format on
    }  // namespace hpack
}  // namespace utils
