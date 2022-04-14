/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/platform/windows/dllexport_source.h"
#include "../../../include/net/http/http1.h"
#include "../../../include/net/http/header.h"
#include "../../../include/wrap/light/string.h"
#include "../../../include/wrap/light/map.h"
#include "../../../include/wrap/light/vector.h"

#include "../../../include/helper/equal.h"
#include "../../../include/number/to_string.h"
#include "../../../include/net/http/body.h"

#include "../../../include/net/async/pool.h"
#include "../../../include/net/http/value.h"
#include "header_impl.h"
#include "async_resp_impl.h"

namespace utils {
    namespace net {
        namespace http {
            enum class HttpState {
                requesting,
                body_sending,
                responding,
                body_recving,
                failed,
                end_process,
            };

            namespace internal {

                struct HttpResponseImpl {
                    wrap::shared_ptr<HeaderImpl> header;
                    HttpState state;
                    IOClose io;
                    wrap::string buf;
                    size_t redpos = 0;
                    Header response;
                    h1body::BodyType bodytype = h1body::BodyType::no_info;
                    size_t expect = 0;
                    wrap::string hostname;
                };

            }  // namespace internal

            HttpResponse::HttpResponse(HttpResponse&& in) {
                impl = in.impl;
                in.impl = nullptr;
            }

            HttpResponse& HttpResponse::operator=(HttpResponse&& in) {
                delete impl;
                impl = in.impl;
                in.impl = nullptr;
                return *this;
            }

            Header::Header() {
                impl = wrap::make_shared<internal::HeaderImpl>();
            }

            Header& Header::set(const char* key, const char* value) {
                if (!impl || !key || !value) {
                    return *this;
                }
                impl->emplace(key, value);
                return *this;
            }

            std::uint16_t Header::status() const {
                if (!impl) {
                    return 0;
                }
                return impl->code;
            }

            const char* Header::response(size_t* p) {
                if (!impl) {
                    return "";
                }
                if (!impl->raw_header.size() || impl->changed) {
                    h1header::render_response(
                        impl->raw_header, impl->code, h1value::reason_phrase(impl->code, true), *impl,
                        helper::no_check(), true, helper::no_check(), impl->version);
                }
                if (p) {
                    *p = impl->raw_header.size();
                }
                return impl->raw_header.c_str();
            }

            const char* Header::value(const char* key, size_t index) {
                if (!key) {
                    return nullptr;
                }
                if (auto ptr = impl->find(key, index); ptr) {
                    return ptr->c_str();
                }
                return nullptr;
            }

            const char* Header::body(size_t* size) const {
                if (!impl) {
                    if (size) {
                        *size = 0;
                    }
                    return "";
                }
                if (size) {
                    *size = impl->body.size();
                }
                return impl->body.c_str();
            }

            bool HttpResponse::failed() const {
                return !impl || impl->state == HttpState::failed;
            }

            Header HttpResponse::get_response() {
                if (!impl || impl->state == HttpState::failed || impl->state == HttpState::end_process) {
                    return nullptr;
                }
                auto failed_clean = [&] {
                    impl->state = HttpState::failed;
                    impl->io.close(true);
                };
                auto is_failed = [](auto res) {
                    return res != State::complete && res != State::running;
                };
                if (impl->state == HttpState::requesting) {
                    auto res = impl->io.write(impl->buf.c_str(), impl->buf.size(), nullptr);
                    if (is_failed(res)) {
                        failed_clean();
                        return nullptr;
                    }
                    if (res == State::running) {
                        return nullptr;
                    }
                    impl->state = HttpState::body_sending;
                }
                if (impl->state == HttpState::body_sending) {
                    if (impl->header->body.size()) {
                        auto res = impl->io.write(impl->header->body.c_str(), impl->header->body.size(), nullptr);
                        if (is_failed(res)) {
                            failed_clean();
                            return nullptr;
                        }
                        if (res == State::running) {
                            return nullptr;
                        }
                    }
                    impl->state = HttpState::responding;
                    impl->buf.clear();
                }
                if (impl->state == HttpState::responding) {
                    auto res = read(impl->buf, impl->io);
                    if (is_failed(res)) {
                        failed_clean();
                        return nullptr;
                    }
                    auto seq = make_ref_seq(impl->buf);
                    seq.rptr = impl->redpos;
                    while (!seq.eos()) {
                        if (seq.seek_if("\r\n\r\n") || seq.seek_if("\n\n") || seq.seek_if("\r\r")) {
                            seq.rptr = 0;
                            if (!h1header::parse_response<wrap::string>(
                                    seq, helper::nop, impl->response.impl->code, helper::nop, *impl->response.impl,
                                    h1body::bodyinfo_preview(impl->bodytype, impl->expect))) {
                                failed_clean();
                                return nullptr;
                            }
                            impl->state = HttpState::body_recving;
                            break;
                        }
                        seq.consume();
                    }
                    impl->redpos = seq.rptr;
                }
                if (impl->state == HttpState::body_recving) {
                    auto seq = make_ref_seq(impl->buf);
                    seq.rptr = impl->redpos;
                    State e = State::complete;
                    if (impl->bodytype == h1body::BodyType::no_info && !seq.eos()) {
                        while (read(impl->buf, impl->io) == State::complete) {
                            // busy loop
                        }
                    }
                BEGIN:
                    e = h1body::read_body(impl->response.impl->body, seq, impl->expect, impl->bodytype);
                    if (is_failed(e)) {
                        failed_clean();
                        return nullptr;
                    }
                    if (e == State::running) {
                        e = read(impl->buf, impl->io);
                        if (is_failed(e)) {
                            failed_clean();
                            return nullptr;
                        }
                        if (e == State::running) {
                            impl->redpos = seq.rptr;
                            return nullptr;
                        }
                        goto BEGIN;
                    }
                    impl->state = HttpState::end_process;
                    return std::move(impl->response);
                }
                return nullptr;
            }

