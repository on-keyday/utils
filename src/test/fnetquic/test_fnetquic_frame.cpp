/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/quic/frame/wire.h>
#include <vector>
#include <testutil/alloc_hook.h>
#include <fnet/quic/frame/dynframe.h>
#include <fnet/heap.h>
#include <fnet/quic/frame/parse.h>
#include <fnet/quic/stream/core/stream_base.h>
#include <fnet/quic/stream/impl/stream.h>
#include <fnet/debug.h>

void make_and_parse(auto&& st) {
    futils::fnet::quic::frame::DynFramePtr dyn = makeDynFrame(std::move(st));
    futils::byte data[100]{};
    futils::binary::writer w{data};
    dyn->render(w);
    futils::binary::reader r{w.written()};
    st.parse(r);
}

int main() {
    using namespace futils::fnet::quic::frame;
    auto allocs = futils::fnet::debug::allocs();
    futils::fnet::set_normal_allocs(allocs);
    futils::test::set_alloc_hook(true);
    StreamFrame st;
    st.stream_data = "Hello World! but this is ...";
    make_and_parse(st);
    DataBlockedFrame data;
    make_and_parse(data);
    auto val = test::check_stream_range();
    val = futils::fnet::quic::stream::core::test::check_stream_send();
}
