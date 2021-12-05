/*license*/

#include "../include/operator/utf_convert.h"

#include <string>

void test_utf_operator() {
    std::u8string str;
    utils::ops::from(U"Hello World! こんにちは世界") >> str;
    assert(str == u8"Hello World! こんにちは世界" && "operator is broken");
}

int main() {
    test_utf_operator();
}