            bool render_request(wrap::string& buf, const char* host, const char* method, const char* path, internal::HeaderImpl& header) {
                constexpr auto validator = h1header::default_validator();
                if (!validator(std::pair{"Host", host})) {
                    return false;
                }
                auto res = h1header::render_request(
                    buf, method, path, header,
                    [&](auto&& keyval) {
                        if (helper::equal(std::get<0>(keyval), "Host", helper::ignore_case())) {
                            return false;
                        }
                        if (helper::equal(std::get<0>(keyval), "Content-Length", helper::ignore_case())) {
                            return false;
                        }
                        return validator(keyval);
                    },
                    false,
                    [&](auto& str) {
                        helper::append(str, "Host: ");
                        helper::append(str, host);
                        helper::append(str, "\r\n");
                        helper::append(str, "Content-Length:");
                        helper::FixedPushBacker<char[64], 63> pb;
                        number::to_string(pb, header.body.size());
                        helper::append(str, pb.buf);
                        helper::append(str, "\r\n");
                    });
                if (!res) {
                    return false;
                }
                return true;
            }

            HttpResponse request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header) {
                if (!io || !host || !method || !path || !header) {
                    return HttpResponse{};
                }
                wrap::string buf;
                if (!render_request(buf, host, method, path, *header.impl)) {
                    return HttpResponse{};
                }
                HttpResponse response;
                response.impl = new internal::HttpResponseImpl{};
                auto done = io.write(buf.c_str(), buf.size(), nullptr);
                if (done != State::complete && done != State::running) {
                    return HttpResponse{};
                }
                response.impl->header = header.impl;
                header.impl = nullptr;
                response.impl->state = done != State::complete ? HttpState::requesting : HttpState::body_sending;
                response.impl->buf = std::move(buf);
                response.impl->io = std::move(io);
                response.impl->hostname = host;
                return response;
            }

            HttpResponse request(HttpResponse&& io, const char* method, const char* path, Header&& header) {
                if (io.failed()) {
                    return HttpResponse{};
                }
                if (io.impl->state != HttpState::end_process) {
                    return HttpResponse{};
                }
                wrap::string buf;
                if (!render_request(buf, io.impl->hostname.c_str(), method, path, *header.impl)) {
                    return HttpResponse{};
                }
                auto done = io.impl->io.write(buf.c_str(), buf.size(), nullptr);
                if (done != State::complete && done != State::running) {
                    return HttpResponse{};
                }
                io.impl->state = done != State::complete ? HttpState::requesting : HttpState::body_sending;
                io.impl->bodytype = h1body::BodyType::no_info;
                io.impl->buf = std::move(buf);
                io.impl->redpos = 0;
                io.impl->expect = 0;
                io.impl->header = header.impl;
                header.impl = nullptr;
                io.impl->response = Header{};
                return std::move(io);
            }

