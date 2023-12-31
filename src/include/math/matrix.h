/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <type_traits>
#include <initializer_list>
#include <tuple>
#include <cmath>

namespace utils::math {

    template <size_t m, size_t n, class Type = double>
    struct Matrix {
       private:
        Type matrix[m][n]{};

        template <size_t x, size_t y, class Ty>
        friend struct Matrix;

       public:
        struct MatrixColom {
           private:
            Type* data = nullptr;

            friend struct Matrix;

            constexpr MatrixColom(Type* d)
                : data(d) {}

           public:
            constexpr Type& operator[](size_t i) {
                return data[i];
            }

            constexpr size_t size() const {
                return n;
            }
        };

        struct InitVec {
            Type vec[n];
        };

        constexpr Matrix(std::initializer_list<InitVec> v) {
            if (v.size() != m) {
                throw "unexpected initializer";
            }
            for (auto i = 0; i < m; i++) {
                for (auto j = 0; j < n; j++) {
                    matrix[i][j] = v.begin()[i].vec[j];
                }
            }
        }

        constexpr Matrix() = default;

        constexpr MatrixColom operator[](size_t i) {
            return MatrixColom{matrix[i]};
        }

        constexpr size_t size() const {
            return m;
        }

        constexpr static bool for_each_if(auto&& cb) {
            for (size_t i = 0; i < m; i++) {
                for (size_t j = 0; j < n; j++) {
                    if (!cb(i, j)) {
                        return false;
                    }
                }
            }
            return true;
        }

        constexpr static void for_each(auto&& cb) {
            for_each_if([&](auto i, auto j) {
                cb(i, j);
                return true;
            });
        }

        // 転置行列
        constexpr Matrix<n, m, Type> T() const {
            Matrix<n, m, Type> t;
            for_each([&](auto i, auto j) {
                t.matrix[j][i] = matrix[i][j];
            });
            return t;
        }

        // 等しい
        constexpr friend bool operator==(const Matrix& a, const Matrix& b) {
            return Matrix::for_each_if([&](auto i, auto j) {
                return a.matrix[i][j] == b.matrix[i][j];
            });
        }

        // 足し算
        constexpr friend Matrix operator+(const Matrix& a, const Matrix& b) {
            Matrix p;
            Matrix::for_each([&](auto i, auto j) {
                p.matrix[i][j] = a.matrix[i][j] + b.matrix[i][j];
            });
            return p;
        }

        // 引き算
        constexpr friend Matrix operator-(const Matrix& a, const Matrix& b) {
            Matrix p;
            Matrix::for_each([&](auto i, auto j) {
                p.matrix[i][j] = a.matrix[i][j] - b.matrix[i][j];
            });
            return p;
        }

        // 単項マイナス
        constexpr Matrix operator-() {
            Matrix p;
            Matrix::for_each([&](auto i, auto j) {
                p.matrix[i][j] = -matrix[i][j];
            });
            return p;
        }

        // スカラー倍
        constexpr friend Matrix operator*(const Matrix& a, Type b) {
            Matrix p;
            Matrix::for_each([&](auto i, auto j) {
                p.matrix[i][j] = a.matrix[i][j] * b;
            });
            return p;
        }

        constexpr friend Matrix operator*(Type a, const Matrix& b) {
            return b * a;
        }

        constexpr bool is_zero() const {
            return for_each_if([&](auto i, auto j) {
                return matrix[i][j] == 0;
            });
        }

        // 単位行列
        constexpr static Matrix identity() {
            if constexpr (n != m) {
                throw "cannot be identity matrix";
            }
            Matrix u;
            u.for_each([&](auto i, auto j) {
                if (i == j) {
                    u.matrix[i][j] = 1;
                }
            });
            return u;
        }

        // 対角行列
        constexpr bool is_diagonal() const {
            if constexpr (n != m) {
                return false;
            }
            else {
                return Matrix::for_each_if([&](auto i, auto j) {
                    if (i == j) {
                        return matrix[i][j] != 0;
                    }
                    else {
                        return matrix[i][j] == 0;
                    }
                });
            }
        }

        // 上三角行列
        constexpr bool is_upper_triangle() const {
            if constexpr (n != m) {
                return false;
            }
            else {
                return Matrix::for_each_if([&](auto i, auto j) {
                    if (i <= j) {
                        return matrix[i][j] != 0;
                    }
                    else {
                        return matrix[i][j] == 0;
                    }
                });
            }
        }

