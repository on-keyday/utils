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
    utils::dnet::Allocs allocs;
    allocs.alloc_ptr = [](void*, size_t size, size_t, utils::dnet::DebugInfo*) {
        return malloc(size);
    };
    allocs.realloc_ptr = [](void*, void* p, size_t size, size_t, utils::dnet::DebugInfo*) {
        return realloc(p, size);
    };
    allocs.free_ptr = [](void*, void* p, utils::dnet::DebugInfo*) {
        return free(p);
    };
    allocs.ctx = nullptr;
    utils::dnet::set_normal_allocs(allocs);
    utils::test::set_alloc_hook(true);
    StreamFrame st;
    st.stream_data = "Hello World! but this is ...";
    make_and_parse(st);
    DataBlockedFrame data;
    make_and_parse(data);
}
