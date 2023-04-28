/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/dll/dllcpp.h>
#include <fnet/http2/http2.h>
#include <helper/appender.h>
#include <view/charvec.h>
#include <helper/pushbacker.h>
#include <helper/strutil.h>
#include <unordered_map>
#include <fnet/dll/glheap.h>
#include <fnet/http2/h2frame.h>
#include <fnet/http2/h2state.h>
#include <net_util/hpack/hpack.h>
#include <functional>
#include <string_view>
#include <optional>
#include <vector>

namespace std {
    template <>
    struct hash<utils::fnet::flex_storage> {
        auto operator()(const utils::fnet::flex_storage& str) const {
            constexpr auto hash = std::hash<std::string_view>{};
            return hash(std::string_view(str.as_char(), str.size()));
        }
    };
}  // namespace std

namespace utils {
    namespace fnet::http2 {
        /*
        quic::allocate::Alloc alloc_init{
            .alloc = [](void*, size_t s, size_t) { return static_cast<void*>(get_cvec(s, DNET_DEBUG_MEMORY_LOCINFO(true, s))); },
            .resize = [](void*, void* p, size_t s, size_t) {
                auto c=static_cast<char*>(p);
                resize_cvec(c , s, DNET_DEBUG_MEMORY_LOCINFO(false,0));
                return static_cast<void*>(c); },
            .put = [](void*, void* c) { free_cvec(static_cast<char*>(c), DNET_DEBUG_MEMORY_LOCINFO(false, 0)); },
        };*/
        struct WrapedHeader {
            using strpair = std::pair<flex_storage, flex_storage>;
            std::vector<strpair, glheap_allocator<strpair>> buf;
            void emplace(auto&& key, auto&& value) {
                buf.push_back(strpair{std::forward<decltype(key)>(key), std::forward<decltype(value)>(value)});
            }
        };

        struct Stream {
            stream::StreamNumState state;
            frame::Priority prio;
            flex_storage data_buf;
            flex_storage header_buf;
            WrapedHeader header;
            int rst_code;
        };

        struct HTTP2Connection {
            flex_storage input;
            flex_storage output;
            using HashMap = std::unordered_map<int, Stream, std::hash<int>, std::equal_to<int>, glheap_objpool_allocator<std::pair<const int, Stream>>>;
            HashMap streams;
            stream::ConnState conn;
            flex_storage debug;
            hpack::HpackError hpkerr{};
            bool preface_ok = false;

            HTTP2StreamCallback stream_cb = nullptr;
            void* strm_user = nullptr;

            HTTP2ConnectionCallback conn_cb = nullptr;
            void* conn_user = nullptr;
        };

        void warning(HTTP2Connection* c, const char* msg) {
            auto debug_ = msg ? 1 : 0;
            auto debug__ = debug_;
        }

        stream::State& state(HTTP2Connection* c) {
            return c->conn.state.s0.com.state;
        }

        bool is_closed(HTTP2Connection* c) {
            return state(c) == stream::State::closed;
        }

        HTTP2Connection* check_opt(void* opt, int& err) {
            if (!opt) {
                err = http2_invalid_opt;
            }
            return static_cast<HTTP2Connection*>(opt);
        }

        static bool check_opt(void* opt, int& err, auto&& callback) {
            auto c = check_opt(opt, err);
            if (!c) {
                return false;
            }
            return callback(c);
        }

        HTTP2::~HTTP2() {
            auto c = check_opt(opt, err);
            if (c) {
                delete_with_global_heap(c, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(*c), alignof(HTTP2Connection)));
            }
        }

        bool HTTP2::provide_http2_data(const void* data, size_t size) {
            return check_opt(opt, err, [&](HTTP2Connection* s) {
                helper::append(s->input, view::CharVec(static_cast<const char*>(data), size));
                return true;
            });
        }

