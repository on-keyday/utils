#include <binary/float.h>
#include <cfenv>

void nan() {
    constexpr utils::binary::SingleFloat f1 = 2143289344u;               // qNAN
    constexpr utils::binary::SingleFloat f2 = 0x80000000 | 2143289344u;  // -qNAN
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
    constexpr utils::binary::DoubleFloat dl = 9221120237041090560;
    constexpr auto v = dl.biased_exponent();
    constexpr auto v2 = utils::binary::DoubleFloat{0}.biased_exponent();
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
    utils::binary::SingleFloat fsNAN = sNAN, fqNAN = qNAN;
    fsNAN.set_fraction(0);
    auto n = fsNAN.to_float();
    n = -n;
    fsNAN = n / n;
    auto val = fsNAN.is_indeterminate_nan();
    fsNAN.set_fraction(1);
    fsNAN.set_fraction(fsNAN.frac_msb | 1);
    fsNAN.set_sign(false);
    fsNAN.set_fraction(fsNAN.frac_msb);
    fsNAN.set_sign(true);
    fsNAN = n * 0.0f;
    auto nan = utils::binary::make_float(NAN);
    fsNAN = (-(float)(((float)(1e+300 * 1e+300)) * 0.0F));
    constexpr auto f03 = utils::binary::make_float(0.2);
    constexpr auto nanf = utils::binary::make_float(__builtin_nanf(""));
    constexpr auto nanl = utils::binary::make_float(__builtin_nan(""));
}
int main() {
    nan();
    return 0;
}