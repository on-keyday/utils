/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <helper/expected.h>
#include <helper/expected_op.h>

// some code are copied from https://cpprefjp.github.io/reference/expected/expected.html and it's linked pages
// which is under CC Attribution 3.0 Unported
// and code are changed to use in helper
// for example, adding std:: prefix to use std functions
// MIT license is maybe compatible to CC

#include <iomanip>
#include <iostream>
#include <string>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <charconv>
#include <numeric>

namespace exp = utils::helper::either;

// 整数除算
exp::expected<int, std::string> idiv(int a, int b) {
    if (b == 0) {
        return exp::unexpected{"divide by zero"};
    }
    if (a % b != 0) {
        return exp::unexpected{"out of domain"};
    }
    return a / b;
}

void dump_result(const exp::expected<int, std::string>& v) {
    if (v) {
        std::cout << *v << std::endl;
    }
    else {
        std::cout << std::quoted(v.error()) << std::endl;
    }
}

void test_1() {
    dump_result(idiv(10, 2));
    dump_result(idiv(10, 3));
    dump_result(idiv(10, 0));
}

// std::pair型から2要素std::tuple型へはコピー変換可能
using IntPair = std::pair<int, int>;
using IntTuple = std::tuple<int, int>;

// std::unique_ptr型からstd::shared_ptr型へはムーブ変換可能
using UniquePtr = std::unique_ptr<int>;
using SharedPtr = std::shared_ptr<int>;

// 引数リスト または 初期化子リスト＋引数リスト から構築可能な型
struct ComplexType {
    std::string data;
    std::vector<int> seq;

    ComplexType(const char* ptr, size_t len)
        : data(ptr, len) {}
    ComplexType(std::initializer_list<int> il, std::string_view sv)
        : data(sv), seq(il) {}
};

void test_2() {
    // (1) デフォルトコンストラクタ
    {
        exp::expected<int, std::string> x;
        assert(x.has_value());
        assert(x.value() == 0);
        // int型の値初期化{}は値0
    }

    // (2) コピーコンストラクタ
    {
        exp::expected<int, std::string> srcV = 42;
        exp::expected<int, std::string> dstV = srcV;
        assert(srcV.has_value() && dstV.has_value());
        assert(srcV.value() == 42 && dstV.value() == 42);

        exp::expected<int, std::string> srcE = exp::unexpected{"Oops"};
        exp::expected<int, std::string> dstE = srcE;
        assert(!srcE.has_value() && !dstE.has_value());
        assert(srcE.error() == "Oops" && dstE.error() == "Oops");
    }

    // (3) ムーブコンストラクタ
    {
        exp::expected<std::string, int> srcV = "ok";
        exp::expected<std::string, int> dstV = std::move(srcV);
        assert(srcV.has_value() && dstV.has_value());
        assert(dstV.value() == "ok");
        // srcV.value()はstd::stringムーブ後の未規定の値

        exp::expected<int, std::string> srcE = exp::unexpected{"ng"};
        exp::expected<int, std::string> dstE = std::move(srcE);
        assert(!srcE.has_value() && !dstE.has_value());
        assert(dstE.error() == "ng");
        // srcE.error()はstd::stringムーブ後の未規定の値
    }

    // (4) 変換コピー構築
    {
        exp::expected<IntPair, int> src = IntPair{1, 2};
        exp::expected<IntTuple, int> dst = src;
        assert(src.has_value() && dst.has_value());
        assert((dst.value() == IntTuple{1, 2}));
    }

    // (5) 変換ムーブ構築
    {
        exp::expected<UniquePtr, int> src = std::make_unique<int>(42);
        exp::expected<SharedPtr, int> dst = std::move(src);
        assert(src.has_value() && dst.has_value());
        assert(*dst.value() == 42);
        assert(src.value() == nullptr);
        // ムーブ後のstd::unique_ptr型はnullptrが保証される
    }

    // (6) 正常値の変換コピー／ムーブ構築
    {
        IntPair src1{1, 2};
        exp::expected<IntTuple, int> dst1 = src1;
        assert(dst1.has_value());
        assert((dst1.value() == IntTuple{1, 2}));

        UniquePtr src2 = std::make_unique<int>(42);
        exp::expected<SharedPtr, int> dst2 = std::move(src2);
        assert(dst2.has_value());
        assert(*dst2.value() == 42);
    }

    // (7),(8) エラー値の変換コピー／ムーブ構築
    {
        exp::unexpected<IntPair> src1{std::in_place, 1, 2};
        exp::expected<int, IntTuple> dst1 = src1;
        assert(not dst1.has_value());
        assert((dst1.error() == IntTuple{1, 2}));

        UniquePtr src2 = std::make_unique<int>(42);
        exp::expected<int, SharedPtr> dst2 = exp::unexpected{std::move(src2)};
        assert(not dst2.has_value());
        assert(*dst2.error() == 42);
    }

    // (9),(10) 引数リストから正常値を直接構築
    {
        exp::expected<ComplexType, int> x1{std::in_place, "C++", 1};
        assert(x1.has_value());
        assert(x1.value().data == "C");
        // "C++"より長さ1の文字列が構築されている

        exp::expected<ComplexType, int> x2{std::in_place, {5, 6, 7, 8}, "Steps"};
        assert(x2.has_value());
        assert(x2.value().data == "Steps");
        assert((x2.value().seq == std::vector<int>{5, 6, 7, 8}));
    }

    // (11),(12) 引数リストからエラー値を直接構築
    {
        exp::expected<int, ComplexType> x1{exp::unexpect, "Hello!", 4};
        assert(not x1.has_value());
        assert(x1.error().data == "Hell");
        // "Hello!"より長さ4の文字列が構築されている

        exp::expected<int, ComplexType> x2{exp::unexpect, {1, 2, 3}, "Lotus"};
        assert(not x2.has_value());
        assert(x2.error().data == "Lotus");
        assert((x2.error().seq == std::vector<int>{1, 2, 3}));
    }
}

