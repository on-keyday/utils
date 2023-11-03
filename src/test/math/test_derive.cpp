/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <math/derive.h>
#include <typeinfo>
#include <wrap/cout.h>

int main() {
    constexpr auto c1 = utils::math::pow(utils::math::sin(utils::math::x), utils::math::c<2>) +
                        utils::math::pow(utils::math::cos(utils::math::x), utils::math::c<2>);
    utils::binary::DoubleFloat res = c1(10);
    auto derived = c1.derive();
    res = derived(10);
    auto f = utils::math::pow(utils::math::x, utils::math::c<4>);
    auto s = utils::math::sin(utils::math::x) + utils::math::c<32>;
    auto sin = s.derive().derive().derive().derive();
    auto fd = (f.derive().derive() * sin / f);
    auto fdd = fd.derive();

    res = sin(utils::math::pi.raw().to_float());
    std::string val;
    namespace m = utils::math;
    fd.print(val);
    val.push_back(',');
    fdd.print(val);
    val.push_back(',');
    auto fddd = fdd.derive();
    fddd.print(val);
    auto fdddd = fddd.derive();
    val.push_back(',');
    fdddd.print(val);
    auto y = utils::math::x / utils::math::sin(utils::math::x);
    auto fddddd = utils::math::replace_x(fdddd, y);
    val.push_back(',');
    fddddd.print(val);
    auto d = fddddd.derive();
    val.push_back(',');
    d.print(val);
    auto dd = d.derive();
    val.push_back(',');
    dd.print(val);
    constexpr auto size = sizeof(dd);
    utils::wrap::cout_wrap() << "x," << val << "\n";
    for (auto i = -5.0; i <= 5.0; i += 0.01) {
        utils::wrap::cout_wrap() << i << "," << fd(i) << "," << fdd(i) << "," << fddd(i) << "," << fdddd(i) << "," << fddddd(i) << "," << d(i) << "," << dd(i) << "\n";
    }
}