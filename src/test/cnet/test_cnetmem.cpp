/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cnet/mem.h>
#include <cstdlib>
#include <number/array.h>

void test_cnet_mem() {
    using namespace utils::cnet;
    using namespace utils::number;
    auto buf = mem::new_buffer(::malloc, ::realloc, ::free);
    auto ok = mem::append(buf, "client hello");
    assert(ok == 12);
    Array<6, char> arr;
    arr.i = mem::remove(buf, arr.buf, arr.capacity());
    assert(arr.i == 6);
    ok = mem::append(buf, "client");
    assert(ok == 6);
    arr.i = mem::remove(buf, arr.buf, arr.capacity());
    mem::append(buf, "gaisikei card");
    Array<12, char> arr2;
    arr2.i = mem::remove(buf, arr2.buf, arr2.capacity());
    mem::append(buf, "gaisikei card");
    arr2.i = mem::remove(buf, arr2.buf, arr2.capacity());
    mem::append(buf, "gaisikei card");
    mem::append(buf, "ex card");
    mem::append(buf, "looping mem together");
    mem::remove(buf, arr2.buf, arr2.capacity());
}

int main() {
    test_cnet_mem();
}
