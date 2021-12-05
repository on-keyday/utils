/*license*/

#include "../include/endian/endian.h"

void test_endian() {
    utils::endian::Endian native = utils::endian::native();
    std::uint32_t test = 1;
    utils::endian::to_big<std::uint32_t>(&test);
}

int main() {
    test_endian();
}
