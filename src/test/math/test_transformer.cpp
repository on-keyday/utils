#include <math/transformer/transformer.h>
#include <math/transformer/weight.h>
#include <random>
#include <string_view>
#include <vector>
#include <string>
#include <iostream>
#include "binary/float.h"
#include "math/vector.h"
#include "wrap/cout.h"
#include <wrap/trace.h>
#include <console/ansiesc.h>
bool nan_fired = false;
struct Config : futils::math::DefaultAllocation<futils::math::DynamicStatusDynamic> {
    template <size_t m, size_t n, class T, class C>
    static void observe(futils::math::ObservePoint p, futils::math::Matrix<m, n, T, C>& mat) {
        if (nan_fired) return;
        mat.for_each([&](size_t i, size_t j) {
            auto val = futils::binary::make_float(mat[i][j]);
            // std::cout << "Param: " << i << "," << v << ": " << mat[i][v] << " " << val.to_int() << "\n";
            if (val.is_nan() && !nan_fired) {
                nan_fired = true;
                std::vector<futils::wrap::stack_trace_entry> v;
                v.resize(20);
                auto trace = futils::wrap::get_stack_trace(v);
                std::cout << "NaN found: " << to_string(p) << " " << i << "," << j << "\n";
                for (auto& v : trace) {
                    std::string buf;
                    v.get_symbol(buf);
                    std::cout << buf << "\n";
                }
            }
        });
    }
    static constexpr auto df0 = futils::binary::DoubleFloat(1e-1);
    static constexpr auto df1 = futils::binary::DoubleFloat(1e-2);
    static constexpr auto df2 = futils::binary::DoubleFloat(1e-3);
    static constexpr auto df3 = futils::binary::DoubleFloat(1e-4);
    static constexpr auto df4 = futils::binary::DoubleFloat(1e-5);
    static constexpr auto df5 = futils::binary::DoubleFloat(1e-6);
    static constexpr auto df6 = futils::binary::DoubleFloat(1e-7);
    static constexpr auto df7 = futils::binary::DoubleFloat(1e-8);
    static constexpr auto df8 = futils::binary::DoubleFloat(1e-9);
    static constexpr auto df9 = futils::binary::DoubleFloat(1e-10);
    static constexpr auto df10 = futils::binary::DoubleFloat(1e-11);
    static constexpr auto df11 = futils::binary::DoubleFloat(1e-12);
    static constexpr auto df12 = futils::binary::DoubleFloat(1e-13);
    static constexpr auto df13 = futils::binary::DoubleFloat(1e-14);
    static constexpr auto df14 = futils::binary::DoubleFloat(1e-15);
    static constexpr auto df15 = futils::binary::DoubleFloat(1e+308);
    static constexpr auto df16 = futils::binary::DoubleFloat(1e+154);
    static constexpr auto df17 = futils::binary::DoubleFloat(1e-77);

    static constexpr auto exp0 = (df0.exponent() + 1) / 3 - ((df0.exponent() - 1) / 32);
    static constexpr auto exp1 = (df1.exponent() + 1) / 3 - ((df1.exponent() - 1) / 32);
    static constexpr auto exp2 = (df2.exponent() + 1) / 3 - ((df2.exponent() - 1) / 32);
    static constexpr auto exp3 = (df3.exponent() + 1) / 3 - ((df3.exponent() - 1) / 32);
    static constexpr auto exp4 = (df4.exponent() + 1) / 3 - ((df4.exponent() - 1) / 32);
    static constexpr auto exp5 = (df5.exponent() + 1) / 3 - ((df5.exponent() - 1) / 32);
    static constexpr auto exp6 = (df6.exponent() + 1) / 3 - ((df6.exponent() - 1) / 32);
    static constexpr auto exp7 = (df7.exponent() + 1) / 3 - ((df7.exponent() - 1) / 32);
    static constexpr auto exp8 = (df8.exponent() + 1) / 3 - ((df8.exponent() - 1) / 32);
    static constexpr auto exp9 = (df9.exponent() + 1) / 3 - ((df9.exponent() - 1) / 32);
    static constexpr auto exp10 = (df10.exponent() + 1) / 3 - ((df10.exponent() - 1) / 32);
    static constexpr auto exp11 = (df11.exponent() + 1) / 3 - ((df11.exponent() - 1) / 32);
    static constexpr auto exp12 = (df12.exponent() + 1) / 3 - ((df12.exponent() - 1) / 32);
    static constexpr auto exp13 = (df13.exponent() + 1) / 3 - ((df13.exponent() - 1) / 32);
    static constexpr auto exp14 = (df14.exponent() + 1) / 3 - ((df14.exponent() - 1) / 32);
    static constexpr auto exp15 = (df15.exponent() + 1) / 3 - ((df15.exponent() - 1) / 32);
    static constexpr auto exp16 = (df16.exponent() + 1) / 3 - ((df16.exponent() - 1) / 32);
    static constexpr auto exp17 = (df17.exponent() + 1) / 3 - ((df17.exponent() - 1) / 32);