        // 下三角行列
        constexpr bool is_lower_triangle() const {
            if constexpr (n != m) {
                return false;
            }
            else {
                return Matrix::for_each_if([&](auto i, auto j) {
                    if (i >= j) {
                        return matrix[i][j] != 0;
                    }
                    else {
                        return matrix[i][j] == 0;
                    }
                });
            }
        }

        constexpr bool can_lu_decomposition() const {
            if (n != m) {
                return false;
            }
            else if constexpr (m == 1) {
                return matrix[0][0] != 0;
            }
            else {
                auto [d, ok] = det();
                if (!ok) {
                    return false;
                }
                if (d == 0) {
                    return false;
                }
                auto [p, ok2] = pullout(m - 1, n - 1);
                if (!ok2) {
                    return false;
                }
                return p.can_lu_decomposition();
            }
        }

        // LU分解
        constexpr std::tuple<Matrix, Matrix, bool> lu_decomposition() const {
            if constexpr (n != m) {
                return {{}, {}, false};
            }
            else {
                if (!can_lu_decomposition()) {
                    return {{}, {}, false};
                }
                Matrix L = identity();
                Matrix U = *this;

                auto elementary_matrix3 = [](auto c, auto i, auto j) {
                    auto p = identity();
                    p[i][j] = c;
                    return p;
                };

                for (auto i = 0; i < m - 1; i++) {
                    for (auto j = i + 1; j < m; j++) {
                        auto c = -U[j][i] / U[i][i];
                        auto q3 = elementary_matrix3(c, j, i);
                        auto q3_inv = elementary_matrix3(-c, j, i);
                        L = L * q3_inv;
                        U = q3 * U;
                    }
                }
                return {L, U, true};
            }
        }

        // 行と列を1つづつ抜き取る
        constexpr std::pair<Matrix<m - 1, n - 1, Type>, bool> pullout(size_t h, size_t w) const {
            if constexpr (m != n || m - 1 == 0) {
                return {{}, false};
            }
            else {
                if (h >= m || w >= n) {
                    return {{}, false};
                }
                Matrix<m - 1, n - 1, Type> a;
                size_t ishift = 0;
                for (size_t i = 0; i < m; i++) {
                    if (i == h) {
                        ishift = 1;
                        continue;
                    }
                    size_t jshift = 0;
                    for (size_t j = 0; j < n; j++) {
                        if (j == w) {
                            jshift = 1;
                            continue;
                        }
                        a.matrix[i - ishift][j - jshift] = matrix[i][j];
                    }
                }
                return {a, true};
            }
        }

        // 行列式
        constexpr std::pair<Type, bool> det() const {
            if constexpr (m != n) {
                return {0, false};
            }
            else if constexpr (m == 1) {
                return {matrix[0][0], true};
            }
            else if constexpr (m == 2) {
                return {matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0], true};
            }
            else {
                // 余因子展開 (0行目について)
                auto sign = [](auto i) {
                    return i & 1 ? -1 : 1;
                };
                Type sum = 0;
                for (auto i = 0; i < n; i++) {
                    if (matrix[0][i] == 0) {
                        continue;  // 結果0になるのでスキップ
                    }
                    auto [p, ok] = pullout(0, i);
                    if (!ok) {
                        return {0, false};
                    }
                    auto [d, ok2] = p.det();
                    if (!ok2) {
                        return {0, false};
                    }
                    sum += matrix[0][i] * sign(i) * d;
                }
                return sum;
            }
        }

        // 逆行列
        constexpr std::pair<Matrix, bool> inverse() const {
            if constexpr (m != n) {
                return {{}, false};
            }
            else {
                Matrix<m, n * 2, Type> p;
                for_each([&](auto i, auto j) {
                    p.matrix[i][j] = matrix[i][j];
                    p.matrix[i][j + n] = (i == j) ? 1 : 0;
                });
                auto div = [&](auto& arr, auto val) {
                    if (val == 0) {
                        return false;
                    }
                    for (auto& d : arr) {
                        d /= val;
                    }
                    return true;
                };
                for (size_t i = 0; i < n; i++) {
                    if (!div(p.matrix[i], p.matrix[i][i])) {
                        return {{}, false};
                    }
                    for (size_t j = 0; j < n; j++) {
                        if (i == j) {
                            continue;
                        }
                        auto t = p.matrix[j][i];
                        for (auto k = 0; k < n * 2; k++) {
                            p.matrix[j][k] = p.matrix[j][k] - p.matrix[i][k] * t;
                        }
                    }
                }
                Matrix r;
                for_each([&](auto i, auto j) {
                    r.matrix[i][j] = p.matrix[i][j + n];
                });
                return {r, true};
            }
        }

