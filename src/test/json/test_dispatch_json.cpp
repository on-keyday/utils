/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <json/convert_json.h>
#include <json/json.h>
#include <map>
using namespace utils;
enum class TestE {
    test,
};
struct Test {
    std::string str;
    const char* c = nullptr;
    std::map<std::string, json::JSON> mp;
    std::vector<json::JSON> vec;
    std::vector<int> veci;
    std::map<std::string, int> mp2;
    TestE enu = TestE::test;
    bool to_json(auto& js) {
        json::internal::dispatch_to_json(str, js);
        json::internal::dispatch_to_json(c, js);
        json::internal::dispatch_to_json(mp, js);
        json::internal::dispatch_to_json(vec, js);
        json::internal::dispatch_to_json(veci, js);
        json::internal::dispatch_to_json(mp2, js);
        return true;
    }
};

bool to_json(const TestE& t, auto& js) {
    js = nullptr;
    return true;
}

void test_dispatch_json() {
    json::JSON js;
    Test t;
    json::internal::dispatch_to_json(&t, js);
}

int main() {
    test_dispatch_json();
}
