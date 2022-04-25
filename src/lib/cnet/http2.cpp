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

            struct Http2State {
                net::http2::Connection ctx;
                net::http2::internal::FrameReader<> r;
                void* callback_this = nullptr;
                bool (*callback)(void* this_, Frames* frames) = nullptr;
                void (*deleter)(void*) = nullptr;
                bool opened = false;

                void delete_callback() {
                    if (deleter) {
                        deleter(callback_this);
                    }
                    callback = nullptr;
                    callback_this = nullptr;
                }

                ~Http2State() {
                    delete_callback();
                }
            };

            struct Frames {
                wrap::vector<wrap::shared_ptr<net::http2::Frame>> frame;
                net::http2::UpdateResult result;
                wrap::string wreq;
                Http2State* state;
            };

            bool STDCALL default_proc(Frames* fr, wrap::shared_ptr<net::http2::Frame>& frame, DefaultProc filter) {
                if (any(filter & DefaultProc::ping)) {
                    if (!(frame->flag & net::http2::Flag::ack)) {
                        auto save = frame->flag;
                        frame->flag |= net::http2::Flag::ack;
                        fr->state->ctx.update_send(*frame);
                        net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                        net::http2::encode(&*frame, w);
                        frame->flag = save;
                    }
                }
                if (any(filter & DefaultProc::send_window_update)) {
                    if (frame->type == net::http2::FrameType::data) {
                        net::http2::WindowUpdateFrame update;
                        update.type = net::http2::FrameType::window_update;
                        update.id = 0;
                        update.increment = frame->len;
                        fr->state->ctx.update_send(update);
                        net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                        net::http2::encode(&update, w);
                        update.id = frame->id;
                        fr->state->ctx.update_send(update);
                        net::http2::encode(&update, w);
                    }
                }
                return true;
            }

            bool STDCALL default_procs(Frames* fr, DefaultProc filter) {
                for (auto& frame : fr->frame) {
                    default_proc(fr, frame, filter);
                }
                return true;
            }

            wrap::shared_ptr<net::http2::Frame>* begin(Frames* fr) {
                return fr ? fr->frame.data() + 0 : nullptr;
            }

            wrap::shared_ptr<net::http2::Frame>* end(Frames* fr) {
                return fr ? fr->frame.data() + fr->frame.size() : nullptr;
            }

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
                if (!write(low, wr.str.c_str(), wr.str.size(), &w)) {
                    return false;
                }
                state->opened = true;
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
                if (!state->opened || !state->callback) {
                    return false;
                }
                auto low = cnet::get_lowlevel_protocol(ctx);
                Frames frames;
                frames.state = state;
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

            bool http2_settings(CNet* ctx, Http2State* state, std::int64_t key, void* value) {
                if (key == poll) {
                    return poll_frame(ctx, state);
                }
                else if (key == set_callback) {
                    auto ptr = static_cast<Callback*>(value);
                    state->delete_callback();
                    state->callback = ptr->callback;
                    state->callback_this = ptr->this_;
                    state->deleter = ptr->deleter;
                    return true;
                }
                return false;
            }

            CNet* STDCALL create_client() {
                cnet::ProtocolSuite<Http2State> protocol{
                    .initialize = open_http2,
                    .settings_ptr = http2_settings,
                    .deleter = [](Http2State* state) { delete state; },
                };
                return cnet::create_cnet(CNetFlag::once_set_no_delete_link | CNetFlag::init_before_io, new Http2State{}, protocol);
            }
        }  // namespace http2
    }      // namespace cnet
}  // namespace utils