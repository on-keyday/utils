/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <math/derive.h>
#include <string>
#include <wrap/cout.h>

int main() {
    namespace m = futils::math;
    constexpr auto z = m::log(m::c_e, pow(m::x, m::c<2>) + m::sin(m::x * m::y - m::x * m::c<32>));
    std::string v;
    z.print(v, m::XYZWPrint{});
    futils::wrap::cout_wrap() << v << "\n";

    v.clear();
    constexpr auto zd1 = z.derive<0>();
    constexpr auto zd2 = z.derive<1>();

    zd1.print(v);
    futils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zd2.print(v);
    futils::wrap::cout_wrap() << v << "\n";
    constexpr auto zdd1 = zd2.derive<0>();
    constexpr auto zdd2 = zd1.derive<1>();

    v.clear();
    zdd1.print(v);
    futils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zdd2.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    constexpr auto zddd1 = zdd2.derive<0>();
    constexpr auto zddd2 = zdd1.derive<1>();

    v.clear();
    zddd1.print(v);
    futils::wrap::cout_wrap() << v << "\n";
    v.clear();
    zddd2.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto zd3 = zd1 + zd2;

    v.clear();
    zd3.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto zd4 = zd1 * zd2;

    v.clear();
    zd4.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto z5 = zd4.derive<0>();

    v.clear();
    z5.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto z6 = m::log(m::c_e, z5 / zd4);

    v.clear();
    z6.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto z7 = z6.derive<0>();

    v.clear();
    z7.print(v);
    futils::wrap::cout_wrap() << v << "\n";

    v.clear();
    auto vz = m::pow(m::x, m::c<2>) * m::pow(m::y * m::x, m::c<2>) / m::y;
    vz.derive<0>().print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto z2 = m::sin(m::pow(m::c<2>, m::x + m::y));
    v.clear();
    z2.derive<0>().print(v);
    futils::wrap::cout_wrap() << v << "\n";

    auto z3 = m::pow(m::sin(m::x + m::y), m::c<2>) + m::pow(m::cos(m::x + m::y), m::c<2>);
    v.clear();
    z3.derive<0>().derive<0>().derive<0>().derive<0>().print(v);
    futils::wrap::cout_wrap() << v << "\n";
}
