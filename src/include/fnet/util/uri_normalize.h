/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// uri_normalize - normalize uri
#pragma once
#include "punycode.h"
#include "urlencode.h"
#include <wrap/light/enum.h>
#include <strutil/strutil.h>

namespace futils {
    namespace uri {
        enum class NormalizeFlag {
            none,
            host = 0x1,
            path = 0x2,
            human_friendly = 0x4,
        };

        DEFINE_ENUM_FLAGOP(NormalizeFlag)

        enum class NormalizeError {
            none,
            encode_host,
            decode_host,
            encode_decode_path,
            encode_decode_query,
        };

        BEGIN_ENUM_STRING_MSG(NormalizeError, error_msg)
        ENUM_STRING_MSG(NormalizeError::encode_host, "failed to encode host")
        ENUM_STRING_MSG(NormalizeError::decode_host, "failed to decode host")
        ENUM_STRING_MSG(NormalizeError::encode_decode_path, "failed to encode/decode path")
        ENUM_STRING_MSG(NormalizeError::encode_decode_query, "failed to encode/decode error")
        END_ENUM_STRING_MSG(nullptr)

        template <class URI, class String = decltype(std::declval<URI>().hostname), class UrlEnc = decltype(urlenc::pathUnescape())>
        NormalizeError normalize_uri(URI& uri, NormalizeFlag flag = NormalizeFlag::none, UrlEnc&& enc = urlenc::pathUnescape()) {
            if (any(flag & NormalizeFlag::host)) {
                String encoded;
                if (strutil::validate(uri.hostname, true, number::is_in_ascii_range<std::uint8_t>) &&
                    strutil::contains(uri.hostname, "xn--")) {
                    if (any(flag & NormalizeFlag::human_friendly)) {
                        if (!punycode::decode_host(uri.hostname, encoded)) {
                            return NormalizeError::decode_host;
                        }
                        uri.hostname = std::move(encoded);
                    }
                }
                else {
                    if (!any(flag & NormalizeFlag::human_friendly)) {
                        if (!punycode::encode_host(uri.hostname, encoded)) {
                            return NormalizeError::encode_host;
                        }
                        uri.hostname = std::move(encoded);
                    }
                }
            }
            auto encode_each = [&](auto& input, bool query) {
                String encoded;
                if (strutil::validate(input, true, number::is_in_ascii_range<std::uint8_t>) &&
                    strutil::contains(input, "%")) {
                    if (any(flag & NormalizeFlag::human_friendly)) {
                        if (urlenc::decode(input, encoded)) {
                            input = std::move(encoded);
                            return true;
                        }
                    }
                    else {
                        return true;
                    }
                    encoded = {};
                }
                if (any(flag & NormalizeFlag::human_friendly)) {
                    return true;
                }
                if constexpr (std::is_same_v<UrlEnc, decltype(urlenc::pathUnescape())>) {
                    if (query) {
                        if (!urlenc::encode(input, encoded, urlenc::queryUnescape())) {
                            return false;
                        }
                    }
                    else {
                        if (!urlenc::encode(input, encoded, enc)) {
                            return false;
                        }
                    }
                }
                else {
                    if (!urlenc::encode(input, encoded, enc)) {
                        return false;
                    }
                }
                input = std::move(encoded);
                return true;
            };
            if (any(flag & NormalizeFlag::path)) {
                if (!encode_each(uri.path, false)) {
                    return NormalizeError::encode_decode_path;
                }
                if (!encode_each(uri.query, true)) {
                    return NormalizeError::encode_decode_query;
                }
            }
            return NormalizeError::none;
        }
    }  // namespace uri
}  // namespace futils