        // 階段行列
        constexpr size_t is_step_matrix() const {
            size_t cur_i = -1;
            bool done = false;
            size_t pos = 0;
            bool be_zero = false;
            return for_each_if([&](auto i, auto j) {
                if (cur_i != i) {
                    if (i != 0 && !done) {
                        if (pos == 0) {
                            return false;
                        }
                        be_zero = true;
                    }
                    cur_i = i;
                    done = false;
                }
                if (!done && matrix[i][j] != 0) {
                    if (be_zero || j < pos) {
                        return false;
                    }
                    pos = j + 1;
                    done = true;
                }
                return true;
            });
        }

        constexpr size_t pibot_max(size_t k) {
            if constexpr (m != n) {
                return -1;
            }
            else {
                if (k > n) {
                    return 0;
                }
                auto abs = [](auto a) {
                    return a < 0 ? -a : a;
                };
                Type mt = matrix[0][k];
                size_t index = 0;
                for (size_t i = 1; i < n; i++) {
                    auto a = abs(matrix[i][k]);
                    if (mt < a) {
                        mt = a;
                        index = i;
                    }
                }
                if (m == 0) {
                    return -1;
                }
                return index;
            }
        }

        // 掛け算
        template <size_t x, size_t y, size_t z, class Ty>
        constexpr friend Matrix<x, z, Ty> operator*(const Matrix<x, y, Ty>& a, const Matrix<y, z, Ty>& b);
        // アダマール積
        template <size_t x, size_t y, class Ty>
        constexpr friend Matrix<x, y, Ty> hadamard(const Matrix<x, y, Ty>& a, const Matrix<x, y, Ty>& b);
    };

    // 掛け算
    template <size_t x, size_t y, size_t z, class Ty>
    constexpr Matrix<x, z, Ty> operator*(const Matrix<x, y, Ty>& a, const Matrix<y, z, Ty>& b) {
        Matrix<x, z, Ty> p;
        for (size_t j = 0; j < x; j++) {          // pとaの行の移動
            for (size_t k = 0; k < z; k++) {      // pとbの列の移動
                for (size_t i = 0; i < y; i++) {  // aの行とbの列の移動
                    p.matrix[j][k] += a.matrix[j][i] * b.matrix[i][k];
                }
            }
        }
        return p;
    }

    // アダマール積
    template <size_t x, size_t y, class Ty>
    constexpr Matrix<x, y, Ty> hadamard(const Matrix<x, y, Ty>& a, const Matrix<x, y, Ty>& b) {
        Matrix<x, y, Ty> r;
        r.for_each([&](auto i, auto j) {
            r.matrix[i][j] = a.matrix[i][j] * b.matrix[i][j];
        });
        return r;
    }

    // x軸に対して対称移動
    template <class Type = double>
    constexpr Matrix<2, 2, Type> symm_move_x() {
        return {
            {1, 0},
            {0, -1},
        };
    }

    // y軸に対して対称移動
    template <class Type = double>
    constexpr Matrix<2, 2, Type> symm_move_y() {
        return {
            {-1, 0},
            {0, 1},
        };
    }

    // 原点に対して対称移動
    template <class Type = double>
    constexpr Matrix<2, 2, Type> symm_move_o() {
        return {
            {-1, 0},
            {0, -1},
        };
    }

    // y=xに対して対称移動
    template <class Type = double>
    constexpr Matrix<2, 2, Type> symm_move_x_y() {
        return {
            {0, 1},
            {1, 0},
        };
    }

    // 拡大/縮小
    template <class Type = double>
    constexpr Matrix<2, 2, Type> expand(Type x, Type y) {
        return {
            {x, 0},
            {0, y},
        };
    }

    // 回転移動
    template <class Type = double>
    constexpr Matrix<2, 2, Type> rotate(Type sheta) {
        return {
            {cos(sheta), -sin(sheta)},
            {sin(sheta), cos(sheta)},
        };
    }

    namespace test {

        constexpr auto check_matrix() {
            Matrix<4, 3> mat{
                {1, 2, 3},
                {0, 1, 2},
                {0, 0, 1},
                {0, 0, 0},
            };
            return mat.is_step_matrix();
        }

        static_assert(check_matrix());
    }  // namespace test
}  // namespace utils::math
