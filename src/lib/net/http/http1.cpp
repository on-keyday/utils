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

#include <algorithm>

namespace utils {
    namespace net {
        namespace internal {
            struct HeaderImpl {
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
            };
        }  // namespace internal

        Header::Header() {
            impl = new internal::HeaderImpl();
        }

        Header::~Header() {
            delete impl;
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
            io.write(buf.c_str(), buf.size());
        }

    }  // namespace net
}  // namespace utils
