/*license*/

#include "../include/endian/endian.h"

void test_endian() {
    utils::endian::Endian native = utils::endian::native();
    std::uint32_t test = 1;
}

int main() {
    test_endian();
}
