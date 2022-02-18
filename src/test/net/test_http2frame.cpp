/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <net/http2/frame.h>

struct Input {
    std::string str;
    size_t pos = 0;
    template <class T>
    bool read(T& t, size_t len = sizeof(T)) {
        if (len > sizeof(T)) {
            return false;
        }
        constexpr size_t sz = sizeof(T);
        char buf[sz]{0};
        for (size_t i = sz - len; i < sz; i++) {
            if (str.size() <= pos) {
                return false;
            }
            buf[sz - i - 1] = str[pos];
            pos++;
        }
        T* c = reinterpret_cast<T*>(buf);
        t = *c;
        return true;
    }

    bool read(utils::net::Dummy&) {
        return true;
    }

    bool read(std::string& s, size_t len) {
        s.reserve(len);
        for (size_t i = 0; i < len; i++) {
            if (str.size() <= pos) {
                return false;
            }
            s.push_back(str[pos]);
            pos++;
        }
        return true;
    }
};

void test_http2frame() {
    using namespace utils;
    wrap::shared_ptr<net::http2::H2Frame> frame;
    Input input;
    char v[] = {0x00, 0x00, 0x06, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,
                0, 0, 0, 0, 0, 0};
    input.str = std::string(v, sizeof(v));
    net::http2::decode(input, frame);
}

int main() {
    test_http2frame();
}
