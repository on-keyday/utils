/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <iface/base_iface.h>
#include <string>
#include <cassert>
#include <map>
#include <wrap/pair_iter.h>
#include <helper/appender.h>
#include <number/to_string.h>
#include <cnet/cnet.h>

struct Empty {
    size_t empty;
    size_t size() const {
        return empty;
    }
};

void test_base_iface() {
    using namespace utils::iface;
    String<Sized<Owns>> str1, mv;
    std::string ref;
    str1 = ref;
    ref = "game over";
    auto ptr = str1.c_str();

    mv = std::move(str1);
    Buffer<PushBacker<String<Ref>>> buf;
    buf = ref;

    buf.push_back('!');
    assert(ref == "game over!");

    Copy<PushBacker<String<Owns>>> copy;

    copy = ref;

    auto clone = copy.clone();

    clone.push_back('o');

    ptr = clone.c_str();

    assert(copy.c_str() != ptr);

    assert(ref == "game over!");

    Sized<Owns> owns = Empty{92};

    assert(owns.size() == 92);

    Subscript<Sized<Ref>> subscript = ref;

    assert(subscript[0] == 'g');

    Subscript<Ref, const char*, String<Ref>> maplike;
    std::map<std::string, std::string> map;
    maplike = map;
    map["key"] = "value";

    auto value = maplike["key"];
    assert(value.c_str() == std::string{"value"});

    ForwardIterator<Ref, char> it, end;
    auto baseit = ref.begin(), baseend = ref.end();
    it = baseit;
    end = baseend;

    assert(it != end);

    auto v = *it;

    assert(v == 'g');

    ForwardIterator2<Ref, char> it2{baseit, baseend}, end2{baseend, baseit};

    auto pair = std::make_pair(std::move(it2), std::move(end2));

    for (auto c : utils::wrap::iter(pair)) {
    }

    struct localError {
        int code;
        void error(PushBacker<Ref> r) {
            utils::helper::append(r, "errors:");
            utils::number::to_string(r, code);
        }
    };

    Error<Powns> err;

    err = localError{10};

    err.serror().c_str();

    auto cpy = std::move(err);

    utils::cnet::Error cneterr;

    cneterr = utils::cnet::wraperror{.err = localError{}};

    auto unwraped = cneterr.unwrap();
}

int main() {
    test_base_iface();
}
