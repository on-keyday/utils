/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../http1/http1.h"
#include <fnet/error.h>
#include <fnet/util/base64.h>

namespace futils::fnet::http2 {

    // h2c upgrade is deprecated by RFC9113 but still support
    // use prior knowledge to use h2c
    template <class TmpString = flex_storage, class Method, class Path, class Header>
    constexpr error::Error open_h2c(http1::HTTP1& h1, Method&& method, Path&& path, Header&& header,
                                    view::rvec settings) {
        auto err = h1.write_request(std::forward<Method>(method), std::forward<Path>(path), [](auto&& set_header) {
            TmpString settings_base64;
            base64::encode(settings, settings_base64, '-', '_', true);
            set_header("Upgrade", "h2c, HTTP2-Settings");
            set_header("Connection", "Upgrade");
            set_header("HTTP2-Settings", settings_base64);
            http1::apply_call_or_iter(std::forward<Header>(header), set_header);
        });
        if (err) {
            return err;
        }
        return error::none;
    }

    template <class TmpString = flex_storage, class Method, class Path, class Header>
    constexpr error::Error read_h2c_request(http1::HTTP1& h1, Method&& method, Path&& path, Header&& header,
                                            TmpString& settings, bool& has_h2c_upgrade) {
        bool has_h2c = false, has_settings = false, has_upgrade = false;
        auto err = h1.read_request(method, path, [&](auto&& seq, http1::FieldRange range) {
            auto save = seq.rptr;
            seq.rptr = range.key.start;
            if (seq.seek_if("Upgrade") && seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                while (seq.rptr < range.value.end) {
                    if (seq.seek_if("h2c")) {
                        has_h2c = true;
                        break;
                    }
                }
            }
            else if (seq.seek_if("HTTP2-Settings") && seq.rptr == range.key.end) {
                if (!base64::decode(seq, settings, '-', '_', true)) {
                    return http1::header::HeaderError::invalid_header_value;
                }
                if (seq.rptr != range.value.end) {
                    return http1::header::HeaderError::invalid_header_value;
                }
                has_settings = true;
            }
            else if (seq.seek_if("Connection") && seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                while (seq.rptr < range.value.end) {
                    if (seq.seek_if("Upgrade", strutil::ignore_case())) {
                        has_upgrade = true;
                        break;
                    }
                }
            }
            seq.rptr = save;
            return http1::call_and_convert_to_HeaderError(header, seq, range);
        });
        if (err) {
            return err;
        }
        has_h2c_upgrade = has_h2c && has_settings && has_upgrade;
        return error::none;
    }

    template <class Status>
    constexpr error::Error accept_h2c(http1::HTTP1& h1, Status&& status) {
        auto err = h1.write_response(server::StatusCode::http_switching_protocol, [&](auto&& set_header) {
            set_header("Upgrade", "h2c");
            set_header("Connection", "Upgrade");
        });
        if (err) {
            return err;
        }
        return error::none;
    }

    template <class Status>
    constexpr error::Error read_h2c_response(http1::HTTP1& h1, Status&& status, bool& has_h2c_upgrade) {
        bool has_h2c = false, has_upgrade = false;
        auto err = h1.read_response(status, [](auto&& seq, http1::FieldRange range) {
            auto save = seq.rptr;
            seq.rptr = range.key.start;
            if (seq.seek_if("Upgrade") && seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                if (seq.seek_if("h2c")) {
                    has_h2c = true;
                }
            }
            else if (seq.seek_if("Connection") && seq.rptr == range.key.end) {
                seq.rptr = range.value.start;
                while (seq.rptr < range.value.end) {
                    if (seq.seek_if("Upgrade", strutil::ignore_case())) {
                        has_upgrade = true;
                        break;
                    }
                }
            }
        });
        if (err) {
            return err;
        }
        has_h2c_upgrade = has_h2c && has_upgrade;
        return error::none;
    }
}  // namespace futils::fnet::http2
