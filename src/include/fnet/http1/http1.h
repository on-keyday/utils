/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http1 - http/1.1 request response generator
#pragma once
#include "header.h"
#include "body.h"
#include "error.h"
#include "predefined.h"
#include <view/charvec.h>
#include <view/sized.h>
#include "../storage.h"

namespace futils {
    namespace fnet {
        namespace server {
            enum class StatusCode {
                http_continue = 100,
                http_switching_protocol = 101,
                http_early_hint = 103,
                http_ok = 200,
                http_created = 201,
                http_accepted = 202,
                http_non_authenticatable_information = 204,
                http_no_content = 205,
                http_partial_content = 206,
                http_multiple_choices = 300,
                http_moved_permanently = 301,
                http_found = 302,
                http_see_other = 303,
                http_not_modified = 304,
                http_temporary_redirect = 307,
                http_permanent_redirect = 308,
                http_bad_request = 400,
                http_unauthorized = 401,
                http_payment_required = 402,
                http_forbidden = 403,
                http_not_found = 404,
                http_method_not_allowed = 405,
                http_not_acceptable = 406,
                http_proxy_authentication_required = 407,
                http_request_timeout = 408,
                http_conflict = 409,
                http_gone = 410,
                http_length_required = 411,
                http_precondition_failed = 412,
                http_payload_too_large = 413,
                http_uri_too_long = 414,
                http_unsupported_media_type = 415,
                http_range_not_satisfiable = 416,
                http_expectation_failed = 417,
                http_Im_a_teapot = 418,
                http_misdirected_request = 421,
                http_too_early = 425,
                http_upgrade_required = 426,
                http_too_many_requests = 428,
                http_request_header_field_too_large = 431,
                http_unavailable_for_legal_reason = 451,
                http_internal_server_error = 500,
                http_not_implemented = 501,
                http_bad_gateway = 502,
                http_service_unavailable = 503,
                http_gateway_timeout = 504,
                http_http_version_not_supported = 505,
                http_variant_also_negotiated = 506,
                http_not_extended = 510,
                http_network_authentication_required = 511,

                http_processing = 102,
                http_multi_status = 207,
                http_already_reported = 208,
                http_IM_used = 226,
                http_unprocessable_entity = 422,
                http_locked = 423,
                http_failed_dependency = 424,
                http_insufficient_storage = 507,
                http_loop_detected = 508,
            };
        }

        namespace http1 {
            // HTTP1 provides Hyper Text Transfer Protocol version 1 writer and reader implementation
            // by default, read consistency is managed by ReadContext
            // and write consistency is not managed
            // remote user data -> input
            // local user data <- output
            struct HTTP1 {
               private:
                flex_storage input;
                flex_storage output;

                HTTPWriteError wrap_write_error(WriteContext& ctx, header::HeaderError herr, body::BodyResult berr) {
                    HTTPWriteError ret;
                    ret.header_error = herr;
                    ret.body_error = berr;
                    return ret;
                }

               public:
                WriteContext write_ctx;

