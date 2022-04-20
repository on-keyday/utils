/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/platform/windows/dllexport_source.h"
#include "../../include/cnet/http2.h"
#include "../../include/net/http2/frame.h"
#include "../../include/net/http2/request_methods.h"
#include "../net/http2/frame_reader.h"
#include "../../include/wrap/light/vector.h"
#include "../../include/number/array.h"

namespace utils {
    namespace cnet {
        namespace http2 {

            struct Frames {
                wrap::vector<wrap::shared_ptr<net::http2::Frame>> frame;
                net::http2::UpdateResult result;
                wrap::string* opaque = nullptr;
                wrap::string wreq;
            };

            struct Http2State {
                net::http2::Connection ctx;
                net::http2::internal::FrameReader<> r;
                void* callback_this;
                bool (*callback)(void* this_, Frames* frames);
            };

            constexpr auto preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

            bool open_http2(CNet* ctx, Http2State* state) {
                auto low = cnet::get_lowlevel_protocol(ctx);
                if (!low) {
                    return false;
                }
                size_t w = 0;
                if (!write(low, preface, 24, &w)) {
                    return false;
                }
                net::http2::internal::FrameWriter<wrap::string> wr;
                net::http2::SettingsFrame frame;
                auto sets = {std::pair{net::http2::SettingKey::enable_push, 0}};
                state->ctx.make_settings(frame, sets);
                state->ctx.update_send(frame);
                net::http2::encode(&frame, wr);
                if (!write(low, frame.setting.c_str(), frame.setting.size(), &w)) {
                    return false;
                }
                return true;
            }

            bool callback_and_write(CNet* ctx, CNet* low, Http2State* state, Frames& frames, bool& ret) {
                auto res = state->callback(state->callback_this, &frames);
                cnet::invoke_callback(ctx);
                if (frames.wreq.size()) {
                    size_t s;
                    if (!write(low, frames.wreq.c_str(), frames.wreq.size(), &s)) {
                        frames.result.err = net::http2::H2Error::transport;
                        frames.result.detail = net::http2::StreamError::writing_frame;
                        state->callback(state->callback_this, &frames);
                        ret = false;
                        return false;
                    }
                }
                ret = true;
                return res;
            }

            bool poll_frame(CNet* ctx, Http2State* state) {
                if (!state->callback) {
                    return false;
                }
                auto low = cnet::get_lowlevel_protocol(ctx);
                Frames frames;
                frames.opaque = &state->r.ref;
                bool ret = false;
                auto read_frames = [&] {
                    while (state->r.ref.size()) {
                        wrap::shared_ptr<net::http2::Frame> frame;
                        auto store = state->r.pos;
                        auto err = net::http2::decode(state->r, frame);
                        if (any(err & net::http2::H2Error::user_defined_bit)) {
                            state->r.tidy();
                            break;
                        }
                        else if (err != net::http2::H2Error::none) {
                            frames.result.err = err;
                            frames.result.detail = net::http2::StreamError::internal_data_read;
                            callback_and_write(ctx, low, state, frames, ret);
                            return false;
                        }
                        frames.result = state->ctx.update_recv(*frame);
                        if (frames.result.err != net::http2::H2Error::none) {
                            callback_and_write(ctx, low, state, frames, ret);
                            return false;
                        }
                        frames.frame.push_back(std::move(frame));
                    }
                    return true;
                };
                while (true) {
                    if (!read_frames()) {
                        return false;
                    }
                    if (!frames.frame.size()) {
                        while (true) {
                            number::Array<1024, char> buf;
                            if (!read(ctx, buf.buf, buf.capacity(), &buf.i)) {
                                frames.result.err = net::http2::H2Error::transport;
                                frames.result.detail = net::http2::StreamError::reading_frame;
                                callback_and_write(ctx, low, state, frames, ret);
                                return false;
                            }
                            state->r.ref.append(buf.c_str(), buf.size());
                            if (buf.size() == buf.capacity()) {
                                continue;
                            }
                        }
                        continue;
                    }
                    if (!callback_and_write(ctx, low, state, frames, ret)) {
                        return ret;
                    }
                    frames.frame.clear();
                    frames.wreq.clear();
                }
            }

            bool settings(CNet* ctx, Http2State* state, std::int64_t key, void* value) {
                if (key == poll) {
                    return poll_frame(ctx, state);
                }
                else if (key == set_callback) {
                    auto ptr = static_cast<Callback*>(value);
                    state->callback = ptr->callback;
                    state->callback_this = ptr->this_;
                    return true;
                }
                return false;
            }

            CNet* STDCALL create_client() {
                cnet::ProtocolSuite<Http2State> protocol{
                    .initialize = open_http2,

                };
                return cnet::create_cnet(CNetFlag::once_set_no_delete_link, new Http2State{}, protocol);
            }
        }  // namespace http2
    }      // namespace cnet
}  // namespace utils
