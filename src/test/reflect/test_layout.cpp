/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <reflect/layout.h>

REFLECT_LAYOUT_BUILDER(FrameBuilder)
REFLECT_LAYOUT_FIELD(type, std::uint8_t)
REFLECT_LAYOUT_FIELD(z, std::uint32_t)
REFLECT_LAYOUT_FIELD(len, std::uint64_t)
REFLECT_LAYOUT_BUILDER_END()

struct Frame {
    std::uint8_t type;
    std::uint32_t z;
    std::uint64_t len;
};

int main() {
    futils::reflect::test::check_name();
    futils::reflect::Layout<FrameBuilder, 1> layout;
    constexpr auto name = layout.nameof<0>();
    constexpr auto offset = layout.offset<2>();
    constexpr auto size = layout.size;
    auto val = layout.get<name>();
    auto print = [](auto name, auto val) {

    };
    layout.apply([&](auto&& name, auto mem, auto val) {
        print(name, *mem);
    },
                 true);

    futils::reflect::Layout<FrameBuilder> compat;
    constexpr auto ofs = compat.offset<0>();
    constexpr auto size2 = compat.size;
    auto fr = compat.cast<Frame>();
    auto len = &fr->len;
}
