/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// uri_normalize - normalize uri
#pragma once
#include "punycode.h"
#include "urlencode.h"
#include "../wrap/lite/enum.h"
#include "../helper/strutil.h"

namespace utils {
    namespace net {
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

        template <class URI, class String = decltype(std::declval<URI>().host)>
        NormalizeError normalize_uri(URI& uri, NormalizeFlag flag = NormalizeFlag::none) {
            if (any(flag & NormalizeFlag::host)) {
                String encoded;
                if (helper::is_valid(uri.host, number::is_in_ascii_range<std::uint8_t>) &&
                    helper::contains(uri.host, "xn--")) {
                    if (any(flag & NormalizeFlag::human_friendly)) {
                        if (!punycode::decode_host(uri.host, encoded)) {
                            return NormalizeError::decode_host;
                        }
                        uri.host = std::move(encoded);
                    }
                }
                else {
                    if (!any(flag & NormalizeFlag::human_friendly)) {
                        if (!net::punycode::encode_host(uri.host, encoded)) {
                            return NormalizeError::encode_host;
                        }
                        uri.host = std::move(encoded);
                    }
                }
            }
            auto encode_each = [&](auto& input) {
                String encoded;
                if (helper::is_valid(input, number::is_in_ascii_range<std::uint8_t>) &&
                    helper::contains(input, "%")) {
                    if (any(flag & NormalizeFlag::human_friendly)) {
                        if (net::urlenc::decode(input, encoded)) {
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
                if (!net::urlenc::encode(input, encoded, net::urlenc::encodeURI())) {
                    return false;
                }
                input = std::move(encoded);
                return true;
            };
            if (any(flag & NormalizeFlag::path)) {
                if (!encode_each(uri.path)) {
                    return NormalizeError::encode_decode_path;
                }
                if (!encode_each(uri.query)) {
                    return NormalizeError::encode_decode_query;
                }
            }
            return NormalizeError::none;
        }
    }  // namespace net
}  // namespace utils