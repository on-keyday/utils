/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/http2/h2frame.h>
#include <dnet/http2/http2.h>
#include <dnet/dll/httpbuf.h>
#include <helper/pushbacker.h>
#include <view/charvec.h>
#include <view/char.h>

namespace utils {
    namespace dnet {
        using Seq = Sequencer<view::CharVec<const char>>;
        struct ArgPack {
            const char* text;
            size_t size;
            size_t& red;
            int& err;
            h2frame::H2Error& h2err;
            bool& strm;
            const char*& debug;
        };

        std::uint32_t packi4(int offset, const char* head) {
            return head[offset + 0] << 24 | head[offset + 1] << 16 | head[offset + 2] << 8 | head[offset + 3];
        }

        static bool check_padding(int& offset, Seq& seq, ArgPack& p, h2frame::Frame& frame, std::uint8_t& padding) {
            if (frame.flag & h2frame::Flag::padded) {
                if (frame.len == 0) {
                    p.err = http2_invalid_data_length;
                    p.h2err = h2frame::H2Error::frame_size;
                    p.debug = "padding expected but Frame.len == 0";
                    return false;
                }
                padding = seq.current();
                if (padding >= frame.len) {
                    p.err = http2_invalid_padding_length;
                    p.h2err = h2frame::H2Error::protocol;
                    p.debug = "padding value is equal to or larger than Frame.len";
                    return false;
                }
                offset += 1;
            }
            return true;
        }

        static bool read_priority(int& offset, h2frame::Frame& frame, ArgPack& p, const char* head, h2frame::Priority& prio) {
            if (frame.len - offset < 5) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "priority field expected but remaining Frame.len is smaller than 5";
                return false;
            }
            prio.depend = packi4(offset, head);
            prio.weight = head[offset + 4];
            offset += 5;
            return true;
        }

        static bool parse_data_frame(Seq& seq, ArgPack& p, h2frame::DataFrame& data) {
            auto head = seq.rptr + p.text;
            int offset = 0;
            if (!check_padding(offset, seq, p, data, data.padding)) {
                return false;
            }
            data.data = head + offset;
            if (offset == 1) {
                data.pad = head + data.len - data.padding;
                data.datalen = data.len - data.padding - 1;
            }
            else {
                data.datalen = data.len;
            }
            if (data.id == 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id == 0 at DataFrame";
                return false;
            }
            return true;
        }

        static bool parse_header_frame(Seq& seq, ArgPack& p, h2frame::HeaderFrame& header) {
            auto head = seq.rptr + p.text;
            int offset = 0;
            if (!check_padding(offset, seq, p, header, header.padding)) {
                return false;
            }
            if (header.flag & h2frame::Flag::priority) {
                if (!read_priority(offset, header, p, head, header.priority)) {
                    return false;
                }
            }
            header.data = head + offset;
            if (header.flag & h2frame::Flag::padded) {
                header.pad = head + header.len - header.padding;
                header.datalen = header.len - header.padding - offset;
            }
            else {
                header.datalen = header.len - offset;
            }
            if (header.id == 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id == 0 at HeaderFrame";
                return false;
            }
            return true;
        }

        static bool parse_priority_frame(Seq& seq, ArgPack& p, h2frame::PriorityFrame& priority) {
            auto head = seq.rptr + p.text;
            int offset = 0;
            if (!read_priority(offset, priority, p, head, priority.priority)) {
                p.strm = true;
                return false;
            }
            if (priority.id == 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id == 0 at PriorityFrame";
                return false;
            }
            return true;
        }

