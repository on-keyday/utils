/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <utf/cast.h>
#include <wrap/lite/string.h>

void test_utfcast() {
    using namespace utils;
    utf::cast_fn<wrap::string>("invoke", [](auto& v) {
        static_assert(std::is_same_v<wrap::string, std::remove_reference_t<decltype(v)>>, "expect type but not");
    });
}

int main() {
    test_utfcast();
}