        bool HTTP2::receive_http2_data(void* data, size_t size, size_t* red) {
            return check_opt(opt, err, [&](HTTP2Connection* s) {
                if (!data || !red) {
                    err = http2_invalid_argument;
                    return false;
                }
                auto pb = helper::CharVecPushbacker(static_cast<char*>(data), size);
                helper::append(pb, s->output);
                s->output.shift_front(pb.size_);
                *red = pb.size();
                return true;
            });
        }

        bool fast_enque_frame(HTTP2Connection* s, frame::Frame& f, ErrCode& err) {
            auto& p = s->output;
            const auto old = p.expand(frame::frame_len(f));
            if (p.size() == old) {
                err.err = http2_unknown_type;
                return false;
            }
            io::writer w{p};
            w.offset(old);
            if (!frame::render_frame(w, f)) {
                err.err = http2_invalid_data;
                p.resize(old);
                return false;
            }
            return true;
        }

        bool enque_settings(HTTP2Connection* s) {
            auto& sets = s->conn.send.settings;
            if (!sets.size()) {
                char buf[42];
                auto settings = h2set::Vec(buf, sizeof(buf));
                auto res = h2set::write_predefined_settings(settings, s->conn.state.send.settings, false);
                assert(res);
                helper::append(sets, settings);
            }
            frame::SettingsFrame f;
            f.id = 0;
            f.len = sets.size();
            f.settings = sets;
            f.flag = {};
            return fast_enque_frame(s, f, s->conn.state.s0.com.errs);
        }

        bool idle_to_open(HTTP2Connection* s) {
            if (state(s) == stream::State::idle) {
                if (!s->conn.state.is_server) {
                    helper::append(s->output, view::rvec(http2_connection_preface));
                }
                if (!enque_settings(s)) {
                    state(s) = stream::State::closed;
                    return false;  // aborted
                }
                state(s) = stream::State::open;
            }
            return true;
        }

        bool check_preface(HTTP2Connection* s) {
            if (!s->preface_ok) {
                if (!helper::starts_with(s->input, http2_connection_preface)) {
                    state(s) = stream::State::closed;
                    s->conn.state.s0.com.errs.err = http2_invalid_preface;
                    s->conn.state.s0.com.errs.h2err = H2Error::protocol;
                    s->conn.state.s0.com.errs.debug = "connection preface not found";
                    return false;
                }
                constexpr auto len = strlen(http2_connection_preface);
                s->input.shift_front(len);
                s->preface_ok = true;
            }
            return true;
        }

        bool enque_frame(HTTP2Connection* s, stream::StreamNumState& sn, frame::Frame& f, ErrCode& err) {
            if (!idle_to_open(s)) {
                return false;
            }
            if (!stream::send_frame(s->conn.state, sn, f)) {
                err = sn.com.errs;
                sn.com.errs = {};
                return false;  // error detected
            }
            return fast_enque_frame(s, f, err);
        }

        void enque_conn_error(HTTP2Connection* s, ErrCode e) {
            if (is_closed(s)) {
                warning(s, "enque_conn_error called after conncetion closed");
                return;  // already sent?
            }
            frame::GoAwayFrame goaway;
            goaway.code = std::uint32_t(e.h2err);
            goaway.processed_id = s->conn.state.max_closed_id;
            goaway.debug = view::rvec(e.debug);
            ErrCode err;
            enque_frame(s, s->conn.state.s0, goaway, err);
            state(s) = stream::State::closed;
            s->conn.state.s0.com.errs = e;  // save connection error
        }

        void enque_strm_error(HTTP2Connection* s, Stream& strm, H2Error e, const char* msg) {
            if (is_closed(s)) {
                warning(s, "enque_strm_error called after connection closed");
                return;  // connection already aborted?
            }
            frame::RstStreamFrame rst{};
            rst.id = strm.state.com.id;
            rst.code = std::uint32_t(e);
            rst.len = 4;
            ErrCode err;
            enque_frame(s, strm.state, rst, err);
        }

