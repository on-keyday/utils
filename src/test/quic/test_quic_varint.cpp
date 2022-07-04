/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/quic_var_int.h>
#include <cassert>

void test_quic_varint() {
    using namespace utils::quic;
    byte s[32];
    tsize offset = 0;

    auto err = varint::encode(s, 0x32, 1, sizeof(s), &offset);

    assert(err == varint::Error::none);
    assert(s[0] == 0x32);
    assert(offset == 1);
    offset = 0;

    err = varint::encode(s, 0xffff, 2, sizeof(s), &offset);

    assert(err == varint::Error::too_large_number);
    assert(offset == 0);

    err = varint::encode(s, 0x9834723, 4, sizeof(s), &offset);

    assert(err == varint::Error::none);
    assert(offset == 4);
    tsize red, redsize;

    err = varint::decode(s, &red, &redsize, sizeof(s), 0);

    assert(err == varint::Error::none);
    assert(redsize == 4);
    assert(red == 0x9834723);
}

int main() {
    test_quic_varint();
}
