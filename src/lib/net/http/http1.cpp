/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../../include/net/http/http1.h"
#include "../../../include/net/http/header.h"
#include "../../../include/wrap/lite/string.h"
#include "../../../include/wrap/lite/map.h"
#include "../../../include/wrap/lite/vector.h"

#include "../../../include/helper/strutil.h"
#include "../../../include/number/to_string.h"
#include "../../../include/net/http/body.h"

#include <algorithm>

namespace utils {
    namespace net {
        enum class HttpState {
            requesting,
            body_sending,
            responding,
            body_recving,
            failed,
        };

        namespace internal {
            struct HeaderImpl {
                h1header::StatusCode code;
                wrap::vector<std::pair<wrap::string, wrap::string>> order;
                wrap::string body;

                void emplace(auto&& key, auto&& value) {
                    order.emplace_back(std::move(key), std::move(value));
                }

                auto begin() {
                    return order.begin();
                }

                auto end() {
                    return order.end();
                }

                wrap::string* find(auto& key, size_t idx = 0) {
                    size_t count = 0;
                    auto found = std::find_if(order.begin(), order.end(), [&](auto& v) {
                        if (helper::equal(std::get<0>(v), key, helper::ignore_case())) {
                            if (idx == count) {
                                return true;
                            }
                            count++;
                        }
                        return false;
                    });
                    if (found != order.end()) {
                        return std::addressof(std::get<0>(*found));
                    }
                    return nullptr;
                }
            };

            struct HttpResponseImpl {
                HeaderImpl* header = nullptr;
                HttpState state;
                IOClose io;
                wrap::string buf;
                size_t redpos = 0;
                Header response;
                h1body::BodyType bodytype = h1body::BodyType::no_info;
                size_t expect = 0;
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
            impl = new internal::HeaderImpl();
        }

        Header::~Header() {
            delete impl;
        }

        Header::Header(Header&& in) {
            impl = in.impl;
            in.impl = nullptr;
        }

        Header& Header::operator=(Header&& in) {
            delete impl;
            impl = in.impl;
            in.impl = nullptr;
            return *this;
        }

        Header HttpResponse::get_response() {
            if (!impl || impl->state == HttpState::failed) {
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
                auto res = impl->io.write(impl->buf.c_str(), impl->buf.size());
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
                    auto res = impl->io.write(impl->header->body.c_str(), impl->header->body.size());
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
                e = h1body::read_body(impl->response.impl->body, seq, impl->bodytype, impl->expect);
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
                    goto BEGIN;
                }
            }
        }

        HttpResponse request(IOClose&& io, const char* host, const char* method, const char* path, Header&& header) {
            if (!io || !host || !method || !path) {
                return HttpResponse{};
            }
            if (!header.impl) {
                return HttpResponse{};
            }
            constexpr auto validator = h1header::default_validator();
            if (!validator(std::pair{"Host", host})) {
                return HttpResponse{};
            }
            wrap::string buf;
            auto res = h1header::render_request(
                buf, method, path, *header.impl,
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
                    number::to_string(pb, header.impl->body.size());
                    helper::append(str, pb.buf);
                    helper::append(str, "\r\n");
                });
            if (!res) {
                return HttpResponse{};
            }
            HttpResponse response;
            response.impl = new internal::HttpResponseImpl{};
            auto done = io.write(buf.c_str(), buf.size());
            if (done != State::complete && done != State::running) {
                return HttpResponse{};
            }
            response.impl->header = header.impl;
            header.impl = nullptr;
            response.impl->state = done == State::complete ? HttpState::requesting : HttpState::body_sending;
            response.impl->buf = std::move(buf);
            return response;
        }

    }  // namespace net
}  // namespace utils