        bool connection_update_recv(HTTP2Connection* s, frame::Frame* f) {
            if (f->type == frame::FrameType::ping) {
                if (f->flag & frame::Flag::ack) {
                    return true;  // nothing to do
                }
                ErrCode err{};
                f->flag |= frame::Flag::ack;
                if (!enque_frame(s, s->conn.state.s0, *f, err)) {
                    return false;
                }
            }
            if (f->type == frame::FrameType::settings) {
                if (f->flag & frame::Flag::ack) {
                    return true;  // nothing to do
                }
                auto set = static_cast<frame::SettingsFrame*>(f);
                auto update_settings = [&](std::uint16_t key, std::uint32_t value) {
                    using sk = h2set::Skey;
                    using e = H2Error;
                    auto& settings = s->conn.state.recv.settings;
                    if (key == k(sk::table_size)) {
                        settings.header_table_size = value;
                    }
                    if (key == k(sk::enable_push)) {
                        if (value != 0 && value != 1) {
                            enque_conn_error(s, {
                                                    .err = http2_settings,
                                                    .h2err = e::protocol,
                                                    .debug = "invalid SETTINGS_ENABLE_PUSH value",
                                                });
                            return;
                        }
                        settings.enable_push = value == 1;
                    }
                    if (key == k(sk::max_concurrent)) {
                        settings.max_concurrent_stream = value;
                    }
                    if (key == k(sk::initial_windows_size)) {
                        auto old = settings.initial_window_size;
                        stream::set_new_window(s->conn.state.s0.send.window, old, value);
                        for (auto& v : s->streams) {
                            stream::set_new_window(v.second.state.send.window, old, value);
                        }
                        settings.initial_window_size = value;
                    }
                    if (key == k(sk::max_frame_size)) {
                        settings.max_frame_size = value;
                    }
                    if (key == k(sk::header_list_size)) {
                        settings.max_header_list_size = value;
                    }
                };
                if (!h2set::read_settings(set->settings, set->len, update_settings)) {
                    enque_conn_error(s, {
                                            .err = http2_library_bug,
                                            .h2err = H2Error::frame_size,
                                            .debug = "invalid SettingsFrame length. settings frame size detection failed at parse_frame",
                                        });
                    return false;
                }
                f->flag |= frame::Flag::ack;
                ErrCode err{};
                if (!enque_frame(s, s->conn.state.s0, *f, err)) {
                    return false;
                }
            }
            if (f->type == frame::FrameType::goaway) {
                auto g = static_cast<frame::GoAwayFrame*>(f);
                s->conn.state.s0.com.errs.h2err = H2Error(g->code);
                s->debug = g->debug;
                state(s) = stream::State::closed;
            }
            return true;
        }

        bool stream_update_recv(HTTP2Connection* s, Stream& strm, frame::Frame* f) {
            using t = frame::FrameType;
            if (f->type == t::data) {
                auto d = static_cast<frame::DataFrame*>(f);
                helper::append(strm.data_buf, d->data);
            }
            if (f->type == t::header || f->type == t::continuous) {
                if (f->type == t::header) {
                    auto h = static_cast<frame::HeaderFrame*>(f);
                    helper::append(strm.header_buf, h->data);
                }
                else {
                    auto h = static_cast<frame::ContinuationFrame*>(f);
                    helper::append(strm.header_buf, h->data);
                }
                if (f->flag & frame::Flag::end_headers) {
                    s->hpkerr = hpack::decode(strm.header_buf, s->conn.recv.table, strm.header, s->conn.state.recv.settings.header_table_size);
                    if (s->hpkerr != hpack::HpackError::none) {
                        enque_conn_error(s, {.err = http2_hpack_error, .h2err = H2Error::compression, .debug = "HPACK decode failure"});
                        return false;
                    }
                }
            }
            if (f->type == t::priority) {
                strm.prio = static_cast<frame::PriorityFrame*>(f)->priority;
            }
            if (f->type == t::rst_stream) {
                strm.rst_code = static_cast<frame::RstStreamFrame*>(f)->code;
            }
            return true;
        }