                HTTPWriteError write_request_line(WriteContext& ctx, auto&& method, auto&& path, auto&& version) {
                    if (auto res = header::render_request_line(ctx, output, method, path, version);
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    return HTTPWriteError{};
                }

                HTTPWriteError write_request_line(auto&& method, auto&& path, auto&& version) {
                    return write_request_line(write_ctx, method, path, version);
                }

                HTTPWriteError write_status_line(WriteContext& ctx, auto&& version, auto&& status, auto&& phrase) {
                    if (auto res = header::render_status_line(ctx, output, version, status, phrase);
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    return HTTPWriteError{};
                }

                HTTPWriteError write_status_line(auto&& version, auto&& status, auto&& phrase) {
                    return write_status_line(write_ctx, version, status, phrase);
                }

                HTTPWriteError write_header(WriteContext& ctx, auto&& header) {
                    if (auto res = header::render_header_common(ctx, output, header, header::default_validator());
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    return HTTPWriteError{};
                }

                HTTPWriteError write_header(auto&& header) {
                    return write_header(write_ctx, header);
                }

                HTTPWriteError write_trailer(WriteContext& ctx, auto&& trailer) {
                    if (auto res = header::render_header_common(ctx, output, trailer, header::default_validator());
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    return HTTPWriteError{};
                }

                HTTPWriteError write_trailer(auto&& trailer) {
                    return write_trailer(write_ctx, trailer);
                }

                template <class Method, class Path, class Header>
                HTTPWriteError write_request(WriteContext& ctx, Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool http_1_0 = false) {
                    if (auto res = header::render_request(ctx, output, method, path, header, header::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1");
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    if (body.size() > 0) {
                        return write_body(body);
                    }
                    return HTTPWriteError{};
                }

                template <class Method, class Path, class Header>
                HTTPWriteError write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool http_1_0 = false) {
                    return write_request(write_ctx, method, path, header, body, http_1_0);
                }

                template <class Status, class Header>
                HTTPWriteError write_response(WriteContext& ctx, Status&& status, Header&& header, view::rvec body = {}, const char* reason = nullptr, bool http_1_0 = false) {
                    if (!reason) {
                        reason = reason_phrase(std::uint16_t(status), true);
                    }
                    if (auto res = header::render_response(ctx, output, status, reason, header, header::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1");
                        !res) {
                        return wrap_write_error(ctx, res, body::BodyResult::full);
                    }
                    if (body.size() > 0) {
                        return write_body(body);
                    }
                    return HTTPWriteError{};
                }

                template <class Status, class Header>
                HTTPWriteError write_response(Status&& status, Header&& header, view::rvec body = {}, const char* reason = nullptr, bool http_1_0 = false) {
                    return write_response(write_ctx, status, header, body, reason, http_1_0);
                }

                HTTPWriteError write_body(WriteContext& ctx, auto&& data) {
                    auto res = http1::body::render_body(ctx, output, data);
                    if (res != body::BodyResult::full) {
                        return wrap_write_error(ctx, header::HeaderError::none, res);
                    }
                    return HTTPWriteError{};
                }

                HTTPWriteError write_body(auto&& data) {
                    return write_body(write_ctx, data);
                }

                HTTPWriteError write_end_of_chunk(WriteContext& ctx) {
                    return write_body(ctx, "");
                }

                HTTPWriteError write_end_of_chunk() {
                    return write_end_of_chunk(write_ctx);
                }

                view::rvec get_input() {
                    return input;
                }

                view::rvec get_output() {
                    return output;
                }

                void clear_input() {
                    input.resize(0);
                }

                void clear_output() {
                    output.resize(0);
                }

                void add_input(view::rvec data) {
                    strutil::append(input, data);
                }

                ReadContext read_ctx;

                template <class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
                HTTPReadError read_response(auto&& status, auto&& header, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                    return read_response(read_ctx, status, header, reason, version);
                }

                template <class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
                HTTPReadError read_request(auto&& method, auto&& path, auto&& header, Version&& version = helper::nop) {
                    return read_request(read_ctx, method, path, header, version);
                }

               private:
                HTTPReadError wrap_read_error(ReadContext& ctx, header::HeaderError herr, body::BodyResult berr) {
                    HTTPReadError ret;
                    ret.state = ctx.state();
                    ret.pos = ctx.suspend_pos();
                    ret.header_error = herr;
                    ret.body_error = berr;
                    ret.is_resumable = ctx.is_resumable();
                    return ret;
                }

               public:
                // use explicit context
                template <class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
                HTTPReadError read_response(ReadContext& ctx, auto&& status, auto&& header, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                    auto check = make_cpy_seq(get_input());
                    header::HeaderError err = header::parse_response(ctx, check, version, status, reason, header);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                // use explicit context
                template <class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
                HTTPReadError read_request(ReadContext& ctx, auto&& method, auto&& path, auto&& header, Version&& version = helper::nop) {
                    auto check = make_cpy_seq(get_input());
                    auto err = header::parse_request(ctx, check, method, path, version, header);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                template <class Version = decltype(helper::nop)&>
                HTTPReadError read_request_line(auto&& method, auto&& path, Version&& version = helper::nop) {
                    return read_request_line(read_ctx, method, path, version);
                }

                template <class Version = decltype(helper::nop)&>
                HTTPReadError read_request_line(ReadContext& ctx, auto&& method, auto&& path, Version&& version = helper::nop) {
                    auto check = make_cpy_seq(get_input());
                    auto err = header::parse_request_line(ctx, check, method, path, version);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                template <class Reason = decltype(helper::nop)&, class Version = decltype(helper::nop)&>
                HTTPReadError read_status_line(auto&& status, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                    return read_status_line(read_ctx, status, reason, version);
                }

                template <class Reason = decltype(helper::nop)&, class Version = decltype(helper::nop)&>
                HTTPReadError read_status_line(ReadContext& ctx, auto&& status, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                    auto check = make_cpy_seq(get_input());
                    auto err = header::parse_status_line(ctx, check, version, status, reason);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                HTTPReadError read_body(auto&& buf) {
                    return read_body(read_ctx, buf);
                }

                HTTPReadError read_body(ReadContext& ctx, auto&& buf) {
                    auto check = make_cpy_seq(get_input());
                    auto err = body::read_body(ctx, check, buf);
                    if (err != body::BodyResult::full) {
                        return wrap_read_error(ctx, header::HeaderError::none, err);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                HTTPReadError read_header(auto&& header) {
                    return read_header(read_ctx, header);
                }

                HTTPReadError read_header(ReadContext& ctx, auto&& header) {
                    auto check = make_cpy_seq(get_input());
                    auto err = header::parse_common(ctx, check, header);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                HTTPReadError read_trailer(auto&& header) {
                    return read_header(read_ctx, header);
                }

                HTTPReadError read_trailer(ReadContext& ctx, auto&& header) {
                    return read_header(ctx, header);
                }

                void adjust_input() {
                    auto remove_size = read_ctx.adjust_offset_to_start();
                    input.shift_front(remove_size);
                }
            };

            constexpr auto sizeof_HTTP = sizeof(HTTP1);
        }  // namespace http1

    }  // namespace fnet
}  // namespace futils
