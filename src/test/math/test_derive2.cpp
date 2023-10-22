/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <math/derive.h>
#include <string>
#include <wrap/cout.h>

int main() {
    namespace m = utils::math;
    constexpr auto z = m::log(m::e, pow(m::x, m::c<2>()) + m::sin(m::x * m::y - m::x * m::c<32>()));
    std::string v;
    z.print(v, m::XYZWPrint{});
    utils::wrap::cout_wrap() << v << "\n";

    v.clear();
    constexpr auto zd1 = z.derive<0>();
    constexpr auto zd2 = z.derive<1>();

    zd1.print(v);
    utils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zd2.print(v);
    utils::wrap::cout_wrap() << v << "\n";
    constexpr auto zdd1 = zd2.derive<0>();
    constexpr auto zdd2 = zd1.derive<1>();

    v.clear();
    zdd1.print(v);
    utils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zdd2.print(v);
    utils::wrap::cout_wrap() << v << "\n";

    constexpr auto zddd1 = zdd2.derive<0>();
    constexpr auto zddd2 = zdd1.derive<1>();

    v.clear();
    zddd1.print(v);
    utils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zddd2.print(v);
    utils::wrap::cout_wrap() << v << "\n";

    auto zd3 = zd1 + zd2;

    v.clear();
    zd3.print(v);
    utils::wrap::cout_wrap() << v << "\n";
}