        bool insert_stream(HTTP2Connection* s, int new_id) {
            // receiveing data/sending window_update/local settings
            auto recv_window = s->conn.state.send.settings.initial_window_size;
            // sending data/receiving window_update/remote settings
            auto send_window = s->conn.state.recv.settings.initial_window_size;
            auto res = s->streams.emplace(
                new_id,
                Stream{.state = {
                           .com = {.id = new_id},
                           .recv = {
                               .window = recv_window,
                           },
                           .send{
                               .window = send_window,
                           }}});
            return res.first != s->streams.end();
        }

        bool progress_callback(void* c, frame::Frame* f, ErrCode err) {
            auto s = static_cast<HTTP2Connection*>(c);
            stream::StreamNumState* strm;
            Stream* object = nullptr;
            if (f->id != 0) {
                insert_stream(s, f->id);  // for receivieng state
                auto ps = s->streams.find(f->id);
                if (ps == s->streams.end()) {
                    enque_conn_error(s, {
                                            .err = http2_resource,
                                            .h2err = H2Error::internal,
                                            .debug = "stream resource allocation failed",
                                        });
                    return false;
                }
                strm = &ps->second.state;
                object = &ps->second;
            }
            else {
                strm = &s->conn.state.s0;
            }
            if (err.err != http2_error_none) {
                if (err.strm && object) {
                    enque_strm_error(s, *object, err.h2err, err.debug);
                }
                else {
                    enque_conn_error(s, err);
                }
                return false;
            }
            if (!stream::recv_frame(s->conn.state, *strm, *f)) {
                if (strm->com.errs.strm && object) {
                    enque_strm_error(s, *object, strm->com.errs.h2err, strm->com.errs.debug);
                }
                else {
                    enque_conn_error(s, strm->com.errs);
                }
                return false;
            }
            if (stream::is_connection_level(f->type)) {
                if (!connection_update_recv(s, f)) {
                    return false;
                }
                if (s->conn_cb) {
                    s->conn_cb(s->conn_user, *f, s->conn);
                }
            }
            else if (stream::is_stream_level(f->type)) {
                if (!stream_update_recv(s, *object, f)) {
                    return false;
                }
                if (s->stream_cb) {
                    s->stream_cb(s->strm_user, *f, object->state);
                }
            }
            return true;
        }

        bool HTTP2::progress() {
            return check_opt(opt, err, [&](HTTP2Connection* s) {
                auto& p = s->input;
                if (!p.size()) {
                    return true;  // nothing to do
                }
                if (!check_preface(s)) {
                    return false;  // connection preface not exists
                }
                size_t red = 0;
                ErrCode code;
                io::reader r{p};
                frame::parse_frame(r, [&](frame::Frame& fr, frame::UnknownFrame& un, bool err) {
                    if (&fr == &un) {
                        return true;  // skip unknown type frame
                    }
                    ErrCode code;
                    if (err) {
                        code.h2err = H2Error::protocol;
                        code.debug = "invalid frame format";
                    }
                    return progress_callback(opt, &fr, code);
                });
                /*
                auto res = parse_frame(p.as_char(), p.size(), red, code, progress_callback, s);
                if (!res) {
                    size_t add = false;
                    pass_frame(p.as_char() + red, p.size() - red, add);
                    red += add;
                }*/
                p.shift_front(r.read().size());
                return true;
            });
        }

        HTTP2Stream HTTP2::create_stream(int new_id) {
            auto s = check_opt(opt, err);
            if (!s) {
                return {};
            }
            if (new_id <= 0) {
                new_id = stream::get_next_id(s->conn.state);
            }
            if (!insert_stream(s, new_id)) {
                err = http2_create_stream;
                return {};
            }
            return HTTP2Stream(opt, new_id);
        }

        HTTP2Stream HTTP2::get_stream(int id) {
            auto s = check_opt(opt, err);
            if (!s) {
                return {};
            }
            auto obj = s->streams.find(id);
            if (obj == s->streams.end()) {
                return HTTP2Stream{};
            }
            return HTTP2Stream(opt, id);
        }

        void HTTP2::fast_open() {
            check_opt(opt, err, [&](HTTP2Connection* c) {
                if (!is_closed(c)) {
                    state(c) = stream::State::open;
                }
                return true;
            });
        }

