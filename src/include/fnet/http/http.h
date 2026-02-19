/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// http - http abstraction
#pragma once
#include "../http1/http1.h"
#include "../http2/http2.h"
#include "../http3/http3.h"
#include <variant>

namespace futils::fnet::http {

    enum class HTTPVersion {
        unknown,
        http1,
        http2,
        http3,
    };

    constexpr http1::HTTPState h3_state_machine_to_http_state(http3::stream::StateMachine s) {
        switch (s) {
            case http3::stream::StateMachine::client_header_recv:
            case http3::stream::StateMachine::server_header_recv:
                return http1::HTTPState::header;
            case http3::stream::StateMachine::client_data_recv:
            case http3::stream::StateMachine::server_data_recv:
                return http1::HTTPState::body;
            case http3::stream::StateMachine::client_end:
            case http3::stream::StateMachine::server_end:
            case http3::stream::StateMachine::server_header_send:
                return http1::HTTPState::end;
            default:
                return http1::HTTPState::init;
        }
    }

    constexpr auto unknown_http_version = error::Error("unknown http version", futils::error::Category::app);

    inline bool is_resumable(const error::Error& err) {
        if (auto r = err.as<http1::HTTPReadError>()) {
            return r->is_resumable;
        }
        if (auto r = err.as<http2::Error>()) {
            return r->is_resumable;
        }
        if (auto r = err.as<http3::Error>()) {
            return r->is_resumable;
        }
        return false;
    }

    struct HTTP {
       private:
        std::variant<std::monostate, http1::HTTP1, http2::HTTP2, http3::HTTP3> internal_;

       public:
        constexpr HTTP() = default;

        HTTP(http1::HTTP1 h1)
            : internal_(std::move(h1)) {}

        HTTP(http2::HTTP2 h2)
            : internal_(std::move(h2)) {}

        HTTP(http3::HTTP3 h3)
            : internal_(std::move(h3)) {}

        HTTPVersion version() const {
            if (std::holds_alternative<http1::HTTP1>(internal_)) {
                return HTTPVersion::http1;
            }
            if (std::holds_alternative<http2::HTTP2>(internal_)) {
                return HTTPVersion::http2;
            }
            if (std::holds_alternative<http3::HTTP3>(internal_)) {
                return HTTPVersion::http3;
            }
            return HTTPVersion::unknown;
        }

        http1::HTTP1* http1() {
            return std::get_if<http1::HTTP1>(&internal_);
        }

        const http1::HTTP1* http1() const {
            return std::get_if<http1::HTTP1>(&internal_);
        }

        http2::HTTP2* http2() {
            return std::get_if<http2::HTTP2>(&internal_);
        }

        const http2::HTTP2* http2() const {
            return std::get_if<http2::HTTP2>(&internal_);
        }

        http3::HTTP3* http3() {
            return std::get_if<http3::HTTP3>(&internal_);
        }

        const http3::HTTP3* http3() const {
            return std::get_if<http3::HTTP3>(&internal_);
        }

        template <class Method, class Path, class Header>
        error::Error read_request(Method&& method, Path&& path, Header&& header) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->read_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->read_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->read_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

