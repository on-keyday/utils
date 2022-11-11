/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/http2/h2frame.h>
#include <dnet/http2/h2frame_c.h>
#include <new>

#define INIT(Struct)                                                    \
    dnet_dll_implement(void) init_##Struct(H2##Struct* ptr) {           \
        static_assert(                                                  \
            sizeof(utils::dnet::h2frame::Struct) == sizeof(H2##Struct), \
            "sizeof(H2" #Struct ") not equal to sizeof(" #Struct ")");  \
        auto p = reinterpret_cast<utils::dnet::h2frame::Struct*>(ptr);  \
        if (!p) {                                                       \
            return;                                                     \
        }                                                               \
        new (p) utils::dnet::h2frame::Struct{};                         \
    }

extern "C" {

INIT(Frame)
INIT(DataFrame)
INIT(HeaderFrame)
INIT(PriorityFrame)
INIT(RstStreamFrame)
INIT(SettingsFrame)
INIT(PushPromiseFrame)
INIT(PingFrame)
INIT(GoAwayFrame)
INIT(WindowUpdateFrame)
INIT(ContinuationFrame)

dnet_dll_export(int) pass_h2frame(const char* text, size_t size, size_t* red) {
    if (!red) {
        return false;
    }
    return utils::dnet::pass_frame(text, size, *red) ? 1 : 0;
}

dnet_dll_implement(int) parse_h2frame(const char* text, size_t size, size_t* red, H2ErrCode* err,
                                      H2ReadCallback cb, void* c) {
    if (!red || !err || !cb) {
        return false;
    }
    struct {
        H2ReadCallback cb;
        void* user;
    } wrap{cb, c};
    auto wcb = [](void* u, utils::dnet::h2frame::Frame* f, utils::dnet::ErrCode err) {
        auto c = static_cast<decltype(wrap)*>(u);
        H2ErrCode perr;
        perr.err = err.err;
        perr.h2err = int(err.h2err);
        perr.strm = err.strm;
        perr.debug = err.debug;
        return c->cb(c->user, reinterpret_cast<H2Frame*>(f), &perr) ? true : false;
    };
    utils::dnet::ErrCode nerr{};
    auto res = utils::dnet::parse_frame(text, size, *red, nerr, wcb, &wrap) ? 1 : 0;
    err->err = nerr.err;
    err->h2err = int(nerr.h2err);
    err->strm = nerr.strm;
    err->debug = nerr.debug;
    return res;
}

dnet_dll_implement(int) render_h2frame(char* text, size_t* size, size_t cap, H2ErrCode* err, H2Frame* frame, bool auto_correct) {
    if (!size || !err) {
        return false;
    }
    utils::dnet::ErrCode nerr{};
    auto res = utils::dnet::render_frame(
                   text, *size, cap, nerr,
                   reinterpret_cast<utils::dnet::h2frame::Frame*>(frame),
                   auto_correct)
                   ? 1
                   : 0;
    err->err = nerr.err;
    err->h2err = int(nerr.h2err);
    err->strm = nerr.strm;
    err->debug = nerr.debug;
    return res;
}
}
