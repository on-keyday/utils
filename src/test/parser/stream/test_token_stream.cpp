/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <parser/stream/stream.h>
#include <vector>
#include <number/char_range.h>
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

    const char* peek(char* p, size_t size, size_t* red) {
        *red = remain();
        return raw;
    }

    bool seek(std::size_t pos) {
        pos_ = pos;
        return true;
    }

    size_t remain() {
        return ::strlen(raw) - pos_;
    }

    st::InputStat info() {
        return st::InputStat{.pos = pos_, .remain = remain()};
    }
};

struct MockStream {
    const char* tok;

    st::Token parse(st::Input& input) {
        if (!input.expect(tok)) {
            return st::simpleErrToken{"input.expect(tok) returns false"};
        }
        input.consume_if([](auto* v) { return utils::number::is_alnum(*v); });
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
