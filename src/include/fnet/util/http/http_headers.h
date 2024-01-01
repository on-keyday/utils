/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http_headers - header and body
#pragma once
#include "header.h"
#include "body.h"

namespace futils::http::header {

    template <class String>
    auto default_parse_callback(body::HTTPBodyInfo& info, auto&& header) {
        auto body_check = body::body_info_preview(info.type, info.expect);
        return header::range_to_string<String>([&, body_check](auto&& key, auto&& value) {
            body_check(key, value);
            return header::apply_call_or_emplace(header, std::move(key), std::move(value));
        });
    }

    template <class T, class Callback, class BeginCheck>
    size_t guess_and_read_raw_header(Sequencer<T>& seq, Callback&& callback, BeginCheck&& check) {
        while (seq.size() == 0) {
            if (!callback(seq, 0, false)) {
                return 0;
            }
        }
        if (!check(seq)) {
            return 0;
        }
        while (true) {
            if (seq.seek_if("\r\n\r\n") ||
                seq.seek_if("\r\r") || seq.seek_if("\n\n")) {
                break;
            }
            seq.consume();
            while (seq.eos()) {
                if (!callback(seq, 0, false)) {
                    return 0;
                }
            }
        }
        auto res = seq.rptr;
        seq.seek(0);
        return res;
    }

    template <class T, class Body, class Callback>
    bool read_body_with_info(Sequencer<T>& seq, Body& body, body::BodyType btype, size_t expect, Callback&& callback) {
        if (btype == body::BodyType::no_info) {
            if (!callback(seq, 0, true)) {
                return false;
            }
        }
        while (true) {
            auto s = body::read_body(body, seq, expect, btype);
            if (s == -1) {
                return false;
            }
            else if (s == 1) {
                break;
            }
            if (!callback(seq, expect, false)) {
                return false;
            }
        }
        return true;
    }

    template <class TmpBuffer, class Buffer, class Method, class Path, class Header, class Body, class Callback>
    bool read_request(Buffer& buf, Method& method, Path& path, Header& h, Body& body, Callback&& callback) {
        auto seq = make_ref_seq(buf);
        if (!guess_and_read_raw_header(seq, callback, [](auto& seq) { return number::is_alnum(seq.current()); })) {
            return false;
        }
        size_t expect = 0;
        body::BodyType btype = body::BodyType::no_info;
        if (!header::parse_request(seq, method, path, helper::nop, h, body::body_info_preview(btype, expect))) {
            return false;
        }
        if (!read_body_with_info(seq, body, btype, expect, callback)) {
            return false;
        }
        return true;
    }

    template <class TmpBuffer, class Buffer, class Status, class Header, class Body, class Callback>
    bool read_response(Buffer& buf, Status& status, Header& h, Body& body, Callback&& callback) {
        auto seq = make_ref_seq(buf);
        if (!guess_and_read_raw_header(seq, callback, [](auto& seq) { return seq.seek_if("HTTP/1.1") ||
                                                                             seq.seek_if("HTTP/1.0"); })) {
            return false;
        }
        size_t expect = 0;
        body::BodyType btype = body::BodyType::no_info;
        auto body_info_preview = body::body_info_preview(btype, expect);
        if (!header::parse_response(seq, helper::nop, status, helper::nop,
                                    header::range_to_string<TmpBuffer>([&](auto&& key, auto&& value) {
                                        body_info_preview(key, value);
                                        h.emplace(std::move(key), std::move(value));
                                    }))) {
            return false;
        }
        if (!read_body_with_info(seq, body, btype, expect, callback)) {
            return false;
        }
        return true;
    }

}  // namespace futils::http::header