void test_3() {
    // (1) デフォルトコンストラクタ
    {
        exp::expected<void, int> x;
        assert(x.has_value());
    }

    // (2) コピーコンストラクタ
    {
        exp::expected<void, int> srcV;
        exp::expected<void, int> dstV = srcV;
        assert(srcV.has_value() && dstV.has_value());

        exp::expected<void, int> srcE = exp::unexpected{42};
        exp::expected<void, int> dstE = srcE;
        assert(!srcE.has_value() && !dstE.has_value());
        assert(srcE.error() == 42 && dstE.error() == 42);
    }

    // (3) ムーブコンストラクタ
    {
        exp::expected<void, std::string> srcV;
        exp::expected<void, std::string> dstV = std::move(srcV);
        assert(srcV.has_value() && dstV.has_value());

        exp::expected<void, std::string> srcE = exp::unexpected{"Oops"};
        exp::expected<void, std::string> dstE = std::move(srcE);
        assert(!srcE.has_value() && !dstE.has_value());
        assert(dstE.error() == "Oops");
        // srcE.error()はstd::stringムーブ後の未規定の値
    }

    // (4) 変換コピー構築
    {
        exp::expected<void, IntPair> src = exp::unexpected{IntPair{1, 2}};
        exp::expected<void, IntTuple> dst = src;
        assert(!src.has_value() && !dst.has_value());
        assert((dst.error() == IntTuple{1, 2}));
    }

    // (5) 変換ムーブ構築
    {
        exp::expected<void, UniquePtr> src = exp::unexpected{std::make_unique<int>(42)};
        exp::expected<void, SharedPtr> dst = std::move(src);
        assert(!src.has_value() && !dst.has_value());
        assert(*dst.error() == 42);
        assert(src.error() == nullptr);
        // ムーブ後のstd::unique_ptr型はnullptrが保証される
    }

    // (6),(7) エラー値の変換コピー／ムーブ構築
    {
        exp::unexpected<IntPair> src1{std::in_place, 1, 2};
        exp::expected<void, IntTuple> dst1 = src1;
        assert(not dst1.has_value());
        assert((dst1.error() == IntTuple{1, 2}));

        UniquePtr src2 = std::make_unique<int>(42);
        exp::expected<void, SharedPtr> dst2 = exp::unexpected{std::move(src2)};
        assert(not dst2.has_value());
        assert(*dst2.error() == 42);
    }

    // (8) 正常値(void)を直接構築
    {
        exp::expected<void, int> x1{std::in_place};
        assert(x1.has_value());
    }

    // (9),(10) 引数リストからエラー値を直接構築
    {
        exp::expected<void, ComplexType> x1{exp::unexpect, "C++", 1};
        assert(not x1.has_value());
        assert(x1.error().data == "C");
        // "C++"より長さ1の文字列が構築されている

        exp::expected<void, ComplexType> x2{exp::unexpect, {11, 14, 17, 20, 23}, "C++"};
        assert(not x2.has_value());
        assert(x2.error().data == "C++");
        assert((x2.error().seq == std::vector<int>{11, 14, 17, 20, 23}));
    }
}

