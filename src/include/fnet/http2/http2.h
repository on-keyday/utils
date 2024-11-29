/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "h2handler.h"
#include <view/concat.h>
#include <view/transform.h>

namespace futils::fnet::http2 {
    struct HTTP2 {
       private:
        std::weak_ptr<stream::Stream> handler_;

       public:
        HTTP2(const std::shared_ptr<stream::Stream>& h)
            : handler_(h) {}
        HTTP2(std::shared_ptr<stream::Stream>&& h)
            : handler_(std::move(h)) {}

        std::shared_ptr<stream::Stream> get_stream() const {
            return handler_.lock();
        }

        template <class Method, class Path, class Header>
        Error write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool fin = true, bool clear_text = false) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            auto err = handler->send_headers(
                [&](auto&& set_header) {
                    set_header(+":method", std::forward<Method>(method));
                    set_header(+":path", std::forward<Path>(path));
                    set_header(+":scheme", clear_text ? "http" : "https");
                    http1::apply_call_or_iter(
                        std::forward<Header>(header),
                        [&](auto&& key, auto&& value) {
                            // this checks only the key is valid for HTTP/1.1
                            // so pseudo headers are detected here
                            if (!http1::header::is_valid_key(key)) {
                                return http1::header::HeaderError::invalid_header_key;
                            }
                            if (!http1::header::is_valid_value(value)) {
                                return http1::header::HeaderError::invalid_header_value;
                            }
                            // add :authority header
                            if (strutil::equal(key, "Host", strutil::ignore_case())) {
                                set_header(":authority", std::forward<decltype(value)>(value));
                                return http1::header::HeaderError::none;
                            }
                            auto lower_key = view::make_transform(key, [](auto&& c) { return strutil::to_lower(c); });
                            set_header(lower_key, std::forward<decltype(value)>(value));
                            return http1::header::HeaderError::none;
                        });
                },
                body.size() == 0 && fin);
            if (err) {
                return err;
            }
            if (body.size() > 0) {
                err = handler->send_data(body, fin);
            }
            return err;
        }

        template <class Status, class Header>
        Error write_response(Status&& status, Header&& header, view::rvec body = {}, bool fin = true) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            if (int(status) < 100 || int(status) > 599) {
                return Error{H2Error::internal, false, "invalid status code"};
            }
            auto err = handler->send_headers(
                [&](auto&& set_header) {
                    char buf[4];
                    auto st = int(status);
                    buf[0] = '0' + (st / 100) % 10;
                    buf[1] = '0' + (st / 10) % 10;
                    buf[2] = '0' + st % 10;
                    buf[3] = '\0';
                    set_header(+":status", +buf);
                    http1::apply_call_or_iter(
                        std::forward<decltype(header)>(header),
                        [&](auto&& key, auto&& value) {
                            // this checks only the key is valid for HTTP/1.1
                            // so pseudo headers are detected here
                            if (!http1::header::is_valid_key(key)) {
                                return http1::header::HeaderError::invalid_header_key;
                            }
                            if (!http1::header::is_valid_value(value)) {
                                return http1::header::HeaderError::invalid_header_value;
                            }
                            auto lower_key = view::make_transform(key, [](auto&& c) { return strutil::to_lower(c); });
                            set_header(lower_key, std::forward<decltype(value)>(value));
                            return http1::header::HeaderError::none;
                        });
                },
                body.size() == 0 && fin);
            if (err) {
                return err;
            }
            if (body.size() > 0) {
                err = handler->send_data(body, fin);
            }
            return err;
        }

        template <class Method, class Path, class Header>
        Error read_request(Method&& method, Path&& path, Header&& header) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            auto ok = handler->receive_header([&](auto&& key, auto&& value) {
                if (strutil::equal(key, ":method", strutil::ignore_case())) {
                    auto seq = make_ref_seq(value);
                    http1::range_to_string_or_call(seq, method, {.start = 0, .end = seq.size()});
                }
                else if (strutil::equal(key, ":path", strutil::ignore_case())) {
                    auto seq = make_ref_seq(value);
                    http1::range_to_string_or_call(seq, path, {.start = 0, .end = seq.size()});
                }
                else if (strutil::equal(key, ":authority", strutil::ignore_case())) {
                    flex_storage host("host");
                    auto concat = view::make_concat<flex_storage&, flex_storage&>(host, value);
                    auto seq2 = make_ref_seq(concat);
                    header(seq2,
                           http1::FieldRange{
                               .key = {0, host.size()},
                               .value = {host.size(), host.size() + value.size()},
                           });
                }
                else {
                    auto concat = view::make_concat<flex_storage&, flex_storage&>(key, value);
                    auto seq = make_ref_seq(concat);
                    header(seq,
                           http1::FieldRange{
                               .key = {0, key.size()},
                               .value = {key.size(), key.size() + value.size()},
                           });
                }
            });
            if (!ok) {
                return Error{
                    .code = H2Error::internal,
                    .stream = true,
                    .debug = "failed to read request",
                    .is_resumable = true,
                };
            }
            return no_error;
        }

        template <class Status, class Header>
        Error read_response(Status&& status, Header&& header) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            auto ok = handler->receive_header([&](auto&& key, auto&& value) {
                if (strutil::equal(key, ":status", strutil::ignore_case())) {
                    auto seq = make_ref_seq(value);
                    http1::range_to_string_or_call(seq, status, http1::Range{.start = 0, .end = seq.size()});
                }
                else {
                    auto view = view::make_concat<flex_storage&, flex_storage&>(key, value);
                    auto seq = make_ref_seq(view);
                    header(seq,
                           http1::FieldRange{
                               .key = {0, key.size()},
                               .value = {key.size(), key.size() + value.size()},
                           });
                }
            });
            if (!ok) {
                return Error{
                    .code = H2Error::internal,
                    .stream = true,
                    .debug = "failed to read response",
                    .is_resumable = ok == http1::header::HeaderError::no_data,
                };
            }
            return no_error;
        }

        template <class Body>
        Error write_body(Body&& body, bool fin = true) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            auto ok = handler->send_data(std::forward<decltype(body)>(body), fin);
            if (!ok) {
                return Error{
                    .code = H2Error::internal,
                    .stream = true,
                    .debug = "failed to write data",
                    .is_resumable = ok == http1::body::BodyResult::incomplete,
                };
            }
            return no_error;
        }

        template <class Body>
        Error read_body(Body&& body) {
            auto handler = handler_.lock();
            if (!handler) {
                return Error{H2Error::internal, false, "handler is not set"};
            }
            auto ok = handler->receive_data(std::forward<decltype(body)>(body));
            if (ok != http1::body::BodyResult::full) {
                return Error{
                    .code = H2Error::internal,
                    .stream = true,
                    .debug = "failed to read data",
                    .is_resumable = ok == http1::body::BodyResult::incomplete,
                };
            }
            return no_error;
        }
    };
}  // namespace futils::fnet::http2
