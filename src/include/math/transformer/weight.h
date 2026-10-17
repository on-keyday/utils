/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <random>
#include <cmath>
#include "../matrix.h"
#include "math/vector.h"

namespace futils::math::transformer {

    struct WeightInitializer {
        std::mt19937 gen;

        explicit WeightInitializer(uint32_t seed = 42) : gen(seed) {}

        // 行列に対する正規分布初期化: N(0, stddev^2)
        template <size_t Rows, size_t Cols, class Type, class Conf>
        void init_matrix(Matrix<Rows, Cols, Type, Conf>& mat, Type stddev) {
            std::normal_distribution<Type> dist(Type(0), stddev);
            mat.for_each([&](size_t i, size_t j) {
                mat[i][j] = dist(gen);
            });
        }

        // Xavier (Glorot) 正規分布: stddev = sqrt(2 / (fan_in + fan_out))
        template <size_t Rows, size_t Cols, class Type, class Conf>
        void init_xavier(Matrix<Rows, Cols, Type, Conf>& mat) {
            Type stddev = std::sqrt(Type(2) / Type(Rows + Cols));
            init_matrix(mat, stddev);
        }

        // Kaiming (He) 正規分布: stddev = sqrt(2 / fan_in)
        template <size_t Rows, size_t Cols, class Type, class Conf>
        void init_kaiming(Matrix<Rows, Cols, Type, Conf>& mat) {
            Type stddev = std::sqrt(Type(2) / Type(Rows));
            init_matrix(mat, stddev);
        }

        // ベクトルに対する定数初期化
        template <size_t Dim, class Type, class Conf>
        void init_vector_const(Vector<Dim, Type, Conf>& vec, Type val) {
            for (size_t i = 0; i < Dim; i++) {
                vec[i] = val;
            }
        }
    };
}  // namespace futils::math::transformer