/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// http_headers - header and body
#pragma once
#include "header.h"
#include "body.h"

namespace utils {
    namespace net {
        namespace h1header {
            template <class TmpBuffer, class Buffer, class Status, class Header, class Body, class Callback>
            bool read_response(Buffer& buf, Status& status, Header& h, Body& body, Callback&& callback) {
                auto seq = make_ref_seq(buf);
                while (seq.size() == 0) {
                    if (!callback(seq, 0, false)) {
                        return false;
                    }
                }
                if (!helper::starts_with(buf, "HTTP/1.1")) {
                    return false;
                }
                seq.seek(9);
                while (true) {
                    if (seq.seek_if("\r\n\r\n") ||
                        seq.seek_if("\r\r") || seq.seek_if("\n\n")) {
                        break;
                    }
                    seq.consume();
                    while (seq.eos()) {
                        if (!callback(seq, 0, false)) {
                            return false;
                        }
                    }
                }
                seq.seek(0);
                size_t expect = 0;
                h1body::BodyType btype = h1body::BodyType::no_info;
                if (!h1header::parse_response<TmpBuffer>(seq, helper::nop, status, helper::nop, h, h1body::bodyinfo_preview(btype, expect))) {
                    return false;
                }
                if (btype == h1body::BodyType::no_info) {
                    if (!callback(seq, 0, true)) {
                        return false;
                    }
                }
                while (true) {
                    auto s = h1body::read_body(body, seq, expect, btype);
                    if (s == State::failed) {
                        return false;
                    }
                    else if (s == State::complete) {
                        break;
                    }
                    if (!callback(seq, expect, false)) {
                        return false;
                    }
                }
                return true;
            }
        }  // namespace h1header
    }      // namespace net
}  // namespace utils
