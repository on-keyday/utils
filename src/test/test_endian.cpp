/*license*/

#include "../include/endian/endian.h"

void test_endian() {
    constexpr utils::endian::Endian native = utils::endian::native();
}

int main() {
    test_endian();
}
