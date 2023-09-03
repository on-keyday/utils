/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <reflect/layout.h>

REFLECT_LAYOUT_BUILDER(Frame)
REFLECT_LAYOUT_FIELD("type", std::uint8_t)
REFLECT_LAYOUT_FIELD("len", std::uint64_t)
REFLECT_LAYOUT_FIELD("", std::uint32_t)
REFLECT_LAYOUT_BUILDER_END()

int main() {
    utils::reflect::test::check_name();
    utils::reflect::Layout<Frame, 1> layout;
    constexpr auto name = layout.nameof<0>();
    auto val = layout.get<name>();
    auto print = [](auto name, auto val) {

    };
    layout.apply([&](auto&& name, auto mem, auto val) {
        print(name, *mem);
    },
                 true);
}