    static constexpr auto max = futils::binary::max_normalized<double>;
    static constexpr auto max_int = max.to_int();
    static constexpr auto max_f = max.to_float();
    static constexpr auto max_de = futils::binary::max_denormalized<double>;
    static constexpr auto norm_max = futils::binary::normalize(max_de).to_float();
    static constexpr auto max_de_f = max_de.to_float();
    static constexpr auto min_de = futils::binary::min_denormalized<double>;
    static constexpr auto min_de_i = min_de.to_int();
    static constexpr auto norm = futils::binary::normalize(min_de).to_float();
    static constexpr auto min_de_f = min_de.to_float();
    static constexpr auto min_no = futils::binary::min_normalized<double>;
    static constexpr auto min_no_f = min_no.to_float();
    static constexpr auto ep = futils::binary::epsilon<double>.to_float();
    static constexpr auto norm_ep = futils::binary::normalize(ep).to_float();
    static constexpr auto exp_max = (min_no.exponent() + 1) / 3;

    static constexpr auto frac = df1.fraction_with_implicit_1();
    static constexpr auto dfr = df1.to_float();
    static constexpr auto dfr2 = df2.to_float();
    static constexpr auto red = futils::console::escape::letter_color<futils::console::escape::ColorPalette::red>;
    static constexpr auto reset = futils::console::escape::color_reset;

    static void observe(futils::math::ObservePoint p, size_t layer, const char* name, futils::binary::DoubleFloat lp, futils::binary::DoubleFloat lm, futils::binary::DoubleFloat num, futils::binary::DoubleFloat ana, futils::binary::DoubleFloat rel, auto... dim) {
        futils::wrap::cout_wrap() << layer << ":" << name << ":" << lp.to_float() << ":" << lm.to_float() << ":" << num.exponent() << ":" << ana.exponent() << ":" << red << rel.exponent() << reset;
        auto call = [&](auto i) {
            futils::wrap::cout_wrap() << ":" << i;
        };
        (call(dim), ...);
        futils::wrap::cout_wrap() << "\n";
    }

    static void observe(futils::math::ObservePoint p, size_t layer, const char* name) {
        futils::wrap::cout_wrap() << "start " << name << " at " << layer << "\n";
    }

    static void observe(futils::math::ObservePoint p, size_t layer, const char* name, futils::binary::DoubleFloat worst) {
        futils::wrap::cout_wrap() << "end " << name << " at " << layer << ":" << worst.to_float() << ":" << red << worst.exponent() << reset << "\n";
    }
};

using Model = futils::math::transformer::Transformer<double, 32, 16, 16, 28, Config>;

struct DatasetItem {
    Model::TokenInput input_ids;
    Model::TokenInput target_ids;
};
constexpr size_t PAD_ID = 27;
constexpr size_t EOS_ID = 0;

constexpr DatasetItem encode_name(std::string_view name) {
    DatasetItem item{};

    // 全要素を一旦 PAD_ID で埋める
    for (size_t i = 0; i < item.input_ids.size(); i++) {
        item.input_ids[i] = PAD_ID;
        item.target_ids[i] = PAD_ID;
    }

    // 系列の構成:
    // 元データ:  <S>,  c0,  c1,  c2,  <S>, <P>, <P>...
    // input:    <S>,  c0,  c1,  c2,  <P>, <P>, <P>...
    // target:    c0,  c1,  c2,  <S>, <P>, <P>, <P>...

    item.input_ids[0] = EOS_ID;  // 開始記号 <S>

    size_t len = name.size();
    for (size_t i = 0; i < len && (i + 1) < item.input_ids.size(); i++) {
        size_t char_id = static_cast<size_t>(name[i] - 'a' + 1);
        item.input_ids[i + 1] = char_id;
        item.target_ids[i] = char_id;
    }

    // 単語の終了記号 <S> を target 側に付与
    if (len < item.target_ids.size()) {
        item.target_ids[len] = EOS_ID;
    }

    return item;
}