            HttpAsyncResult STDCALL request_async(async::Context& ctx, AsyncIOClose&& io, const char* host, const char* method, const char* path, Header&& header) {
                if (!io || !host || !method || !path || !header) {
                    return HttpAsyncResult{
                        .err = HttpError::invalid_arg};
                }
                wrap::string buf;
                if (!render_request(buf, host, method, path, *header.impl)) {
                    return {.err = HttpError::invalid_arg};
                }
                bool req_is_head = helper::equal(method, "HEAD");
                auto impl = new internal::HttpAsyncResponseImpl{};
                impl->io = std::move(io);
                impl->reqests = std::move(buf);
                impl->hostname = host;
                struct Defer {
                    decltype(impl) ptr;
                    bool no_del = false;
                    ~Defer() {
                        if (!no_del) {
                            delete ptr;
                        }
                    }
                } def{impl};
                auto got = impl->io.write(ctx, impl->reqests.c_str(), impl->reqests.size());
                if (got.err) {
                    return {.err = HttpError::write_request, .base_err = got.err};
                }
                auto seq = make_ref_seq(buf);
                size_t red = 0;
                wrap::string recvbuf;
                auto read_one = [&](size_t reqsize = 1024) {
                    recvbuf.resize(reqsize == 0 ? 1024 : reqsize);
                    auto data = impl->io.read(ctx, recvbuf.data(), recvbuf.size());
                    if (data.err) {
                        return data.err;
                    }
                    buf.append(recvbuf, 0, data.read);
                    red = data.read;
                    return 0;
                };
                while (true) {
                    if (auto err = read_one(); err != 0) {
                        return {.err = HttpError::read_header, .base_err = err};
                    }
                    if (!helper::starts_with(buf, "HTTP/1.1")) {
                        return {.err = HttpError::not_http_response, .base_err = -1};
                    }
                    while (seq.rptr < buf.size()) {
                        if (seq.seek_if("\r\n\r\n") || seq.seek_if("\n\n") ||
                            seq.seek_if("\r\r")) {
                            goto HEADER_RECVED;
                        }
                        seq.consume();
                    }
                }
            HEADER_RECVED:
                seq.rptr = 0;
                auto resp = internal::HttpSet::get(impl->response);
                if (!h1header::parse_response<wrap::string>(seq, helper::nop, resp->code, helper::nop, *resp,
                                                            h1body::bodyinfo_preview(impl->bodytype, impl->expect))) {
                    return {.err = HttpError::invalid_header};
                }
                auto rem = seq.remain();
                if (impl->expect) {
                    resp->body.reserve(impl->expect);
                }
                if (impl->bodytype == h1body::BodyType::no_info) {
                    if (!req_is_head || !seq.eos()) {
                        while (red == 1024) {
                            if (auto err = read_one(); err != 0) {
                                return {.err = HttpError::read_body, .base_err = err};
                            }
                        }
                    }
                }
                while (true) {
                    auto res = h1body::read_body(resp->body, seq, impl->expect, impl->bodytype);
                    if (res == State::failed) {
                        return {.err = HttpError::invalid_body};
                    }
                    if (res == State::complete) {
                        break;
                    }
                    if (seq.eos() && req_is_head) {
                        break;
                    }
                    if (auto err = read_one(impl->expect); err != 0) {
                        return {.err = HttpError::read_body, .base_err = err};
                    }
                }
                HttpAsyncResponse res;
                internal::HttpSet::set(res, impl);
                def.no_del = true;
                return {.resp = std::move(res)};
            }

            async::Future<HttpAsyncResult> STDCALL request_async(AsyncIOClose&& io, const char* host, const char* method, const char* path, Header&& header) {
                return net::start(
                    [](async::Context& ctx, auto... args) {
                        return request_async(ctx, std::forward<decltype(args)>(args)...);
                    },
                    std::move(io), host, method, path, std::move(header));
            }

            HttpAsyncResponse::~HttpAsyncResponse() {
                delete impl;
            }

            AsyncIOClose HttpAsyncResponse::get_io() {
                if (!impl) {
                    return nullptr;
                }
                return std::move(impl->io);
            }

            Header HttpAsyncResponse::response() {
                if (!impl) {
                    return nullptr;
                }
                return std::move(impl->response);
            }

            HttpAsyncResponse& HttpAsyncResponse::operator=(HttpAsyncResponse&& in) {
                delete impl;
                impl = std::exchange(in.impl, nullptr);
                return *this;
            }
        }  // namespace http
    }      // namespace net
}  // namespace utils
