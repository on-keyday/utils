/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <binary/reader.h>
#include <binary/writer.h>

int main() {
    futils::binary::test::test_read_stream();
    futils::binary::test::test_write_stream();
}