void test_4() {
    {
        exp::expected<int, std::string> srcV = 42;
        exp::expected<int, std::string> dstV;
        dstV = srcV;
        assert(srcV.has_value() && dstV.has_value());
        assert(srcV.value() == 42 && dstV.value() == 42);

        exp::expected<int, std::string> srcE = exp::unexpected{"Oops"};
        exp::expected<int, std::string> dstE;
        dstE = srcE;
        assert(!srcE.has_value() && !dstE.has_value());
        assert(srcE.error() == "Oops" && dstE.error() == "Oops");
    }

    // (2) ムーブ代入
    {
        exp::expected<std::string, int> srcV = "ok";
        exp::expected<std::string, int> dstV;
        dstV = std::move(srcV);
        assert(srcV.has_value() && dstV.has_value());
        assert(dstV.value() == "ok");
        // srcV.value()はstd::stringムーブ後の未規定の値

        exp::expected<int, std::string> srcE = exp::unexpected{"ng"};
        exp::expected<int, std::string> dstE;
        dstE = std::move(srcE);
        assert(!srcE.has_value() && !dstE.has_value());
        assert(dstE.error() == "ng");
        // srcE.error()はstd::stringムーブ後の未規定の値
    }

    // (3) 正常値の変換コピー代入
    {
        IntPair src = IntPair{1, 2};
        exp::expected<IntTuple, int> dst;
        dst = src;
        assert(dst.has_value());
        assert((dst.value() == IntTuple{1, 2}));
    }
    // (3) 正常値の変換ムーブ代入
    {
        UniquePtr src = std::make_unique<int>(42);
        exp::expected<SharedPtr, int> dst;
        dst = std::move(src);
        assert(dst.has_value());
        assert(*dst.value() == 42);
    }

    // (4) エラー値の変換コピー代入
    {
        exp::unexpected<IntPair> src{IntPair{1, 2}};
        exp::expected<int, IntTuple> dst;
        dst = src;
        assert(not dst.has_value());
        assert((dst.error() == IntTuple{1, 2}));
    }

    // (5) エラー値の変換ムーブ代入
    {
        exp::unexpected<UniquePtr> src{std::make_unique<int>(42)};
        exp::expected<int, SharedPtr> dst;
        dst = std::move(src);
        assert(not dst.has_value());
        assert(*dst.error() == 42);
    }
}

void test_5() {
    // (1) コピー代入
    {
        exp::expected<void, int> srcV;
        exp::expected<void, int> dstV;
        dstV = srcV;
        assert(srcV.has_value() && dstV.has_value());

        exp::expected<void, int> srcE = exp::unexpected{42};
        exp::expected<void, int> dstE;
        dstE = srcE;
        assert(!srcE.has_value() && !dstE.has_value());
        assert(srcE.error() == 42 && dstE.error() == 42);
    }

    // (2) ムーブ代入
    {
        exp::expected<void, std::string> srcV;
        exp::expected<void, std::string> dstV;
        dstV = std::move(srcV);
        assert(srcV.has_value() && dstV.has_value());

        exp::expected<void, std::string> srcE = exp::unexpected{"Oops"};
        exp::expected<void, std::string> dstE;
        dstE = std::move(srcE);
        assert(!srcE.has_value() && !dstE.has_value());
        assert(dstE.error() == "Oops");
        // srcE.error()はstd::stringムーブ後の未規定の値
    }

    // (3) エラー値の変換コピー代入
    {
        exp::unexpected<IntPair> src{IntPair{1, 2}};
        exp::expected<void, IntTuple> dst;
        dst = src;
        assert(not dst.has_value());
        assert((dst.error() == IntTuple{1, 2}));
    }

    // (4) エラー値の変換ムーブ代入
    {
        exp::unexpected<UniquePtr> src{std::make_unique<int>(42)};
        exp::expected<void, SharedPtr> dst;
        dst = std::move(src);
        assert(not dst.has_value());
        assert(*dst.error() == 42);
    }
}

