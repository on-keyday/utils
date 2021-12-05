/*license*/

#include "../include/endian/endian.h"
#include <cassert>

void test_endian() {
    utils::endian::Endian native = utils::endian::native();
    std::uint32_t test = 0x00000001;
    std::uint32_t expected = 0x01000000;
    if (native == utils::endian::Endian::big) {
        test = utils::endian::to_little(&test);
    }
    else {
        test = utils::endian::to_big(&test);
    }
    assert(test == expected && "endian convert is incorrect");
}

int main() {
    test_endian();
}
