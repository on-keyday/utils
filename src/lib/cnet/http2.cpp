/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include "../../include/platform/windows/dllexport_source.h"
#include <deprecated/cnet/http2.h>
#include "../../include/net/http2/frame.h"
#include "../../include/net/http2/request_methods.h"
#include "../net/http2/frame_reader.h"
#include "../../include/wrap/light/vector.h"
#include "../../include/number/array.h"
#include "../../include/net_util/hpack.h"
#include "../net/http2/stream_impl.h"
#include "../../include/wrap/pair_iter.h"
#include "../../include/net/http/http_headers.h"

#define LOGHTTP2(ctx, record, msg) record.log(logobj{ \
    __FILE__,                                         \
    __LINE__,                                         \
    "http2", msg, ctx})

namespace utils {
    namespace cnet {
        namespace http2 {

            struct Http2State {
                net::http2::Connection ctx;
                net::http2::internal::FrameReader<> r;

                ReadCallback rcb{0};

                WriteCallback wcb{0};

                bool goneaway = false;
                bool opened = false;

                void delete_read_callback() {
                    if (rcb.deleter) {
                        rcb.deleter(rcb.this_);
                    }
                    rcb = {0};
                }

                void delete_write_callback() {
                    if (wcb.deleter) {
                        wcb.deleter(wcb.this_);
                    }
                    wcb = {0};
                }

                ~Http2State() {
                    delete_read_callback();
                    delete_write_callback();
                }
            };

            struct Frames {
                wrap::vector<wrap::shared_ptr<net::http2::Frame>> frame;
                net::http2::UpdateResult result;
                wrap::string wreq;
                Http2State* state;
                CNet* low;
            };

            void callback_on_write(Http2State* state, const net::http2::Frame* frame) {
                if (state->wcb.callback) {
                    state->wcb.callback(state, frame);
                }
            }