        bool HTTP2::flush_preface() {
            return check_opt(opt, err, [&](HTTP2Connection* c) {
                return idle_to_open(c);
            });
        }

        ErrCode HTTP2::get_conn_err() {
            int err;
            auto c = check_opt(opt, err);
            if (c) {
                return {};
            }
            return c->conn.state.s0.com.errs;
        }

        bool HTTP2::closed() const {
            int err;
            auto c = check_opt(opt, err);
            if (!c) {
                return true;
            }
            return state(c) == stream::State::closed;
        }

        void HTTP2::set_stream_callback(HTTP2StreamCallback cb, void* user) {
            check_opt(opt, err, [&](HTTP2Connection* c) {
                c->stream_cb = cb;
                c->strm_user = user;
                return true;
            });
        }

        void HTTP2::set_connection_callback(HTTP2ConnectionCallback cb, void* user) {
            check_opt(opt, err, [&](HTTP2Connection* c) {
                c->conn_cb = cb;
                c->conn_user = user;
                return true;
            });
        }

        bool HTTP2::write_frame(frame::Frame& frame, ErrCode& errs) {
            return check_opt(opt, err, [&](HTTP2Connection* c) {
                if (stream::is_connection_level(frame.type) ||
                    (frame.type == frame::FrameType::window_update && frame.id == 0)) {
                    return enque_frame(c, c->conn.state.s0, frame, errs);
                }
                if (frame.id == 0) {
                    errs.err = http2_invalid_id;
                    errs.h2err = H2Error::protocol;
                    errs.debug = "Frame.id == 0 for stream related frame";
                    return false;
                }
                insert_stream(c, frame.id);
                auto kv = c->streams.find(frame.id);
                if (kv == c->streams.end()) {
                    enque_conn_error(c, {
                                            .err = http2_resource,
                                            .h2err = H2Error::internal,
                                            .debug = "stream resource allocation failure",
                                        });
                    return false;
                }
                return enque_frame(c, kv->second.state, frame, errs);
            });
        }

        bool HTTP2::write_goaway(int code, const char* debug) {
            return check_opt(opt, err, [&](HTTP2Connection* c) {
                enque_conn_error(c, {
                                        .err = http2_user_error,
                                        .h2err = H2Error(code),
                                        .debug = debug,
                                    });
                return true;
            });
        }

        struct tmp_head {
            using strpair = std::pair<flex_storage, flex_storage>;
            strpair pair;
            internal::data_set set;
            bool is_end() const {
                return set.is_end(set.iter, set.cmp);
            }

            void do_load() {
                if (is_end()) {
                    return;
                }
                pair.first.resize(0);
                pair.second.resize(0);
                auto cb = [](void* c, char t) {
                    auto s = static_cast<flex_storage*>(c);
                    s->push_back(t);
                };
                set.load(set.iter, true, &pair.first, cb);
                set.load(set.iter, false, &pair.second, cb);
            }

            void increment() {
                set.incr(set.iter);
            }
        };

        struct tmp_iter {
            using deference_type = std::ptrdiff_t;
            tmp_head* h = nullptr;
            bool operator==(const tmp_iter&) const {
                return h->is_end();
            }

            tmp_iter& operator++() {
                h->increment();
                h->do_load();
                return *this;
            }

            tmp_head::strpair& operator*() {
                return h->pair;
            }
        };

        struct TmpHeader {
            tmp_head h;

            TmpHeader(internal::data_set s) {
                h.set = s;
                h.do_load();
            }
            tmp_iter begin() {
                return tmp_iter{&h};
            }
            tmp_iter end() {
                return tmp_iter();
            }
        };

