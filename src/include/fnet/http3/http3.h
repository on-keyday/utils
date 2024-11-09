/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "stream.h"
#include <fnet/quic/quic.h>
#include "typeconfig.h"
#include <fnet/util/qpack/typeconfig.h>
#include <view/concat.h>
#include "error.h"
#include <view/transform.h>

namespace futils::fnet::http3 {
    struct HTTP3 {
        using Stream = stream::RequestStream<TypeConfig<qpack::TypeConfig<flex_storage>, quic::use::smartptr::DefaultStreamTypeConfig>>;

       private:
        std::weak_ptr<Stream> handler_;

       public:
        HTTP3(std::shared_ptr<Stream> s)
            : handler_(s) {}

        std::shared_ptr<Stream> get_stream() const {
            return handler_.lock();
        }

        template <class Method, class Path, class Header>
        Error write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool fin = true) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            H3Error err = handler->write_header(
                body.size() == 0 && fin,
                [&](auto&& set_entry, auto&& set_header) {
                    set_header(":method", std::forward<Method>(method));
                    set_header(":path", std::forward<Path>(path));
                    set_header(":scheme", "https");
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
                            // key must be lower case
                            auto lower_key = view::make_transform(key, [](auto&& c) { return strutil::to_lower(c); });
                            set_header(lower_key, std::forward<decltype(value)>(value));
                        });
                });
            if (err != H3Error::H3_NO_ERROR) {
                return make_error(err);
            }
            if (body.size() > 0) {
                err = handler->write(body, fin);
            }
            return make_error(err);
        }

        template <class Status, class Header>
        Error write_response(Status&& status, Header&& header, view::rvec body = {}, bool fin = true) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            if (int(status) < 100 || int(status) > 599) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "invalid status code");
            }
            H3Error err = handler->write_header(
                body.size() == 0 && fin,
                [&](auto&& add_entry, auto&& set_header) {
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
                            // key must be lower case
                            auto lower_key = view::make_transform(key, [](auto&& c) { return strutil::to_lower(c); });
                            set_header(lower_key, std::forward<decltype(value)>(value));
                            return http1::header::HeaderError::none;
                        });
                });
            if (err != H3Error::H3_NO_ERROR) {
                return make_error(err);
            }
            if (body.size() > 0) {
                err = handler->write(body, fin);
            }
            return make_error(err);
        }

        template <class Body>
        Error write_body(Body&& body, bool fin = true) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            auto err = handler->write(body, fin);
            return make_error(err);
        }

        template <class Method, class Path, class Header>
        Error read_request(Method&& method, Path&& path, Header&& header) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            handler->read_header([&](qpack::DecodeField<flex_storage>& field) {
                if (strutil::equal(field.key, ":method", strutil::ignore_case())) {
                    auto seq = make_ref_seq(field.value);
                    http1::range_to_string_or_call(seq, method, {.start = 0, .end = seq.size()});
                }
                else if (strutil::equal(field.key, ":path", strutil::ignore_case())) {
                    auto seq = make_ref_seq(field.value);
                    http1::range_to_string_or_call(seq, path, {.start = 0, .end = seq.size()});
                }
                else if (strutil::equal(field.key, ":authority", strutil::ignore_case())) {
                    flex_storage host("host");
                    auto concat = view::make_concat<flex_storage&, flex_storage&>(host, field.value);
                    auto seq2 = make_ref_seq(concat);
                    header(seq2,
                           http1::FieldRange{
                               .key = {0, host.size()},
                               .value = {host.size(), host.size() + field.value.size()},
                           });
                }
                else {
                    auto concat = view::make_concat<flex_storage&, flex_storage&>(field.key, field.value);
                    auto seq = make_ref_seq(concat);
                    header(seq, http1::FieldRange{
                                    .key = {.start = 0, .end = field.key.size()},
                                    .value = {.start = field.key.size(), .end = seq.size()},
                                });
                }
            });
            return make_error(H3Error::H3_NO_ERROR);
        }

        template <class Status, class Header>
        Error read_response(Status&& status, Header&& header) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            handler->read_header([&](qpack::DecodeField<flex_storage>& field) {
                if (strutil::equal(field.key, ":status", strutil::ignore_case())) {
                    auto seq = make_ref_seq(field.value);
                    http1::range_to_string_or_call(seq, status, {.begin = 0, .end = seq.size()});
                }
                else {
                    http1::apply_call_or_emplace(header, std::move(field.key), std::move(field.value));
                }
            });
            return make_error(H3Error::H3_NO_ERROR);
        }

        template <class Body>
        Error read_body(Body&& body) {
            auto handler = get_stream();
            if (!handler) {
                return make_error(H3Error::H3_INTERNAL_ERROR, "handler is not set");
            }
            handler->read([&](auto&& data) {
                body(data);
            });
            return make_error(H3Error::H3_NO_ERROR);
        }
    };
}  // namespace futils::fnet::http3
