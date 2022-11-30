/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <platform/windows/dllexport_source.h>

#include <net_util/hpack.h>
#include "../http/header_impl.h"
#include "frame_reader.h"
#include "stream_impl.h"
#include "../http/header_impl.h"
#include "../http/async_resp_impl.h"

namespace utils {
    namespace net {
        namespace http2 {
            Connection::Connection() {
                impl = wrap::make_shared<internal::ConnectionImpl>();
            }

            Stream* Connection::stream(int id) {
                return impl->get_stream(id);
            }

            http::HttpAsyncResponse Stream::response() {
                http::Header h;
                auto ptr = http::internal::HttpSet::get(h);
                ptr->changed = true;
                auto found = impl->h.find(":status");
                if (found) {
                    ptr->code.append(*found);
                }
                ptr->order = impl->h.order;
                ptr->body = impl->data;
                ptr->version = 2;
                http::HttpAsyncResponse resp;
                auto to_set = new http::internal::HttpAsyncResponseImpl{};
                to_set->response = std::move(h);
                http::internal::HttpSet::set(resp, to_set);
                return resp;
            }

            const char* Stream::peek_header(const char* key, size_t index) const {
                auto ptr = impl->h.find(key, index);
                if (ptr) {
                    return ptr->c_str();
                }
                return nullptr;
            }

            Stream* Connection::new_stream() {
                return impl->get_stream(impl->next_stream());
            }

            int Connection::errcode() {
                return impl->code;
            }

            std::int32_t Connection::max_proced() const {
                return impl->id_max;
            }

            bool Connection::make_data(std::int32_t id, wrap::string& data, DataFrame& frame, bool& block) {
                block = false;
                auto stream = impl->get_stream(id);
                if (!stream) {
                    return false;
                }
                auto& f = frame;
                if (stream->status() != Status::open && stream->status() != Status::half_closed_remote) {
                    return false;
                }
                const auto conn_window = impl->send.window;
                const auto stream_window = stream->impl->send_window;
                if (conn_window < 0 || stream_window < 0) {
                    block = true;
                    return false;
                }
                const auto frame_max = impl->send.setting[k(SettingKey::max_frame_size)];
                const auto require_size = data.size();
                const auto use_window = conn_window < stream_window ? conn_window : stream_window;
                const auto use_size = frame_max < require_size ? frame_max : require_size;
                const auto use = use_window < use_size ? use_window : use_size;
                f = {};
                f.type = FrameType::data;
                f.id = id;
                if (use < require_size) {
                    auto b = data.begin();
                    auto e = data.begin() + use;
                    f.data.assign(b, e);
                    data.erase(b, e);
                }
                else {
                    f.data = std::move(data);
                    f.flag |= Flag::end_stream;
                }
                if (use_window == use) {
                    block = true;
                }
                return true;
            }

            bool Connection::make_header(http::Header&& h, HeaderFrame& f, wrap::string& remain) {
                f = {};
                f.type = FrameType::header;
                auto raw_h = http::internal::HttpSet::get(h);
                if (!raw_h) {
                    return false;
                }
                auto cpy = raw_h->order;
                constexpr auto validator_ = h1header::default_validator();
                for (auto& kv : cpy) {
                    if (helper::equal(kv.first, ":path") ||
                        helper::equal(kv.first, ":authority") ||
                        helper::equal(kv.first, ":status") ||
                        helper::equal(kv.first, ":scheme") ||
                        helper::equal(kv.first, ":method")) {
                        // nothing to do
                    }
                    else if (!validator_(kv)) {
                        return false;
                    }
                    if (helper::equal(kv.first, "host") ||
                        helper::equal(kv.second, "connection")) {
                        continue;
                    }
                    for (auto& c : kv.first) {
                        c = helper::to_lower(c);
                    }
                    for (auto& c : kv.second) {
                        c = helper::to_lower(c);
                    }
                }
                if (auto e = hpack::encode(f.data, impl->send.encode_table, cpy, impl->send.setting[k(SettingKey::table_size)], true); !e) {
                    return false;
                }
                auto mxsz = impl->send.setting[k(SettingKey::max_frame_size)];
                if (f.data.size() > mxsz) {
                    auto b = f.data.begin() + mxsz;
                    auto e = f.data.end();
                    remain.assign(b, e);
                    f.data.erase(mxsz, ~0);
                }
                else {
                    f.flag |= Flag::end_headers;
                }
                f.len = (int)f.data.size();
                auto stream = new_stream();
                if (!stream) {
                    return false;
                }
                f.id = stream->id();
                return true;
            }

            bool Connection::split_header(HeaderFrame& f, wrap::string& remain) {
                f = {};
                f.type = FrameType::header;
                auto mxsz = impl->send.setting[k(SettingKey::max_frame_size)];
                f.data = std::move(remain);
                if (f.data.size() > mxsz) {
                    auto b = f.data.begin() + mxsz;
                    auto e = f.data.end();
                    remain.assign(b, e);
                    f.data.erase(mxsz, ~0);
                }
                else {
                    f.flag |= Flag::end_headers;
                }
                f.len = (int)f.data.size();
                auto stream = new_stream();
                if (!stream) {
                    return false;
                }
                f.id = stream->id();
                return true;
            }

            bool Connection::make_continuous(std::int32_t id, wrap::string& remain, Continuation& f) {
                if (remain.size() == 0) {
                    return false;
                }
                f.id = id;
                f.type = FrameType::continuous;
                auto mxsz = impl->send.setting[k(SettingKey::max_frame_size)];
                f.data = std::move(remain);
                if (f.data.size() > mxsz) {
                    auto b = f.data.begin() + mxsz;
                    auto e = f.data.end();
                    remain.assign(b, e);
                    f.data.erase(mxsz, ~0);
                }
                else {
                    f.flag |= Flag::end_headers;
                }
                return true;
            }
        }  // namespace http2
    }      // namespace net
}  // namespace utils
