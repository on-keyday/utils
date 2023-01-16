/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// header_impl - http1 header impl
#pragma once
#include <net_util/http/header.h>
#include <wrap/light/vector.h>
#include <wrap/light/string.h>
#include <deprecated/net/http/http1.h>
#include <algorithm>
#include <helper/equal.h>

namespace utils {
    namespace net {
        namespace http {
            namespace internal {
                struct HttpSet {
                    static wrap::shared_ptr<HeaderImpl>& get(Header& p) {
                        return p.impl;
                    }

                    static void set(HttpAsyncResponse& p, HttpAsyncResponseImpl* impl) {
                        p.impl = impl;
                    }
                };

                struct HeaderImpl {
                    h1header::StatusCode code;
                    wrap::vector<std::pair<wrap::string, wrap::string>> order;
                    wrap::string body;
                    wrap::string raw_header;
                    int version = 1;
                    bool changed = false;

                    void emplace(auto&& key, auto&& value) {
                        order.emplace_back(key, value);
                        changed = true;
                    }

                    auto begin() const {
                        return order.begin();
                    }

                    auto end() const {
                        return order.end();
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
                            return std::addressof(std::get<1>(*found));
                        }
                        return nullptr;
                    }
                };

            }  // namespace internal
        }      // namespace http
    }          // namespace net
}  // namespace utils