void test_6() {
    exp::expected<int, std::string> x = 42;
    exp::expected<int, std::string> y = exp::unexpected{"ERR"};
    assert(x.value() == 42 && y.error() == "ERR");

    x.swap(y);
    assert(x.error() == "ERR" && y.value() == 42);
}

void test_7() {
    exp::expected<void, int> x;
    exp::expected<void, int> y = exp::unexpected{42};
    assert(x.has_value() && y.error() == 42);

    x.swap(y);
    assert(x.error() == 42 && y.has_value());
}

void test_8() {
    exp::expected<int, std::string> x = 42;
    std::cout << x.value_or(0) << std::endl;

    exp::expected<int, std::string> y = exp::unexpected{"ERR"};
    std::cout << y.value_or(0) << std::endl;
}

void test_9() {
    exp::expected<void, std::string> x;
    std::cout << x.error_or("-") << std::endl;

    exp::expected<void, std::string> y = exp::unexpected{"ERR"};
    std::cout << y.error_or("-") << std::endl;
}

void test_10() {
    exp::expected<int, std::string> x = 42;
    std::cout << x.error_or("-") << std::endl;

    exp::expected<int, std::string> y = exp::unexpected{"ERR"};
    std::cout << y.error_or("-") << std::endl;
}

// 正数なら2倍／それ以外はエラー値を返す関数
exp::expected<int, std::string> twice(int n) {
    if (0 < n) {
        return n * 2;
    }
    else {
        return exp::unexpected{"out of domain"};
    }
}

void test_11() {
    exp::expected<int, std::string> v1 = 1;
    assert(v1.and_then(twice).value() == 2);

    exp::expected<int, std::string> v2 = 0;
    assert(v2.and_then(twice).error() == "out of domain");

    exp::expected<int, std::string> e1 = exp::unexpected{"NaN"};
    assert(e1.and_then(twice).error() == "NaN");
}

exp::expected<void, std::string> ok() {
    return {};
}

exp::expected<void, std::string> ng() {
    return exp::unexpected{"ng"};
}

void test_12() {
    exp::expected<void, std::string> v1;
    assert(v1.and_then(ok).has_value());

    exp::expected<void, std::string> v2;
    assert(v2.and_then(ng).error() == "ng");

    exp::expected<void, std::string> e1 = exp::unexpected{"empty"};
    assert(e1.and_then(ng).error() == "empty");
}

exp::expected<void, std::string> validate(int code) {
    if (0 <= code) {
        return {};
    }
    else {
        return exp::unexpected{"bad code"};
    }
}

void test_13() {
    exp::expected<void, int> v1;
    assert(v1.or_else(validate).has_value());

    exp::expected<void, int> e1 = exp::unexpected{42};
    assert(e1.or_else(validate).has_value());

    exp::expected<void, int> e2 = exp::unexpected{-100};
    assert(e2.or_else(validate).error() == "bad code");
}

exp::expected<int, std::string> parse(std::string_view s) {
    int val{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec == std::errc{} && ptr == s.data() + s.size()) {
        return val;
    }
    else {
        return exp::unexpected<std::string>{s};
    }
}

void test_14() {
    exp::expected<int, std::string> v1 = 1;
    assert(v1.or_else(parse).value() == 1);

    exp::expected<int, std::string> e1 = exp::unexpected{"123"};
    assert(e1.or_else(parse) == 123);

    exp::expected<int, std::string> e2 = exp::unexpected{"bad"};
    assert(e2.or_else(parse).error() == "bad");
}

