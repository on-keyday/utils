/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <binary/float.h>
#include <cfenv>
#include <cfloat>

void nan() {
    constexpr futils::binary::SingleFloat f1 = 2143289344u;               // qNAN
    constexpr futils::binary::SingleFloat f2 = 0x80000000 | 2143289344u;  // -qNAN
    constexpr auto mask = f1.exponent_mask;
    constexpr bool is_nan = f1.is_nan();
    constexpr bool is_quiet_nan = f1.is_quiet_nan();
    constexpr bool is_nan2 = f2.is_nan();
    constexpr bool sign = f1.sign();
    constexpr bool sign2 = f2.sign();
    constexpr bool test1 = f2.exponent() == f2.exponent_max;
    constexpr auto frac_value = f1.fraction();
    constexpr auto cmp = f1 == f2;
    constexpr auto cmp2 = f1.to_float() == f2.to_float();
    constexpr auto cmp3 = f1.to_float() == f1.to_float();
    constexpr auto cmp4 = f1.to_int() == f1.to_int();
    constexpr futils::binary::DoubleFloat dl = 9221120237041090560;
    constexpr auto v = dl.biased_exponent();
    constexpr auto v2 = futils::binary::DoubleFloat{0}.biased_exponent();
    constexpr auto v3 = f1.is_canonical_nan();
    constexpr auto v4 = f1.is_arithmetic_nan();
    constexpr auto v5 = f2.is_denormalized();
    constexpr auto v6 = f2.is_normalized();
    constexpr auto f3 = [&] {
        auto copy = f1;
        copy.make_quiet_signaling();
        return copy;
    }();
    fexcept_t pre, post1, post2;
    std::fegetexceptflag(&pre, FE_ALL_EXCEPT);
    auto sNAN = f3.to_float() + f3.to_float();
    std::fegetexceptflag(&post1, FE_ALL_EXCEPT);
    std::fesetexceptflag(&pre, FE_ALL_EXCEPT);
    auto qNAN = f1.to_float() + f1.to_float();
    std::fegetexceptflag(&post2, FE_ALL_EXCEPT);
    futils::binary::SingleFloat fsNAN = sNAN, fqNAN = qNAN;
    fsNAN.fraction(0);
    auto n = fsNAN.to_float();
    n = -n;
    fsNAN = n / n;
    auto val = fsNAN.is_indeterminate_nan();
    fsNAN.fraction(1);
    fsNAN.fraction(fsNAN.fraction_msb | 1);
    fsNAN.sign(false);
    fsNAN.fraction(fsNAN.fraction_msb);
    fsNAN.sign(true);
    fsNAN = n * 0.0f;
    auto nan = futils::binary::make_float(NAN);
    fsNAN = (-(float)(((float)(1e+300 * 1e+300)) * 0.0F));
    constexpr auto f03 = futils::binary::make_float(0.2);
    constexpr auto nanf = futils::binary::make_float(__builtin_nanf(""));
    constexpr auto nanl = futils::binary::make_float(__builtin_nan(""));
    constexpr auto nan_with_sign = [&] {
        auto copy = nanf;
        copy.sign(true);
        return copy;
    }();
    constexpr auto yes_nan = nanf.to_float();
    constexpr auto yes_nan2 = nan_with_sign.to_float();
    constexpr auto reconvert_nan = futils::binary::make_float(yes_nan);
    constexpr auto reconvert_nan2 = futils::binary::make_float(yes_nan2);
    constexpr auto epsilon = futils::binary::make_float(FLT_EPSILON);
    constexpr auto epsilon_exponent = epsilon.exponent();
    constexpr auto epsilon_fraction = epsilon.fraction();
    constexpr auto epsilon_biased_exponent = epsilon.biased_exponent();
    constexpr auto epsilon_plus_1 = futils::binary::make_float(1.0f + FLT_EPSILON);
    constexpr auto epsilon_plus_1_exponent = epsilon_plus_1.exponent();
    constexpr auto epsilon_plus_1_fraction = epsilon_plus_1.fraction();
    constexpr auto epsilon_plus_1_biased_exponent = epsilon_plus_1.biased_exponent();
    constexpr auto epsilon2 = futils::binary::make_float(epsilon_plus_1.to_float() - 1.0f);
    static_assert(epsilon2 == epsilon, "epsilon2 != epsilon");
    constexpr auto frac_with_1 = epsilon_plus_1.fraction_with_implicit_1();
    constexpr auto frac_with_1_epsilon = epsilon.fraction_with_implicit_1();

    static_assert(futils::binary::epsilon<float> == epsilon, "ep != epsilon");
    static_assert(futils::binary::quiet_nan<float> == nanf, "qn != nanf");
    static_assert(futils::binary::indeterminate_nan<float> == nan_with_sign, "in != nan_with_sign");
    static_assert(futils::binary::infinity<float> == futils::binary::make_float(INFINITY), "inf != inf");
    constexpr auto vas = futils::binary::epsilon<double>.to_float();
    constexpr auto vas2 = futils::binary::make_float(vas + 1.0);
    constexpr auto vas3 = vas2.to_float();
    constexpr auto vas4 = vas3 - 1.0;
    constexpr auto no_effect = vas - vas / 2.0000000000000;
    constexpr auto effected = 1.0 + no_effect;
    static_assert(effected == 1.0, "effected != 1.0");
    constexpr auto normalized_min = futils::binary::min_normalized<double>.to_float();
    constexpr auto denormalized_min = futils::binary::min_denormalized<double>.to_float();
    static_assert(normalized_min == DBL_MIN, "min_normalized != DBL_MIN");
}
int main() {
    nan();
    return 0;
}