        static bool parse_rst_stream_frame(Seq& seq, ArgPack& p, h2frame::RstStreamFrame& rst) {
            auto head = seq.rptr + p.text;
            if (rst.len != 4) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "Frame.len != 4 at RstStreamFrame";
                return false;
            }
            rst.code = packi4(0, head);
            if (rst.id == 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id == 0 at RstStreamFrame";
                return false;
            }
            return true;
        }

        static bool parse_settings_frame(Seq& seq, ArgPack& p, h2frame::SettingsFrame& settings) {
            auto head = seq.rptr + p.text;
            if (settings.flag & h2frame::Flag::ack) {
                if (settings.len != 0) {
                    p.err = http2_invalid_data_length;
                    p.h2err = h2frame::H2Error::frame_size;
                    p.debug = "Frame.len != 0 at SettingsFrame with Flag.ack";
                    return false;
                }
            }
            else {
                settings.settings = head;
                if (settings.len % 6) {
                    p.err = http2_invalid_data_length;
                    p.h2err = h2frame::H2Error::frame_size;
                    p.debug = "Frame.len is not multiple of 6 at SettingsFrame";
                    return false;
                }
            }
            if (settings.id != 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id != 0 at SettingsFrame";
                return false;
            }
            return true;
        }

        static bool parse_push_promise_frame(Seq& seq, ArgPack& p, h2frame::PushPromiseFrame& header) {
            auto head = seq.rptr + p.text;
            int offset = 0;
            if (!check_padding(offset, seq, p, header, header.padding)) {
                return false;
            }
            if (header.len < 4 + offset) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "Frame.len remaining is smaller than 4 at PushPromiseFrame";
                return false;
            }
            header.promise = packi4(offset, head);
            offset += 4;
            header.data = head + offset;
            if (header.flag & h2frame::Flag::padded) {
                header.pad = head + header.len - header.padding;
            }
            if (header.id == 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id == 0 at PushPromiseFrame";
                return false;
            }
            return true;
        }

        static bool parse_ping_frame(Seq& seq, ArgPack& p, h2frame::PingFrame& ping) {
            auto head = seq.rptr + p.text;
            if (ping.len != 8) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "Frame.len != 8 at PingFrame";
                return false;
            }
            auto heads = [&](int i) {
                return std::uint64_t(head[i]);
            };
            ping.opaque = heads(0) << 56 | heads(1) << 48 | heads(2) << 40 | heads(3) << 32 |
                          heads(4) << 24 | heads(5) << 16 | heads(6) << 8 | heads(7);
            if (ping.id != 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id != 0 at PingFrame";
                return false;
            }
            return true;
        }

        static bool parse_goaway_frame(Seq& seq, ArgPack& p, h2frame::GoAwayFrame& goaway) {
            auto head = seq.rptr + p.text;
            if (goaway.len < 8) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "Frame.len is smaller than 8 at GoAwayFrame";
                return false;
            }
            goaway.processed_id = packi4(0, head);
            goaway.code = packi4(4, head);
            if (goaway.len > 8) {
                goaway.debug_data = head + 8;
                goaway.dbglen = goaway.len - 8;
            }
            if (goaway.id != 0) {
                p.err = http2_invalid_id;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "Frame.id != 0 at GoAwayFrame";
                return false;
            }
            return true;
        }

        static bool parse_window_update_frame(Seq& seq, ArgPack& p, h2frame::WindowUpdateFrame& winup) {
            auto head = seq.rptr + p.text;
            if (winup.len != 4) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "Frame.len != 4 at WindowUpdateFrame";
                return false;
            }
            winup.increment = packi4(0, head);
            return true;
        }

        static bool parse_continuous_frame(Seq& seq, ArgPack& p, h2frame::ContinuationFrame& cont) {
            auto head = seq.rptr + p.text;
            cont.data = head;
            return true;
        }

        struct UnknownFrame : h2frame::Frame {
            UnknownFrame(h2frame::FrameType type)
                : h2frame::Frame(type) {}
        };

        dnet_dll_implement(bool) pass_frame(const char* text, size_t size, size_t& red) {
            if (!text) {
                return false;
            }
            auto seq = make_cpy_seq(view::CharVec(text, size));
            if (seq.remain() < 9) {
                return false;
            }
            const char* head = text + seq.rptr;
            int len = head[0] << 16 | head[1] << 8 | head[2];
            seq.consume(9);
            if (len < seq.remain()) {
                return false;
            }
            seq.consume(len);
            red = seq.rptr;
            return true;
        }

        dnet_dll_implement(bool) parse_frame(const char* text, size_t size, size_t& red, ErrCode& err, H2Callback cb, void* c) {
            if (!cb || !text) {
                err.err = http2_invalid_argument;
                return false;
            }
            auto seq = make_cpy_seq(view::CharVec(text, size));
            ArgPack pack{
                text,
                size,
                red,
                err.err,
                err.h2err,
                err.strm,
                err.debug,
            };
            auto set_debug = [&](const char* msg) {
                err.debug = msg;
            };
            while (true) {
                err.err = 0;
                pack.debug = nullptr;
                if (seq.remain() < 9) {
                    err.err = http2_need_more_data;
                    set_debug("least 9 byte required");
                    return true;  // nothing to do
                }
                const char* head = text + seq.rptr;
                int len = head[0] << 16 | head[1] << 8 | head[2];
                auto type = h2frame::FrameType(head[3]);
                auto flag = h2frame::Flag(head[4]);
                int id = head[5] << 24 | head[6] << 16 | head[7] << 8 | head[8];
                seq.consume(9);
                if (len > seq.remain()) {
                    err.err = http2_need_more_data;
                    set_debug("need more length");
                    return true;  // nothing to do
                }
                auto set = [&](auto& frame) {
                    frame.len = len;
                    frame.flag = flag;
                    frame.id = id;
                };
                auto call = [&](auto& frame) -> bool {
                    return cb(c, &frame, err);
                };
#define MAP(TYPE, STRUCT, PARSE)        \
    if (type == TYPE) {                 \
        STRUCT frame;                   \
        set(frame);                     \
        if (!PARSE(seq, pack, frame)) { \
            call(frame);                \
            return false;               \
        }                               \
        if (!call(frame)) {             \
            seq.consume(len);           \
            red = seq.rptr;             \
            return true;                \
        }                               \
    }                                   \
    else

                MAP(h2frame::FrameType::data, h2frame::DataFrame, parse_data_frame)
                MAP(h2frame::FrameType::header, h2frame::HeaderFrame, parse_header_frame)
                MAP(h2frame::FrameType::priority, h2frame::PriorityFrame, parse_priority_frame)
                MAP(h2frame::FrameType::rst_stream, h2frame::RstStreamFrame, parse_rst_stream_frame)
                MAP(h2frame::FrameType::settings, h2frame::SettingsFrame, parse_settings_frame)
                MAP(h2frame::FrameType::push_promise, h2frame::PushPromiseFrame, parse_push_promise_frame)
                MAP(h2frame::FrameType::ping, h2frame::PingFrame, parse_ping_frame)
                MAP(h2frame::FrameType::goaway, h2frame::GoAwayFrame, parse_goaway_frame)
                MAP(h2frame::FrameType::window_update, h2frame::WindowUpdateFrame, parse_window_update_frame)
                MAP(h2frame::FrameType::continuous, h2frame::ContinuationFrame, parse_continuous_frame) {
                    UnknownFrame frame{type};
                    set(frame);
                    err.err = http2_unknown_type;
                    call(frame);
                }
#undef MAP
                seq.consume(len);
                red = seq.rptr;
            }
        }

        using CVec = helper::CharVecPushbacker<char>;

        static void spliti4(CVec& vec, std::uint32_t value, bool three = false) {
            if (!three) {
                vec.push_back((value >> 24) & 0xff);
            }
            vec.push_back((value >> 16) & 0xff);
            vec.push_back((value >> 8) & 0xff);
            vec.push_back((value)&0xff);
        }

        struct RPack {
            int& err;
            h2frame::H2Error& h2err;
            bool& strm;  // stream level error
            const char*& debug;
            bool auto_correct;
            char* head;
            size_t limit = 0xffffff;
        };

        constexpr auto head_flag = 4;
        constexpr auto head_len = 0;
        constexpr auto head_id = 5;

        static bool correct_len(RPack& p, size_t len) {
            if (len > p.limit) {
                p.err = http2_invalid_data_length;
                p.h2err = h2frame::H2Error::frame_size;
                p.debug = "expected Frame.len is larger than SETTING_MAX_FRAME_SIZE";
                return false;
            }
            p.head[head_len + 0] = (len >> 16) & 0xff;
            p.head[head_len + 1] = (len >> 8) & 0xff;
            p.head[head_len + 2] = (len)&0xff;
            return true;
        }

        static bool check_id(RPack& p, bool expect_0, h2frame::Frame& f) {
            if (expect_0) {
                if (f.id != 0) {
                    if (p.auto_correct) {
                        p.head[head_id + 0] = 0;
                        p.head[head_id + 1] = 0;
                        p.head[head_id + 2] = 0;
                        p.head[head_id + 3] = 0;
                        f.id = 0;
                    }
                    else {
                        p.err = http2_invalid_id;
                        p.h2err = h2frame::H2Error::protocol;
                        p.debug = "Frame.id != 0 detected on connection level frame";
                        return false;
                    }
                }
            }
            else {
                if (f.id == 0) {
                    p.err = http2_invalid_id;
                    p.h2err = h2frame::H2Error::protocol;
                    p.debug = "Frame.id == 0 detected on stream related frame";
                    return false;
                }
            }
            return true;
        }

        template <class Frame>
        static bool check_padding(int& offset, CVec& vec, RPack& p, Frame& f) {
            if (f.flag & h2frame::Flag::padded) {
                if (f.len == 0 || f.padding >= f.len) {
                    if (p.auto_correct) {
                        p.head[head_flag] &= ~h2frame::Flag::padded;
                        f.padding = 0;
                        f.flag &= ~h2frame::Flag::padded;
                        if (!correct_len(p, f.datalen)) {
                            return false;
                        }
                    }
                    else {
                        p.err = http2_invalid_data_length;
                        p.h2err = h2frame::H2Error::protocol;
                        p.debug = "padding expected but Frame.len==0 or padding value is larger than Frame.len";
                        return false;
                    }
                }
                else {
                    vec.push_back(f.padding);
                    offset += 1;
                }
            }
            return true;
        }

        static bool check_length(h2frame::Frame& f, RPack& p, size_t expect) {
            if (f.len != expect) {
                if (p.auto_correct) {
                    if (!correct_len(p, expect)) {
                        return false;
                    }
                    f.len = expect;
                }
                else {
                    p.err = http2_length_consistency;
                    p.h2err = h2frame::H2Error::internal;
                    p.debug = "invalid internal data length consitancy";
                    return false;
                }
            }
            return true;
        }

        static void render_padding(CVec& vec, auto& f) {
            if (f.padding) {
                if (f.pad) {
                    helper::append(vec, view::CharVec(f.pad, f.padding));
                }
                else {
                    helper::append(vec, view::CharView(0, f.padding));
                }
            }
        }

        static void render_priority(CVec& vec, h2frame::Priority& prio) {
            spliti4(vec, prio.depend);
            vec.push_back(prio.weight);
        }

        static bool render_data_frame(CVec& vec, RPack& p, h2frame::DataFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            int offset = 0;
            if (!check_padding(offset, vec, p, f)) {
                return false;
            }
            if (!check_length(f, p, f.datalen + f.padding + offset)) {
                return false;
            }
            if (!f.data && f.datalen) {
                p.err = http2_invalid_null_data;
                p.h2err = h2frame::H2Error::internal;
                p.debug = "expected data in DataFrame but nullptr is detected";
                return false;
            }
            helper::append(vec, view::CharVec(f.data, f.datalen));
            render_padding(vec, f);
            return true;
        }

        static bool render_header_frame(CVec& vec, RPack& p, h2frame::HeaderFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            int expsize = 0;
            if (!check_padding(expsize, vec, p, f)) {
                return false;
            }
            if (f.flag & h2frame::Flag::priority) {
                render_priority(vec, f.priority);
                expsize += 5;
            }
            if (!check_length(f, p, f.datalen + f.padding + expsize)) {
                return false;
            }
            if (!f.data && f.datalen) {
                p.err = http2_invalid_null_data;
                p.h2err = h2frame::H2Error::internal;
                p.debug = "expected data in HeaderFrame but nullptr is detected";
                return false;
            }
            helper::append(vec, view::CharVec(f.data, f.datalen));
            render_padding(vec, f);
            return true;
        }

        static bool render_priority_frame(CVec& vec, RPack& p, h2frame::PriorityFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            if (!check_length(f, p, 5)) {
                p.strm = true;
                return false;
            }
            render_priority(vec, f.priority);
            return true;
        }

        static bool render_rst_stream_frame(CVec& vec, RPack& p, h2frame::RstStreamFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            if (!check_length(f, p, 4)) {
                return false;
            }
            spliti4(vec, f.code);
            return true;
        }

        static bool render_settings_frame(CVec& vec, RPack& p, h2frame::SettingsFrame& f) {
            if (!check_id(p, true, f)) {
                return false;
            }
            if (f.flag & h2frame::Flag::ack) {
                if (!check_length(f, p, 0)) {
                    return false;
                }
            }
            else {
                if (f.len % 6) {
                    p.err = http2_invalid_data_length;
                    p.h2err = h2frame::H2Error::frame_size;
                    p.debug = "Frame.len is multiple of 6 at SettingsFrame";
                    return false;
                }
                if (!f.settings && f.len) {
                    p.err = http2_invalid_null_data;
                    p.h2err = h2frame::H2Error::frame_size;
                    p.debug = "expected data in SettingsFrame but nullptr is detected";
                    return false;
                }
                helper::append(vec, view::CharVec(f.settings, f.len));
            }
            return true;
        }

        static bool render_push_promise_frame(CVec& vec, RPack& p, h2frame::PushPromiseFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            int offset = 0;
            if (!check_padding(offset, vec, p, f)) {
                return false;
            }
            if (!check_length(f, p, offset + 4 + f.datalen + f.padding)) {
                return false;
            }
            spliti4(vec, f.promise);
            if (!f.data && f.datalen) {
                p.err = http2_invalid_null_data;
                p.h2err = h2frame::H2Error::internal;
                p.debug = "expected data in PushPromiseFrame but nullptr is detected";
                return false;
            }
            helper::append(vec, view::CharVec(f.data, f.datalen));
            render_padding(vec, f);
            return true;
        }

        static bool render_ping_frame(CVec& vec, RPack& p, h2frame::PingFrame& f) {
            if (!check_id(p, true, f)) {
                return false;
            }
            if (!check_length(f, p, 8)) {
                return false;
            }
            constexpr auto mask = 0xffffffff;
            auto high = std::uint32_t(f.opaque >> 32 & mask);
            auto low = std::uint32_t(f.opaque & mask);
            spliti4(vec, high);
            spliti4(vec, low);
            return true;
        }

        static bool render_goaway_frame(CVec& vec, RPack& p, h2frame::GoAwayFrame& f) {
            if (!check_id(p, true, f)) {
                return false;
            }
            if (!check_length(f, p, 8 + f.dbglen)) {
                return false;
            }
            spliti4(vec, f.processed_id);
            spliti4(vec, f.code);
            if (!f.debug_data && f.dbglen) {
                p.err = http2_invalid_null_data;
                p.h2err = h2frame::H2Error::internal;
                p.debug = "expected debug_data in GoAway but nullptr is detected";
                return false;
            }
            helper::append(vec, view::CharVec(f.debug_data, f.dbglen));
            return false;
        }

        static bool render_window_update_frame(CVec& vec, RPack& p, h2frame::WindowUpdateFrame& f) {
            if (!check_length(f, p, 4)) {
                return false;
            }
            if (f.increment <= 0) {
                p.err = http2_invalid_data;
                p.h2err = h2frame::H2Error::protocol;
                p.debug = "WindowUpdateFrame increment value is invalid";
                return false;
            }
            spliti4(vec, f.increment);
            return true;
        }

        static bool render_continuous_frame(CVec& vec, RPack& p, h2frame::ContinuationFrame& f) {
            if (!check_id(p, false, f)) {
                return false;
            }
            if (!f.data && f.len) {
                p.err = http2_invalid_null_data;
                p.h2err = h2frame::H2Error::internal;
                p.debug = "expected data in ContinuousFrame but nullptr is detected";
                return false;
            }
            helper::append(vec, view::CharVec(f.data, f.len));
            return true;
        }

        bool render_frame(char* text, size_t& size, size_t cap, ErrCode& err, h2frame::Frame* frame, bool auto_correct) {
            auto set_debug = [&](const char* msg) {
                err.debug = msg;
            };
            if (!text || !frame) {
                err.err = http2_invalid_argument;
                set_debug("text or frame argument is nullptr");
                return false;
            }
            helper::CharVecPushbacker vec(text, size, cap);
            RPack p{err.err, err.h2err, err.strm, err.debug, auto_correct, text + size};
            const size_t start = vec.size();
            spliti4(vec, frame->len, true);
            vec.push_back(std::uint8_t(frame->type));
            vec.push_back(std::uint8_t(frame->flag));
            spliti4(vec, frame->id, false);
            auto overflow = [&] {
                memset(text + start, 0, cap - start);
                err.err = http2_need_more_buffer;
                set_debug("buffer overflow occured");
                return false;
            };
            if (vec.overflow) {
                return overflow();
            }
#define MAP(TYPE, STRUCT, RENDER)                            \
    if (frame->type == TYPE) {                               \
        if (!RENDER(vec, p, *static_cast<STRUCT*>(frame))) { \
            set_debug(p.debug);                              \
            return false;                                    \
        }                                                    \
    }                                                        \
    else

            MAP(h2frame::FrameType::data, h2frame::DataFrame, render_data_frame)
            MAP(h2frame::FrameType::header, h2frame::HeaderFrame, render_header_frame)
            MAP(h2frame::FrameType::priority, h2frame::PriorityFrame, render_priority_frame)
            MAP(h2frame::FrameType::rst_stream, h2frame::RstStreamFrame, render_rst_stream_frame)
            MAP(h2frame::FrameType::settings, h2frame::SettingsFrame, render_settings_frame)
            MAP(h2frame::FrameType::push_promise, h2frame::PushPromiseFrame, render_push_promise_frame)
            MAP(h2frame::FrameType::ping, h2frame::PingFrame, render_ping_frame)
            MAP(h2frame::FrameType::goaway, h2frame::GoAwayFrame, render_goaway_frame)
            MAP(h2frame::FrameType::window_update, h2frame::WindowUpdateFrame, render_window_update_frame)
            MAP(h2frame::FrameType::continuous, h2frame::ContinuationFrame, render_continuous_frame) {
                memset(text + start, 0, cap - start);
                err.err = http2_unknown_type;
                set_debug("Frame.type is unknown");
                return false;
            }
            if (vec.overflow) {
                return overflow();
            }
            size = vec.size();
            return true;
        }
    }  // namespace dnet
}  // namespace utils
