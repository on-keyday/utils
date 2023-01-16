/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/quic/frame/wire.h>
#include <vector>
#include <testutil/alloc_hook.h>
#include <dnet/quic/frame/dynframe.h>
#include <dnet/heap.h>
#include <dnet/quic/frame/parse.h>
#include <dnet/quic/stream/stream_base.h>
#include <dnet/quic/stream/impl/stream.h>
#include <dnet/quic/event/stream_event.h>
#include <dnet/debug.h>

void make_and_parse(auto&& st) {
    utils::dnet::quic::frame::DynFramePtr dyn = makeDynFrame(std::move(st));
    utils::byte data[100]{};
    utils::io::writer w{data};
    dyn->render(w);
    utils::io::reader r{w.written()};
    st.parse(r);
}

int main() {
    using namespace utils::dnet::quic::frame;
    auto allocs = utils::dnet::debug::allocs();
    utils::dnet::set_normal_allocs(allocs);
    utils::test::set_alloc_hook(true);
    StreamFrame st;
    st.stream_data = "Hello World! but this is ...";
    make_and_parse(st);
    DataBlockedFrame data;
    make_and_parse(data);
    auto val = test::check_stream_range();
    val = utils::dnet::quic::stream::test::check_stream_send();
}