            bool STDCALL default_proc(Frames* fr, wrap::shared_ptr<net::http2::Frame>& frame, DefaultProc filter) {
                if (any(filter & DefaultProc::ping)) {
                    if (frame->type == net::http2::FrameType::ping) {
                        if (!(frame->flag & net::http2::Flag::ack)) {
                            auto save = frame->flag;
                            frame->flag |= net::http2::Flag::ack;
                            auto& ref = *frame;
                            fr->state->ctx.update_send(ref);
                            net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                            net::http2::encode(&ref, w);
                            callback_on_write(fr->state, &ref);
                            frame->flag = save;
                        }
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
                        net::http2::encode(update, w);
                        callback_on_write(fr->state, &update);
                        update.id = frame->id;
                        fr->state->ctx.update_send(update);
                        net::http2::encode(update, w);
                        callback_on_write(fr->state, &update);
                    }
                }
                if (any(filter & DefaultProc::settings)) {
                    if (frame->type == net::http2::FrameType::settings) {
                        if (!(frame->flag & net::http2::Flag::ack)) {
                            auto save = frame->flag;
                            auto save2 = frame->len;
                            frame->len = 0;
                            frame->flag |= net::http2::Flag::ack;
                            net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                            auto ptr = &*frame;
                            net::http2::encode(ptr, w);
                            callback_on_write(fr->state, ptr);
                            frame->flag = save;
                            frame->len = save2;
                        }
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

            wrap::shared_ptr<net::http2::Frame>* STDCALL begin(Frames* fr) {
                return fr ? fr->frame.data() + 0 : nullptr;
            }

            wrap::shared_ptr<net::http2::Frame>* STDCALL end(Frames* fr) {
                return fr ? fr->frame.data() + fr->frame.size() : nullptr;
            }

            struct UnwrapCb {
                mutable std::pair<std::string, std::string> obj;
                bool first = true;
                void fetching(auto& v) {
                    constexpr auto valid = net::h1header::default_validator();
                    constexpr auto h2key = net::h1header::http2_key_validator();
                    while (!v.on_end()) {
                        v.fetch(obj.first, obj.second);
                        if (!h2key(obj) && !valid(obj)) {
                            obj.first.clear();
                            obj.second.clear();
                            continue;
                        }
                        if (helper::equal(obj.first, "connection", helper::ignore_case()) ||
                            helper::equal(obj.first, "host", helper::ignore_case())) {
                            continue;
                        }
                        auto to_lower = [](auto& c) {
                            c = helper::to_lower(c);
                        };
                        std::for_each(obj.first.begin(), obj.first.end(), to_lower);
                        break;
                    }
                    first = false;
                }

                auto& unwrap(auto& v) {
                    if (first) {
                        fetching(v);
                    }
                    return obj;
                }

                auto const_unwrap(auto&) {
                    return obj;
                }

                void increment(auto& v) {
                    obj.first.clear();
                    obj.second.clear();
                    first = true;
                }

                void decrement(auto&) {}

                bool equal(auto& v1, auto&) const {
                    return v1.on_end();
                }
            };

            bool STDCALL write_window_update(Frames* fr, std::uint32_t increment, std::int32_t id) {
                if (!fr) {
                    return false;
                }
                net::http2::WindowUpdateFrame update;
                update.type = net::http2::FrameType::window_update;
                update.increment = increment;
                update.len = 4;
                update.id = id;
                net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                net::http2::encode(update, w);
                return true;
            }

            bool STDCALL read_header(Frames* fr, RFetcher fetch, std::int32_t id) {
                auto stream = fr->state->ctx.stream(id);
                if (!stream) {
                    return false;
                }
                auto& impl = net::http2::internal::get_impl(*stream);
                for (auto& kv : impl->h) {
                    fetch.emplace(kv.first.c_str(), kv.second.c_str());
                }
                return true;
            }

            bool STDCALL read_data(Frames* fr, helper::IPushBacker pb, std::int32_t id) {
                auto stream = fr->state->ctx.stream(id);
                if (!stream) {
                    return false;
                }
                auto& impl = net::http2::internal::get_impl(*stream);
                helper::append(pb, impl->data);
                return true;
            }

            bool STDCALL write_header(Frames* fr, Fetcher fetch, std::int32_t& id, bool no_data, net::http2::Priority* prio) {
                if (!fr) {
                    return false;
                }
                wrap::string buf;
                auto& b = net::http2::internal::get_impl(fr->state->ctx);
                auto range = wrap::make_wraprange(fetch, UnwrapCb{}, 0, wrap::NothingUnwrap{});
                auto err = net::hpack::encode(buf, b->send.encode_table, range, b->send.setting[k(net::http2::SettingKey::table_size)]);
                if (!err) {
                    return false;
                }
                net::http2::HeaderFrame frame;
                if (!fr->state->ctx.split_header(frame, buf)) {
                    return false;
                }
                if (no_data) {
                    frame.flag |= net::http2::Flag::end_stream;
                }
                if (prio) {
                    frame.flag |= net::http2::Flag::priority;
                    frame.priority = *prio;
                }
                net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                net::http2::encode(frame, w);
                callback_on_write(fr->state, &frame);
                id = frame.id;
                while (buf.size()) {
                    net::http2::Continuation cont;
                    fr->state->ctx.make_continuous(id, buf, cont);
                    net::http2::encode(cont, w);
                    callback_on_write(fr->state, &cont);
                }
                return true;
            }

            void STDCALL write_goaway(Frames* fr, std::uint32_t code, bool notify_graceful) {
                net::http2::GoAwayFrame goaway;
                goaway.type = net::http2::FrameType::goaway;
                goaway.id = 0;
                goaway.code = code;
                auto& impl = net::http2::internal::get_impl(fr->state->ctx);
                if (notify_graceful) {
                    constexpr auto graceful = static_cast<std::uint32_t>(~0) >> 1;
                    goaway.processed_id = graceful;
                }
                else {
                    goaway.processed_id = impl->id_max;
                }
                goaway.len = 8;
                net::http2::internal::FrameWriter<wrap::string&> w{fr->wreq};
                net::http2::encode(goaway, w);
                callback_on_write(fr->state, &goaway);
                fr->state->goneaway = true;
            }

            Error open_http2(Stopper stop, CNet* ctx, Http2State* state) {
                auto low = cnet::get_lowlevel_protocol(ctx);
                if (!low) {
                    return consterror{"http2: low level protocol is nullptr"};
                }
                size_t w = 0;
                if (!write(stop, low, preface, 24, &w)) {
                    return consterror{"http2: failed to write connection preface"};
                }
                net::http2::internal::FrameWriter<wrap::string> wr;
                net::http2::SettingsFrame frame;
                auto sets = {std::pair{net::http2::SettingKey::enable_push, 0}};
                state->ctx.make_settings(frame, sets);
                state->ctx.update_send(frame);
                net::http2::encode(&frame, wr);
                callback_on_write(state, &frame);
                if (!write(stop, low, wr.str.c_str(), wr.str.size(), &w)) {
                    return consterror{"http2: failed to write initial settings"};
                }
                state->opened = true;
                return nil();
            }

            bool STDCALL flush_write_buffer(Frames* fr) {
                if (!fr) {
                    return false;
                }
                if (fr->wreq.size()) {
                    size_t s;
                    if (!write({}, fr->low, fr->wreq.c_str(), fr->wreq.size(), &s)) {
                        fr->result.err = net::http2::H2Error::transport;
                        fr->result.detail = net::http2::StreamError::writing_frame;
                        return false;
                    }
                    fr->wreq.clear();
                }
                return true;
            }

            bool callback_and_write(Stopper stop, CNet* ctx, Http2State* state, Frames& frames, bool& ret) {
                auto res = state->rcb.callback(state->rcb.this_, &frames);
                LOGHTTP2(ctx, stop, "reading callback invoked");
                if (!flush_write_buffer(&frames)) {
                    state->rcb.callback(state->rcb.this_, &frames);
                    ret = false;
                    return false;
                }
                ret = true;
                return res;
            }

            bool poll_frame(Stopper stop, CNet* ctx, Http2State* state) {
                if (!state->opened || !state->rcb.callback) {
                    return false;
                }
                auto low = cnet::get_lowlevel_protocol(ctx);
                Frames frames;
                frames.state = state;
                frames.low = low;
                bool ret = false;
                auto read_frames = [&] {
                    while (state->r.ref.size()) {
                        wrap::shared_ptr<net::http2::Frame> frame;
                        auto store = state->r.pos;
                        auto err = net::http2::decode(state->r, frame);
                        if (any(err & net::http2::H2Error::user_defined_bit)) {
                            state->r.pos = store;
                            state->r.tidy();
                            break;
                        }
                        else if (err != net::http2::H2Error::none) {
                            frames.result.err = err;
                            frames.result.detail = net::http2::StreamError::internal_data_read;
                            callback_and_write(stop, ctx, state, frames, ret);
                            return false;
                        }
                        frames.result = state->ctx.update_recv(*frame);
                        if (frames.result.err != net::http2::H2Error::none) {
                            callback_and_write(stop, ctx, state, frames, ret);
                            return false;
                        }
                        frames.frame.push_back(std::move(frame));
                    }
                    return true;
                };
                bool on_error = false;
                if (!callback_and_write(stop, ctx, state, frames, ret)) {
                    return ret;
                }
                while (true) {
                    if (!read_frames()) {
                        return false;
                    }
                    if (!frames.frame.size()) {
                        while (true) {
                            number::Array<1024, char> buf;
                            if (on_error || !read({}, ctx, buf.buf, buf.capacity(), &buf.i)) {
                                if (!on_error && state->r.ref.size()) {
                                    on_error = true;
                                    break;
                                }
                                if (state->goneaway) {
                                    return true;
                                }
                                frames.result.err = net::http2::H2Error::transport;
                                frames.result.detail = net::http2::StreamError::reading_frame;
                                callback_and_write(stop, ctx, state, frames, ret);
                                return false;
                            }
                            state->r.ref.append(buf.c_str(), buf.size());
                            if (buf.size() == buf.capacity()) {
                                continue;
                            }
                            break;
                        }
                        continue;
                    }
                    if (!callback_and_write(stop, ctx, state, frames, ret)) {
                        return ret;
                    }
                    frames.frame.clear();
                }
            }

            bool http2_settings(CNet* ctx, Http2State* state, std::int64_t key, void* value) {
                if (key == config_frame_poll) {
                    return poll_frame({}, ctx, state);
                }
                else if (key == config_set_read_callback) {
                    auto ptr = static_cast<ReadCallback*>(value);
                    state->delete_read_callback();
                    state->rcb = *ptr;
                    return true;
                }
                else if (key == config_set_write_callback) {
                    auto ptr = static_cast<WriteCallback*>(value);
                    state->delete_write_callback();
                    state->wcb = *ptr;
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
