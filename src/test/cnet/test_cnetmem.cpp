/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/mem.h>
#include <cstdlib>
#include <number/array.h>

void test_cnet_mem_buffer() {
    using namespace utils::cnet;
    using namespace utils::number;
    auto buf = mem::new_buffer(::malloc, ::realloc, ::free);
    Array<40, char> arr;
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
    auto mem1 = mem::create_mem();
    auto mem2 = mem::create_mem();
    auto make = []() { return mem::new_buffer(::malloc, ::realloc, ::free); };
    mem::set_buffer(mem1, make());
    mem::set_buffer(mem2, make());
    mem::set_link(mem1, mem::get_link(mem2));
    mem::set_link(mem2, mem::get_link(mem1));
}

int main() {
    test_cnet_mem_buffer();
}
