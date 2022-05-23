/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/stream/stream.h>
#include <vector>
namespace st = utils::parser::stream;

struct MockToken {
    const char* tok;
    void token(st::PB pb) {
        utils::helper::append(pb, tok);
    }
    st::Error err() {
        return {};
    }
};

struct MockInput {
    const char* raw;
    size_t pos_ = 0;

    size_t peek(char* p, size_t size) {
        size_t i = 0;
        for (; i < size; i++) {
            if (raw[pos_ + i] == 0) {
                break;
            }
            p[i] = raw[pos_ + i];
        }
        return i;
    }

    bool seek(std::size_t pos) {
        pos_ = pos;
        return true;
    }
};

struct MockStream {
    const char* tok;

    st::Token parse(st::Input& input) {
        if (!input.expect(tok)) {
            return st::simpleErrToken{"input.expect(tok) returns false"};
        }
        return MockToken{tok};
    }
};

void test_stream() {
    st::Stream st;
    st = MockStream{"expect token but not"};
    auto tok = st.parse(MockInput{"expect token but not"});
    assert(!has_err(tok));
    st = MockStream{"expectation is all right?"};
    tok = st.parse(MockInput{"expectation is all right?"});
    assert(!has_err(tok));
}

int main() {
    test_stream();
}