        template <class Status, class Header>
        error::Error read_response(Status&& status, Header&& header) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->read_response(std::forward<Status>(status), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->read_response(std::forward<Status>(status), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->read_response(std::forward<Status>(status), std::forward<Header>(header));
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

        error::Error read_body(auto&& callback) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->read_body(callback);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->read_body(callback);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->read_body(callback);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

       private:
        error::Error check_write_http1_body_end() {
            if (http1()->write_ctx.state() == http1::WriteState::chunked_body) {
                if (auto err = http1()->write_end_of_chunk()) {
                    return err;
                }
            }
            if (http1()->write_ctx.state() != http1::WriteState::end) {
                return error::Error("http1 write state is not end", futils::error::Category::app);
            }
            return error::none;
        }

       public:
        template <class Method, class Path, class Header>
        error::Error write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool fin = true) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->write_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header), body);
                    if (err) {
                        return err;
                    }
                    if (fin) {
                        return check_write_http1_body_end();
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->write_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header), body, fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->write_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header), body, fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

        template <class Status, class Header>
        error::Error write_response(Status&& status, Header&& header, view::rvec body = {}, bool fin = true) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->write_response(std::forward<Status>(status), std::forward<Header>(header), body);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->write_response(std::forward<Status>(status), std::forward<Header>(header), body, fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->write_response(std::forward<Status>(status), std::forward<Header>(header), body, fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

        template <class Body>
        error::Error write_body(Body&& body, bool fin = true) {
            switch (version()) {
                case HTTPVersion::http1: {
                    auto err = http1()->write_body(std::forward<Body>(body), fin);
                    if (err) {
                        return err;
                    }
                    if (fin) {
                        return check_write_http1_body_end();
                    }
                    break;
                }
                case HTTPVersion::http2: {
                    auto err = http2()->write_body(std::forward<Body>(body), fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case HTTPVersion::http3: {
                    auto err = http3()->write_body(std::forward<Body>(body), fin);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    return unknown_http_version;
            }
            return error::none;
        }

        http1::HTTPState state() const {
            switch (version()) {
                case HTTPVersion::http1:
                    return http1()->read_ctx.http_state();
                case HTTPVersion::http2:
                    return http2()->get_stream()->recv_state();
                case HTTPVersion::http3:
                    return h3_state_machine_to_http_state(http3()->get_stream()->state);
                default:
                    return http1::HTTPState::end;
            }
        }
    };

    struct RequestWriter {
       private:
        HTTP* http;
        bool fin = false;
        error::Error capture;

       public:
        RequestWriter() = default;
        RequestWriter(HTTP& h)
            : http(&h) {}

        void reset(HTTP& h) {
            http = &h;
            fin = false;
            capture = error::none;
        }

        error::Error get_error() const {
            return capture;
        }

        bool is_fin() const {
            return fin;
        }

        template <class Method, class Path, class Header>
        error::Error write_request(Method&& method, Path&& path, Header&& header, view::rvec body = {}, bool fin = true) {
            if (!http) {
                return error::Error("http is not set", futils::error::Category::lib);
            }
            if (this->fin) {
                return error::Error("write_request after fin", futils::error::Category::lib);
            }
            if (capture) {
                return capture;
            }
            auto err = http->write_request(std::forward<Method>(method), std::forward<Path>(path), std::forward<Header>(header), body, fin);
            if (err) {
                capture = err;
                return err;
            }
            this->fin = fin;
            return error::none;
        }

        template <class Body>
        error::Error write_body(Body&& body, bool fin = true) {
            if (!http) {
                return error::Error("http is not set", futils::error::Category::lib);
            }
            if (this->fin) {
                return error::Error("write_body after fin", futils::error::Category::lib);
            }
            if (capture) {
                return capture;
            }
            auto err = http->write_body(std::forward<Body>(body), fin);
            if (err) {
                capture = err;
                return err;
            }
            this->fin = fin;
            return error::none;
        }
    };

    struct ResponseReader {
       private:
        HTTP* http;
        error::Error capture;
        bool resumable = false;
        bool called = false;

       public:
        ResponseReader() = default;
        ResponseReader(HTTP& h)
            : http(&h) {}

        void reset(HTTP& h) {
            http = &h;
            capture = error::none;
            resumable = false;
            called = false;
        }

        http1::HTTPState state() const {
            if (!http) {
                return http1::HTTPState::end;
            }
            return http->state();
        }

        error::Error get_error() const {
            return capture;
        }

        bool is_resumable() const {
            return resumable;
        }

        bool is_called() const {
            return called;
        }

        template <class Status, class Header>
        error::Error read_response(Status&& status, Header&& header) {
            if (!http) {
                return error::Error("http is not set", futils::error::Category::lib);
            }
            if (capture) {
                return capture;
            }
            auto err = http->read_response(std::forward<Status>(status), std::forward<Header>(header));
            called = true;
            if (err) {
                capture = err;
                resumable = http::is_resumable(err);
                return err;
            }
            return error::none;
        }

        template <class Body>
        error::Error read_body(Body&& body) {
            if (!http) {
                return error::Error("http is not set", futils::error::Category::lib);
            }
            if (capture) {
                return capture;
            }
            auto err = http->read_body(std::forward<Body>(body));
            called = true;
            if (err) {
                capture = err;
                resumable = http::is_resumable(err);
                return err;
            }
            return error::none;
        }
    };

}  // namespace futils::fnet::http
