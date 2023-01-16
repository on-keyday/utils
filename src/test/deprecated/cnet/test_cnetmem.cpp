/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <deprecated/cnet/mem.h>
#include <cstdlib>
#include <number/array.h>
#include <helper/appender.h>
#include <helper/equal.h>
#include <thread>
#include <net_util/http/http_headers.h>
#include <string>
#include <map>

void test_cnet_mem_buffer() {
    using namespace utils::cnet;
    using namespace utils::number;
    auto buf = mem::new_buffer(::malloc, ::realloc, ::free);
    Array<char, 40> arr;
    mem::append(buf, "client hello");
    arr.i = mem::remove(buf, arr.buf, 12);
    assert(arr.i == 12);
    mem::append(buf, "client hello part 2");
    arr.i = mem::remove(buf, arr.buf, 19);
    assert(arr.i == 19);
    mem::append(buf, "client hello ultra super derax part 3");
    arr.i = mem::remove(buf, arr.buf, 37);
    assert(arr.i == 37);
    mem::delete_buffer(buf);
}

void test_cnet_mem_interface() {
    using namespace utils::cnet;
    using namespace utils::number;
    CNet *mem1, *mem2;
    mem::make_pair(mem1, mem2, ::malloc, ::realloc, ::free);
    Array<char, 50> arr{0};
    utils::helper::append(arr, "client hello");
    auto old = arr.i;
    write({}, mem1, arr.buf, arr.size(), &arr.i);
    read({}, mem2, arr.buf, arr.capacity(), &arr.i);
    assert(old == arr.i);
}

void test_thread_io() {
    using namespace utils::cnet;
    using namespace utils::number;
    using namespace utils::helper;
    // protocol
    // peer1 -> hello -> peer2
    // peer1 <- hello <- peer2
    // peer1 -> how are you? -> peer2
    // peer1 <- (respond emotion) <- peer2
    using Buf = Array<char, 100>;
    auto read_while = [](CNet *mem, Buf &arr) {
        arr.i = 0;
        while (arr.size() == 0) {
            read({}, mem, arr.buf, arr.capacity(), &arr.i);
        }
    };
    auto write_str = [](CNet *mem, auto str) {
        size_t s = 0;
        write({}, mem, str, ::strlen(str), &s);
    };
    auto client = [&](CNet *mem) {
        Buf arr{};
        write_str(mem, "hello");
        read_while(mem, arr);
        assert(equal(arr, "hello"));
        write_str(mem, "how are you?");
        read_while(mem, arr);
        assert(equal(arr, "I'm fine, thank you!"));
    };
    auto server = [&](CNet *mem) {
        Buf arr{};
        read_while(mem, arr);
        assert(equal(arr, "hello"));
        write_str(mem, "hello");
        read_while(mem, arr);
        assert(equal(arr, "how are you?"));
        write_str(mem, "I'm fine, thank you!");
    };
    CNet *cl, *sv;
    mem::make_pair(cl, sv, ::malloc, ::realloc, ::free);
    std::thread s_thread{server, sv}, c_thread{client, cl};
    s_thread.join();
    c_thread.join();
    delete_cnet(cl);
    delete_cnet(sv);
}

void test_http_protocol() {
    using namespace utils::cnet;
    using namespace utils::net::h1header;
    using namespace utils::number;
    auto reader = [](CNet *mem) {
        return [=](auto &seq, size_t expect, bool end) {
            Array<char, 1024> v{0};
            auto &buf = seq.buf.buffer;
            while (v.size() == 0) {
                read({}, mem, v.buf, v.capacity(), &v.i);
                if (end) {
                    break;
                }
            }
            utils::helper::append(buf, v);
            return true;
        };
    };
    auto server = [&](CNet *mem) {
        std::string buf, meth, path, body;
        std::map<std::string, std::string> h;
        auto r = reader(mem);
        read_request<std::string>(buf, meth, path, h, body, r);
        buf.clear();
        h.clear();
        h["Content-Length"] = "3";
        render_response(buf, 200, "OK", h);
        size_t s;
        write({}, mem, buf.c_str(), buf.size(), &s);
        write({}, mem, "OK!", 3, &s);
    };

    auto client = [&](CNet *mem) {
        std::string buf, body;
        std::map<std::string, std::string> h;
        auto r = reader(mem);
        StatusCode code;
        h["Host"] = "localhost";
        render_request(buf, "GET", "/", h);
        size_t s;
        write({}, mem, buf.c_str(), buf.size(), &s);
        buf.clear();
        h.clear();
        read_response<std::string>(buf, code, h, body, r);
    };

    CNet *cl, *sv;
    mem::make_pair(cl, sv, ::malloc, ::realloc, ::free);
    std::thread s_thread{server, sv}, c_thread{client, cl};
    s_thread.join();
    c_thread.join();
    delete_cnet(cl);
    delete_cnet(sv);
}

int main() {
    test_cnet_mem_buffer();
    test_cnet_mem_interface();
    test_thread_io();
    test_http_protocol();
}
