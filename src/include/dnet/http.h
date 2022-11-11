/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http - http request response generator
#pragma once
#include "dll/httpbuf.h"
#include "httpstring.h"
#include "../net/http/http_headers.h"
#include "../net/http/predefined.h"

namespace utils {
    namespace dnet {
        namespace server {
            enum StatusCode {
                http_continue = 100,
                http_switching_protocol = 101,
                http_early_hint = 103,
                http_ok = 200,
                http_created = 201,
                http_accepted = 202,
                http_non_authoritable_information = 204,
                http_no_content = 205,
                http_partical_content = 206,
                http_multiple_choices = 300,
                http_moved_parmently = 301,
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
                http_unavailable_for_leagal_reason = 451,
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

        // HTTPBodyInfo is body information of HTTP
        // this is required for HTTP.body_read
        // and also can get value by HTTP.read_response or HTTP.read_request
        struct HTTPBodyInfo {
            net::h1body::BodyType type = net::h1body::BodyType::no_info;
            size_t expect = 0;
        };

        // HTTP provides HyperText Transfer Protocol version 1 writer and reader implementation
        // consistancy check of header and body is not provided
        // most of implementation is rely to net/http
        // remote user data -> input
        // local user data <- output
        struct HTTP {
           private:
            String input;
            String output;

           public:
            template <class Body = const char*>
            bool write_request(auto&& method, auto&& path, auto&& header, Body&& body = "", size_t blen = 0) {
                namespace h = net::h1header;
                if (!h::render_request(output, method, path, header, h::default_validator())) {
                    return false;
                }
                helper::append(output, helper::SizedView(body, blen));
                return true;
            }

            template <class Body = const char*>
            bool write_response(auto&& status, auto&& header, Body&& body = "", size_t blen = 0, const char* reason = nullptr, bool http_1_0 = false) {
                namespace h = net::h1header;
                if (!reason) {
                    reason = net::h1value::reason_phrase(status, true);
                }
                if (!h::render_response(output, status, reason, header, h::default_validator(), false, helper::no_check(), http_1_0 ? 0 : 1)) {
                    return false;
                }
                helper::append(output, helper::RefSizedView(body, blen));
                return true;
            }

            void write_data(auto&& data, size_t len) {
                helper::append(output, helper::RefSizedView(data, len));
            }

            void write_chunked_data(auto&& data, size_t len) {
                namespace h = net::h1body;
                h::render_chuncked_data(output, helper::RefSizedView(data, len));
            }

            void write_end_of_chunck() {
                write_chunked_data("", 0);
            }

            void borrow_output(const char*& data, size_t& size) {
                data = output.begin();
                size = output.size();
            }

            void borrow_input(const char*& data, size_t& size) {
                data = input.begin();
                size = input.size();
            }

            size_t get_output(auto&& buf, size_t limit = ~0, bool peek = false) {
                auto check = make_cpy_seq(helper::SizedView(output.begin(), output.size()));
                if (limit > output.size()) {
                    limit = output.size();
                }
                helper::read_n(buf, check, limit);
                if (!peek) {
                    output.shift(limit);
                }
                return limit;
            }

            size_t get_input(auto& buf, size_t limit = ~0, bool peek = false) {
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                if (limit > input.size()) {
                    limit = input.size();
                }
                helper::read_n(buf, check, limit);
                if (!peek) {
                    input.shift(limit);
                }
                return limit;
            }

            void clear_input() {
                input.resize(0);
            }

            void clear_output() {
                output.resize(0);
            }

            void add_input(auto&& data, size_t size) {
                helper::append(input, helper::RefSizedView(data, size));
            }

