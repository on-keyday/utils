/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http - http request response generator
#pragma once
// #include "httpstring.h"
#include "util/http/http_headers.h"
#include "util/http/predefined.h"
#include "../view/charvec.h"
#include "../view/sized.h"
#include "storage.h"

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

        using HTTPBodyInfo = http::body::HTTPBodyInfo;

        // HTTP provides HyperText Transfer Protocol version 1 writer and reader implementation
        // consistency check of header and body is not provided
        // most of implementation is rely to util/http
        // remote user data -> input
        // local user data <- output
        struct HTTP {
           private:
            flex_storage input;
            flex_storage output;

           public:
            bool write_request(auto&& method, auto&& path, auto&& header, view::rvec body = {}, bool http_1_0 = false) {
                namespace h = http::header;
                if (!h::render_request(output, method, path, header, h::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1")) {
                    return false;
                }
                strutil::append(output, body);
                return true;
            }

            bool write_response(auto&& status, auto&& header, view::rvec body = {}, const char* reason = nullptr, bool http_1_0 = false) {
                namespace h = http::header;
                if (!reason) {
                    reason = http::value::reason_phrase(std::uint16_t(status), true);
                }
                if (!h::render_response(output, status, reason, header, h::default_validator(), false, http_1_0 ? "HTTP/1.0" : "HTTP/1.1")) {
                    return false;
                }
                strutil::append(output, body);
                return true;
            }

            void write_data(auto&& data, size_t len) {
                strutil::append(output, view::SizedView(data, len));
            }

            void write_chunked_data(auto&& data, size_t len) {
                namespace h = http::body;
                h::render_chunked_data(output, view::SizedView(data, len));
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

            /*
            void borrow_output(const char*& data, size_t& size) {
                data = output.as_char();
                size = output.size();
            }

            void borrow_input(const char*& data, size_t& size) {
                data = input.as_char();
                size = input.size();
            }

            size_t get_output(auto&& buf, size_t limit = ~0, bool peek = false) {
                auto check = make_cpy_seq(view::CharVec(output.begin(), output.size()));
                if (limit > output.size()) {
                    limit = output.size();
                }
                strutil::read_n(buf, check, limit);
                if (!peek) {
                    output.shift_front(limit);
                }
                return limit;
            }

            size_t get_input(auto& buf, size_t limit = ~0, bool peek = false) {
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                if (limit > input.size()) {
                    limit = input.size();
                }
                strutil::read_n(buf, check, limit);
                if (!peek) {
                    input.shift_front(limit);
                }
                return limit;
            }
            */

            void clear_input() {
                input.resize(0);
            }

            void clear_output() {
                output.resize(0);
            }

            void add_input(view::rvec data) {
                strutil::append(input, data);
            }

            // check_response make sure input contains full of response header
            // if input contains full header, this function returns header size
            // otherwise returns 0
            // begin_ok represents header is begin with HTTP/1.1 or HTTP/1.0 or not
            size_t check_response(bool* begin_ok = nullptr) {
                namespace h = http::header;
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
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
                namespace h = http::header;
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                auto begin_check = [&](auto& seq) {
                    if (validate_method) {
                        for (auto meth : http::value::methods) {
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

           private:
           public:
            template <class TmpString = flex_storage, class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
            size_t read_response(auto&& status, auto&& header, HTTPBodyInfo* info, bool peek = false, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                namespace h = http::header;
                HTTPBodyInfo body{};
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                auto body_check = http::body::body_info_preview(body.type, body.expect);
                if (!h::parse_response(check, version, status, reason, h::default_parse_callback<TmpString>(body, header))) {
                    return 0;
                }
                if (info) {
                    *info = body;
                }
                if (!peek) {
                    input.shift_front(check.rptr);
                }
                return check.rptr;
            }

            template <class TmpString = flex_storage, class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&>
            size_t read_request(auto&& method, auto&& path, auto&& header, HTTPBodyInfo* info = nullptr, bool peek = false, Version&& version = helper::nop) {
                namespace h = http::header;
                HTTPBodyInfo body{};
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                auto body_check = http::body::body_info_preview(body.type, body.expect);
                if (!h::parse_request(check, method, path, version, h::default_parse_callback<TmpString>(body, header))) {
                    return 0;
                }
                if (info) {
                    *info = body;
                }
                if (!peek) {
                    input.shift_front(check.rptr);
                }
                return check.rptr;
            }

            template <class Version = decltype(helper::nop)&>
            http::header::HeaderErr peek_request_line(auto&& method, auto&& path, Version&& version = helper::nop) {
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                namespace h = http::header;
                return h::parse_request_line(check, method, path, version);
            }

            template <class Reason = decltype(helper::nop)&, class Version = decltype(helper::nop)&>
            http::header::HeaderErr peek_status_line(auto&& status, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                auto check = make_cpy_seq(view::CharVec(input.begin(), input.size()));
                namespace h = http::header;
                return h::parse_status_line(check, version, status, reason);
            }

            // peek value level
            // peek < 0: not remain data also running
            // peek = 0: not remain data if completed
            // peek > 0: remain data
            // if read_from > 0 peek must be grater than 0
            http::body::BodyReadResult read_body(auto&& buf, HTTPBodyInfo& info, int peek = -1, size_t read_from = 0) {
                if (peek <= 0 && read_from != 0) {
                    return http::body::BodyReadResult::invalid;  // TODO(on-keyday): read only body?
                }
                namespace h = http::body;
                auto check = make_cpy_seq(input);
                check.rptr = read_from;
                auto res = h::read_body(buf, check, info);
                if (res == h::BodyReadResult::invalid) {
                    return res;
                }
                if (res == h::BodyReadResult::incomplete ||
                    res == h::BodyReadResult::chunk_read) {
                    if (peek < 0) {
                        input.shift_front(check.rptr);
                        return res;
                    }
                    return res;
                }
                if (peek <= 0) {
                    input.shift_front(check.rptr);
                }
                return res;  // full or best_effort
            }

            constexpr size_t input_len() const {
                return input.size();
            }

            constexpr size_t output_len() const {
                return output.size();
            }

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
        };
    }  // namespace fnet
}  // namespace futils
