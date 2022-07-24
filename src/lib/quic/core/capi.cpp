/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <quic/common/dll_cpp.h>
#include <quic/capi/capi.h>
#include <quic/core/core.h>
#include <quic/frame/cast.h>
using namespace utils::quic;

dll(QUIC*) default_QUIC(NOARG) {
    return reinterpret_cast<::QUIC*>(core::default_QUIC());
}

dll(QUICFrameType) get_frame_type(QUICFrame* frame) {
    if (!frame) {
        return QUICFrameType::PADDING;
    }
    auto f = reinterpret_cast<frame::Frame*>(frame);
    return (QUICFrameType)f->type;
}

dll(const byte*) get_frame_data(QUICFrame* frame, const char* param, size_t* len) {
    if (!frame || !param || !len) {
        return nullptr;
    }
    auto f = reinterpret_cast<frame::Frame*>(frame);
    using namespace frame;
    auto p = [&](auto&& cb) {
        if_(f, cb);
    };
    const byte* b = nullptr;
    p([&](Crypto* a) {
        b = a->crypto_data.c_str();
    });
    p([&](Stream* s) {
        b = s->stream_data.c_str();
    });
    // TODO(on-keyday): write more!
    return b;
}