            // check_response make sure input contains full of response header
            // if contains this function returns header size
            // otherwise returns 0
            // begin_ok represents header is begin with HTTP/1.1 or HTTP/1.0 or not
            size_t check_response(bool* begin_ok = nullptr) {
                namespace h = net::h1header;
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
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
                namespace h = net::h1header;
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                auto begcheck = [&](auto& seq) {
                    if (validate_method) {
                        for (auto meth : net::h1value::methods) {
                            auto rptr = seq.rptr;
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
                return h::guess_and_read_raw_header(check, always_false, begcheck);
            }

            template <class TmpString = String, class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&,
                      class Preview = decltype(helper::no_check2()), class Prepare = decltype(helper::no_check2())>
            size_t read_response(auto&& status, auto&& header, HTTPBodyInfo* info, bool peek = false, Reason&& reason = helper::nop, Version&& version = helper::nop,
                                 Preview&& preview = helper::no_check2(), Prepare&& prepare = helper::no_check2()) {
                namespace h = net::h1header;
                HTTPBodyInfo body{};
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                auto body_check = net::h1body::bodyinfo_preview(body.type, body.expect);
                if (!h::parse_response<TmpString>(
                        check, version, status, reason, header, [&](auto&& key, auto&& value) {
                            body_check(key, value);
                            preview(key, value);
                        },
                        prepare)) {
                    return 0;
                }
                if (info) {
                    *info = body;
                }
                if (!peek) {
                    input.shift(check.rptr);
                }
                return check.rptr;
            }

            template <class TmpString = String, class Version = decltype(helper::nop)&, class Reason = decltype(helper::nop)&,
                      class Preview = decltype(helper::no_check2()), class Prepare = decltype(helper::no_check2())>
            size_t read_request(auto&& method, auto&& path, auto&& header, HTTPBodyInfo* info, bool peek = false, Version&& version = helper::nop,
                                Preview&& preview = helper::no_check2(), Prepare&& prepare = helper::no_check2()) {
                namespace h = net::h1header;
                HTTPBodyInfo body{};
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                auto body_check = net::h1body::bodyinfo_preview(body.type, body.expect);
                if (!h::parse_request<TmpString>(
                        check, method, path, version, header, [&](auto&& key, auto&& value) {
                            body_check(key, value);
                            preview(key, value);
                        },
                        prepare)) {
                    return 0;
                }
                if (info) {
                    *info = body;
                }
                if (!peek) {
                    input.shift(check.rptr);
                }
                return check.rptr;
            }

            template <class Version = decltype(helper::nop)&>
            bool peek_request_line(auto&& method, auto&& path, Version&& version = helper::nop) {
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                namespace h = net::h1header;
                return h::parse_request_line(check, method, path, version);
            }

            template <class Reason = decltype(helper::nop)&, class Version = decltype(helper::nop)&>
            bool peek_status_line(auto&& status, Reason&& reason = helper::nop, Version&& version = helper::nop) {
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                namespace h = net::h1header;
                return h::parse_status_line(check, version, status, reason);
            }

            // peek value level
            // peek < 0: not remain data also running
            // peek = 0: not remain data if completed
            // peek > 0: remain data
            // if read_from > 0 peek must be grater than 0
            bool read_body(auto&& buf, HTTPBodyInfo info, int peek = -1, size_t read_from = 0, bool* invalid = nullptr) {
                if (peek <= 0 && read_from != 0) {
                    return false;  // TODO(on-keyday): read only body?
                }
                namespace h = net::h1body;
                auto check = make_cpy_seq(helper::SizedView(input.begin(), input.size()));
                check.rptr = read_from;
                auto res = h::read_body(buf, check, info.expect, info.type);
                if (res == -1) {
                    if (invalid) {
                        *invalid = true;
                    }
                    return false;
                }
                if (invalid) {
                    *invalid = false;
                }
                if (res == 0) {
                    if (peek < 0) {
                        input.shift(check.rptr);
                        return check.rptr != 0;
                    }
                    return false;
                }
                if (peek <= 0) {
                    input.shift(check.rptr);
                }
                return true;
            }

            constexpr size_t input_len() const {
                return input.size();
            }

            constexpr size_t output_len() const {
                return output.size();
            }

            // strict_check_header is strict header checker
            // this function uses check_request and check_response
            // and check header byte is vaild
            // if not valid header begining or not vaild byte appears
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
                String in;
                if (len == 0) {
                    len = input.size();
                }
                else {
                    if (complete_header) {
                        *complete_header = true;
                    }
                }
                for (size_t i = 0; i < len; i++) {
                    if (in[i] == '\r' || in[i] == '\n' || in[i] == '\t') {
                        continue;
                    }
                    if (in[i] < 0x20 || in[i] > 0x7f) {
                        return 0;
                    }
                }
                return len;
            }
        };
    }  // namespace dnet
}  // namespace utils