        bool HTTP2Stream::write_hpack_header(internal::data_set data,
                                             std::uint8_t* padding, frame::Priority* prio, bool end_stream,
                                             ErrCode& errc) {
            auto c = check_opt(opt, errc.err);
            if (!c) {
                return false;
            }
            auto kv = c->streams.find(id_);
            if (kv == c->streams.end()) {
                return false;
            }
            flex_storage buf;
            TmpHeader tmph{data};
            c->hpkerr = hpack::encode(buf, c->conn.send.table, tmph, c->conn.state.send.settings.header_table_size, true);
            if (c->hpkerr != hpack::HpackError::none) {
                enque_conn_error(c, {
                                        .err = http2_hpack_error,
                                        .h2err = H2Error::compression,
                                        .debug = "HPACK encode failure",
                                    });
                return false;
            }
            frame::HeaderFrame head{};
            head.id = id_;
            if (prio) {
                head.flag |= frame::Flag::priority;
                head.priority = *prio;
            }
            if (padding) {
                head.flag |= frame::Flag::padded;
                head.padding = *padding;
            }
            if (end_stream) {
                head.flag |= frame::Flag::end_stream;
            }
            const auto frame_size = c->conn.state.recv.settings.max_frame_size;
            size_t progress = 0;
            auto increment_progress = [&]() {
                if (buf.size() - progress <= frame_size) {
                    head.data = buf;
                    progress = buf.size();
                }
                else {
                    head.data = buf.rvec().substr(progress, frame_size);
                    progress += frame_size;
                }
            };
            increment_progress();
            if (progress == buf.size()) {
                head.flag |= frame::Flag::end_headers;
            }
            if (!enque_frame(c, kv->second.state, head, errc)) {
                return false;
            }
            while (progress != buf.size()) {
                // now enter danger zone
                // failure of this section causes connection error
                frame::ContinuationFrame fr{};
                fr.id = id_;
                increment_progress();
                if (progress == buf.size()) {
                    fr.flag |= frame::Flag::end_headers;
                }
                if (!enque_frame(c, kv->second.state, fr, errc)) {
                    enque_conn_error(c, errc);
                    return false;
                }
            }
            return true;
        }

        bool HTTP2Stream::write_data(const char* data, size_t len, size_t* sent, bool end_stream, unsigned char* padding, ErrCode* err) {
            ErrCode code;
            auto c = check_opt(opt, code.err);
            if (!c) {
                if (err) {
                    *err = code;
                }
                return false;
            }
            auto kv = c->streams.find(id_);
            if (kv == c->streams.end()) {
                return false;
            }
            frame::DataFrame f{};
            auto send_size = stream::get_sendable_size(c->conn.state.s0.send, kv->second.state.send);
            if (send_size < len) {
                if (!sent) {
                    return false;
                }
                len = send_size;
            }
            f.id = id_;
            if (padding) {
                f.flag |= frame::Flag::padded;
            }
            if (end_stream) {
                f.flag |= frame::Flag::end_stream;
            }
            f.data = data;
            if (!enque_frame(c, kv->second.state, f, code)) {
                if (err) {
                    *err = code;
                }
                return false;
            }
            return true;
        }

        bool HTTP2Stream::get_header_impl(void (*add)(void* user, const char* key, size_t keysize, const char* value, size_t valuesize), void* user) {
            ErrCode code;
            auto c = check_opt(opt, code.err);
            auto kv = c->streams.find(id_);
            if (kv == c->streams.end()) {
                return false;
            }
            if (kv->second.header.buf.size()) {
                return false;
            }
            for (auto& field : kv->second.header.buf) {
                add(user, field.first.as_char(), field.first.size(),
                    field.second.as_char(), field.second.size());
            }
            return true;
        }

        fnet_dll_export(HTTP2) create_http2(bool server, h2set::PredefinedSettings settings) {
            auto c = new_from_global_heap<HTTP2Connection>(DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(HTTP2Connection), alignof(HTTP2Connection)));
            if (!c) {
                return HTTP2(nullptr);
            }
            c->conn.state.is_server = server;
            c->conn.state.send.settings = settings;
            c->preface_ok = !server;
            // stream 0 state represents connection state
            // flow is
            // State.idle -> State.open -> State.closed
            state(c) = stream::State::idle;
            return HTTP2(c);
        }
    }  // namespace fnet::http2
}  // namespace utils
