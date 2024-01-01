/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <math/derive.h>
#include <typeinfo>
#include <wrap/cout.h>

int main() {
    constexpr auto c1 = futils::math::pow(futils::math::sin(futils::math::x), futils::math::c<2>) +
                        futils::math::pow(futils::math::cos(futils::math::x), futils::math::c<2>);
    futils::binary::DoubleFloat res = c1(10);
    auto derived = c1.derive();
    res = derived(10);
    auto f = futils::math::pow(futils::math::x, futils::math::c<4>);
    auto s = futils::math::sin(futils::math::x) + futils::math::c<32>;
    auto sin = s.derive().derive().derive().derive();
    auto fd = (f.derive().derive() * sin / f);
    auto fdd = fd.derive();

    res = sin(futils::math::c_pi.raw().to_float());
    std::string val;
    namespace m = futils::math;
    fd.print(val);
    val.push_back(',');
    fdd.print(val);
    val.push_back(',');
    auto fddd = fdd.derive();
    fddd.print(val);
    auto fdddd = fddd.derive();
    val.push_back(',');
    fdddd.print(val);
    auto y = futils::math::x / futils::math::sin(futils::math::x);
    auto fddddd = futils::math::replace_x(fdddd, y);
    val.push_back(',');
    fddddd.print(val);
    auto d = fddddd.derive();
    val.push_back(',');
    d.print(val);
    auto dd = d.derive();
    val.push_back(',');
    dd.print(val);
    constexpr auto size = sizeof(dd);
    futils::wrap::cout_wrap() << "x," << val << "\n";
    for (auto i = -5.0; i <= 5.0; i += 0.01) {
        futils::wrap::cout_wrap() << i << "," << fd(i) << "," << fdd(i) << "," << fddd(i) << "," << fdddd(i) << "," << fddddd(i) << "," << d(i) << "," << dd(i) << "\n";
    }
}