std::string generate(Model::FullWeights<2>& w, std::string_view prefix) {
    std::string buffer;
    std::random_device rng;
    Model::generate_with_prefix(
        buffer, w, 0.7, rng, PAD_ID, EOS_ID,
        [&](std::string& buffer, Model::TokenInput& input) {
            for (size_t i = 0; i < prefix.size() && (i + 1) < Model::token_count; i++) {
                input[i + 1] = static_cast<size_t>(prefix[i] - 'a' + 1);
            }
            return prefix.size();
        },
        [](std::string& buffer, size_t id) { buffer += static_cast<char>('a' + (id - 1)); });
    return buffer;
}

int main() {
    const std::vector<std::string> raw_names = {
        "emma", "olivia", "liam", "noah", "ava",
        "sophia", "lucas", "mia", "oliver", "amelia",
        "james", "harper", "henry", "evelyn", "alex",
        "leo", "mila", "jack", "ella", "luna"};
    Model::FullWeights<2> weights;
    futils::math::transformer::WeightInitializer init{1337};
    weights.init(init);
    std::vector<DatasetItem> dataset;
    for (const auto& name : raw_names) {
        dataset.push_back(encode_name(name));
    }

    // 学習ループ (1000 エポック)
    float lr = 0.05f;
    for (size_t epoch = 0; epoch < 1000; epoch++) {
        float total_loss = 0.0f;
        futils::binary::DoubleFloat(*rb)[28][32] = (futils::binary::DoubleFloat(*)[28][32])weights.embed.raw();
        futils::binary::DoubleFloat(*rv)[32][28] = (futils::binary::DoubleFloat(*)[32][28])weights.vocab.raw();
        futils::binary::DoubleFloat(*rq1)[32][32] = (futils::binary::DoubleFloat(*)[32][32])weights.weights[0].q.raw();
        futils::binary::DoubleFloat(*rq2)[32][32] = (futils::binary::DoubleFloat(*)[32][32])weights.weights[1].q.raw();

        for (const auto& item : dataset) {
            total_loss += Model::train_step(
                item.input_ids,
                item.target_ids,
                weights,
                lr, 5);
            /*
            weights.vocab.for_each([&](size_t i, size_t j) {
                auto b = (*rb)[j][i].fraction();
                auto v = (*rv)[i][j].fraction();
                (*rb)[j][i].fraction(v);
                (*rv)[i][j].fraction(b);
            });
            weights.weights[0].q.for_each([&](size_t i, size_t j) {
                auto q1 = (*rq1)[i][j];
                auto q2 = (*rq2)[i][j];
                (*rq2)[j][i].fraction(q1.fraction());
                (*rq1)[j][i].fraction(q2.fraction());
            });
            */
        }

        if (futils::binary::SingleFloat(total_loss).is_nan()) {
            std::cout << "Loss become NaN at epoch " << epoch << "\n";
            break;
        }

        if (epoch % 100 == 0) {
            std::cout << "Exponent: " << (*rb)[0][0].exponent() << std::endl;
            std::cout << "Fraction: " << (*rb)[0][0].fraction() << std::endl;

            std::cout << "Epoch " << epoch << " Loss: " << total_loss / dataset.size() << std::endl;
            std::cout << "\n=== generation test ===" << std::endl;

            // 1. 完全自律生成 (<S> から開始)
            std::string generated_name = generate(weights, "");
            std::cout << "Generated: " << generated_name << std::endl;

            // 2. 指定文字からの補完
            std::cout << "Prefix 'e' -> " << generate(weights, "e") << std::endl;
            std::cout << "Prefix 'l' -> " << generate(weights, "l") << std::endl;
            std::cout << "Prefix 'm' -> " << generate(weights, "m") << std::endl;
        }
    }
}