void test_15() {
    exp::expected<long, long> x1 = 1;
    exp::expected<short, short> y1 = 1;
    exp::expected<long, long> x2 = exp::unexpected{1};
    exp::expected<short, short> y2 = exp::unexpected{1};

    // (1)
    assert(x1 == y1);
    assert(x2 == y2);
    assert(not(x1 == y2));
    assert(not(x2 == y1));

    // (2), (3)
    assert(x1 == 1);
    assert(1 == x1);

    // (4), (5)
    assert(x2 == exp::unexpected{1});
    assert(exp::unexpected{1} == x2);
}

void test_16() {
    exp::expected<long, long> x1 = 1;
    exp::expected<short, short> y1 = 100;
    exp::expected<long, long> x2 = exp::unexpected{1};
    exp::expected<short, short> y2 = exp::unexpected{100};
    // (1)
    assert(x1 != y1);
    assert(x2 != y2);
    assert(x1 != y2);
    assert(x2 != y1);

    // (2), (3)
    assert(x1 != 2);
    assert(2 != x1);

    // (4), (5)
    assert(x2 != exp::unexpected{2});
    assert(exp::unexpected{2} != x2);
}

void test_17() {
    exp::expected<void, long> x1;
    exp::expected<void, short> y1;
    exp::expected<void, long> x2 = exp::unexpected{1};
    exp::expected<void, short> y2 = exp::unexpected{1};

    // (1)
    assert(x1 == y1);
    assert(x2 == y2);
    assert(not(x1 == y2));
    assert(not(x2 == y1));

    // (2), (3)
    assert(x2 == exp::unexpected{1});
    assert(exp::unexpected{1} == x2);
}

void test_18() {
    exp::expected<void, long> x1;
    exp::expected<void, short> y1;
    exp::expected<void, long> x2 = exp::unexpected{1};
    exp::expected<void, short> y2 = exp::unexpected{100};

    // (1)
    assert(not(x1 != y1));
    assert(x2 != y2);
    assert(x1 != y2);
    assert(x2 != y1);

    // (2), (3)
    assert(x2 != exp::unexpected{2});
    assert(exp::unexpected{2} != x2);
}

// 1..N数列を生成する関数
std::vector<int> make_seq(int n) {
    std::vector<int> seq(n, 0);
    std::iota(seq.begin(), seq.end(), 1);
    return seq;
}

void test_19() {
    exp::expected<int, std::string> v1 = 3;
    assert((v1.transform(make_seq).value() == std::vector<int>{1, 2, 3}));

    exp::expected<int, std::string> e1 = exp::unexpected{"NaN"};
    assert(e1.transform(make_seq).error() == "NaN");
}

int get_answer() {
    return 42;
}

void test_20() {
    exp::expected<void, std::string> v1;
    assert(v1.transform(get_answer).value() == 42);

    exp::expected<void, std::string> e1 = exp::unexpected{"galaxy"};
    assert(e1.transform(get_answer).error() == "galaxy");
}

std::string revstr(std::string str) {
    std::reverse(str.begin(), str.end());
    return str;
}

void test_21() {
    exp::expected<int, std::string> v1 = 42;
    assert(v1.transform_error(revstr).value() == 42);

    exp::expected<int, std::string> e1 = exp::unexpected{"Oops"};
    assert(e1.transform_error(revstr).error() == "spoO");
}

std::string int2str(int n) {
    return "(" + std::to_string(n) + ")";
}

void test_22() {
    exp::expected<void, int> v1;
    assert(v1.transform_error(int2str).has_value());

    exp::expected<void, int> e1 = exp::unexpected{42};
    assert(e1.transform_error(int2str).error() == "(42)");
}

void test_23() {
    exp::expected<void, int> v1;
    auto val = v1 & get_answer | int2str | revstr;
}

int main() {
    test_1();
    test_2();
    test_3();
    test_4();
    test_5();
    test_6();
    test_7();
    test_9();
    test_10();
    test_11();
    test_12();
    test_13();
    test_14();
    test_15();
    test_16();
    test_17();
    test_18();
    test_19();
    test_20();
    test_21();
    test_22();
}
