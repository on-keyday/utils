/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/frame/encode.h>
#include <quic/frame/decode.h>
#include <quic/core/core.h>

using namespace utils::quic;

struct Callback {
    frame::types prev;
    frame::Error frame(frame::Frame* f) {
        prev = f->type;
        frame::if_<frame::Ack>(f, [](frame::Ack* a) {
            assert(a->ack_range_count == 1);
            assert(a->ack_ranges[0].gap == 3392);
            assert(a->ack_ranges[0].ack_len == 21);
        });
        return frame::Error::none;
    }
    void frame_error(frame::Error err, const char* where, frame::Frame* red = nullptr) {
        assert(false);
    }
    void varint_error(varint::Error err, const char* where, frame::Frame* red = nullptr) {
        assert(false);
    }
    core::QUIC* q;

    allocate::Alloc* get_alloc() {
        return core::get_alloc(q);
    }

    bytes::Bytes get_bytes(tsize s) {
        return core::get_bytes(q, s);
    }
    void put_bytes(bytes::Bytes& b) {
        return core::put_bytes(q, std::move(b));
    }
};

void test_quic_frame() {
    auto q = core::new_QUIC(allocate::stdalloc());
    core::set_bpool(q, bytes::stdbpool(q));
    frame::Buffer b{};
    Callback cb;
    cb.q = q;
    b.a = core::get_alloc(q);
    frame::Ack ack{frame::types::ACK};
    ack.ack_range_count = 1;
    ack.ack_ranges = {b.a};
    append(ack.ack_ranges, frame::AckRange{3392, 21});
    frame::write_ack(b, ack, cb);

    frame::ResetStream reset{frame::types::RESET_STREAM};
    frame::write_reset_stream(b, reset, cb);

    frame::StopSending stop{frame::types::STOP_SENDING};
    frame::write_stop_sending(b, stop, cb);
    tsize offset = 0;
    frame::read_frame(b.b.c_str(), b.len, &offset, cb);
    assert(cb.prev == frame::types::ACK);
    frame::read_frame(b.b.c_str(), b.len, &offset, cb);
    assert(cb.prev == frame::types::RESET_STREAM);
    frame::read_frame(b.b.c_str(), b.len, &offset, cb);
    assert(cb.prev == frame::types::STOP_SENDING);
    core::put_bytes(q, std::move(b.b));
}

int main() {
    test_quic_frame();
}
