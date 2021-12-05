/*license*/

#include "../include/endian/endian.h"
#include "../include/endian/writer.h"
#include "../include/endian/reader.h"
#include <cassert>
#include <string>

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
    std::string buf;
    utils::endian::Writer<std::string&> w(buf);
    w.write_hton(1);
    assert(w.buf.size() == 4 && "endian writer is incorrect");
    assert(w.buf == std::string("\0\0\0\1", 4) && "endian writer is incorrect");
    w.buf.clear();
    w.write_hton<int, 4, 1>(1);
    utils::endian::Reader<std::string&> d(w.buf);
    int result = 0;
    d.read_ntoh<int, 4, 1>(result);
    assert(result == 1 && "endian reader is incorrect");
}

int main() {
    test_endian();
}
