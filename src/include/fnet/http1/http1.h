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

                HTTPWriteError wrap_write_error(header::HeaderError herr, body::BodyReadResult berr) {
                    HTTPWriteError ret;
                    ret.header_error = herr;
                    ret.body_error = berr;
                    return ret;
                }

               public:
                template <class Method, class Path, class Header>
                HTTPWriteError write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool http_1_0 = false) {
                    if (auto res = header::render_request(output, method, path, header, header::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1");
                        !res) {
                        return wrap_write_error(res, body::BodyReadResult::full);
                    }
                    strutil::append(output, body);
                    return HTTPWriteError{};
                }

                template <class Status, class Header>
                HTTPWriteError write_response(Status&& status, Header&& header, view::rvec body = {}, const char* reason = nullptr, bool http_1_0 = false) {
                    if (!reason) {
                        reason = reason_phrase(std::uint16_t(status), true);
                    }
                    if (auto res = header::render_response(output, status, reason, header, header::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1");
                        !res) {
                        return wrap_write_error(res, body::BodyReadResult::full);
                    }
                    strutil::append(output, body);
                    return HTTPWriteError{};
                }

                void write_data(auto&& data, size_t len) {
                    strutil::append(output, view::SizedView(data, len));
                }

                void write_chunked_data(auto&& data, size_t len) {
                    body::render_chunked_data(output, view::SizedView(data, len));
                }

                void write_end_of_chunk() {
                    write_chunked_data("", 0);
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
                HTTPReadError wrap_read_error(ReadContext& ctx, header::HeaderError herr, body::BodyReadResult berr) {
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
                        return wrap_read_error(ctx, err, body::BodyReadResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                // use explicit context
                template <class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
                HTTPReadError read_request(ReadContext& ctx, auto&& method, auto&& path, auto&& header, Version&& version = helper::nop) {
                    auto check = make_cpy_seq(get_input());
                    auto err = header::parse_request(ctx, check, method, path, version, header);
                    if (err != header::HeaderError::none) {
                        return wrap_read_error(ctx, err, body::BodyReadResult::full);
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
                        return wrap_read_error(ctx, err, body::BodyReadResult::full);
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
                        return wrap_read_error(ctx, err, body::BodyReadResult::full);
                    }
                    return {ctx.state(), ctx.suspend_pos()};
                }

                HTTPReadError read_body(auto&& buf) {
                    return read_body(read_ctx, buf);
                }

                HTTPReadError read_body(ReadContext& ctx, auto&& buf) {
                    auto check = make_cpy_seq(get_input());
                    auto err = body::read_body(ctx, check, buf);
                    if (err != body::BodyReadResult::full) {
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
                        return wrap_read_error(ctx, err, body::BodyReadResult::full);
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
        /*
       // check_response make sure input contains full of response header
       // if input contains full header, this function returns header size
       // otherwise returns 0
       // begin_ok represents header is begin with HTTP/1.1 or HTTP/1.0 or not
       size_t check_response(bool* begin_ok = nullptr) {
           namespace h = header;
           auto check = make_cpy_seq(get_input());
           auto begcheck = [&](auto& seq) {
               auto begin_check = seq.seek_if("HTTP/1.1") || seq.seek_if("HTTP/1.0");
               if (begin_ok) {
                   *begin_ok = begin_check;
               }
               return begin_check;
           };
           auto always_false = [](auto&&...) { return false; };
           return h::guess_and_read_raw_header(check, always_false, begcheck);
       }

       // check_request make sure input contains full of request header
       // if contains this function returns header size
       // otherwise returns 0
       // if validate_method is true this function checks header begin with known http method
       size_t check_request(bool* begin_ok = nullptr, bool validate_method = true) {
           namespace h = header;
           auto check = make_cpy_seq(get_input());
           auto begin_check = [&](auto& seq) {
               if (validate_method) {
                   for (auto meth : value::methods) {
                       // auto rptr = seq.rptr;
                       if (seq.seek_if(meth) && seq.consume_if(' ')) {
                           goto OK;
                       }
                   }
                   if (begin_ok) {
                       *begin_ok = false;
                   }
                   return false;
               }
           OK:
               if (begin_ok) {
                   *begin_ok = true;
               }
               return true;
           };
           auto always_false = [](auto&&...) { return false; };
           return h::guess_and_read_raw_header(check, always_false, begin_check);
       }
       */

        /*
       // strict_check_header is strict header checker
       // this function uses check_request and check_response
       // and check header byte is valid
       // if not valid header beginning or not valid byte appears
       // this function returns 0
       // otherwise this function returns header length current available
       size_t strict_check_header(bool request_header = true, bool* complete_header = nullptr) {
           bool beg = false;
           size_t len = 0;
           if (complete_header) {
               *complete_header = false;
           }
           if (request_header) {
               len = check_request(&beg);
           }
           else {
               len = check_response(&beg);
           }
           if (!beg) {
               return 0;
           }
           if (len == 0) {
               len = input.size();
           }
           else {
               if (complete_header) {
                   *complete_header = true;
               }
           }
           for (size_t i = 0; i < len; i++) {
               if (input[i] == '\r' || input[i] == '\n' || input[i] == '\t') {
                   continue;
               }
               if (input[i] < 0x20 || input[i] > 0x7f) {
                   return 0;
               }
           }
           return len;
       }
       */

    }  // namespace fnet
}  // namespace futils
