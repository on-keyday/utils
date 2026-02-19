/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "binary/float.h"
#include "cg/base_object.h"
#include "math/vector.h"
#include "base_object.h"
#include "ray.h"

namespace futils::cg::object {

    template <class T = double>
    struct Triangle {
        BaseObject base;
        math::Vec3<T> points[3];
    };

    auto is_non_negative(auto v) {
        auto f = binary::make_float(v);
        return !f.is_nan() && !f.sign();
    }
    auto is_less_or_equal_to_1(auto v) {
        return v <= 1;
    }
    constexpr float epsilon = 1e-6f;
    // reference: https://pheema.hatenablog.jp/entry/ray-triangle-intersection
    template <class Ty>
    constexpr HitInfo<Ty> hit(const ray::Ray<Ty>& r, const Triangle<Ty>& tri) {
        auto e1 = tri.points[1] - tri.points[0];
        auto e2 = tri.points[2] - tri.points[0];
        auto d = r.direction;
        auto P = cross(d, e2);
        auto det_v = binary::make_float(dot(e1, P));
        auto det = det_v.to_float();
        if (det_v.is_nan() || -epsilon < det && det < epsilon) {
            return invalid_hit<Ty>;
        }
        auto vo = r.origin - tri.points[0];
        auto Q = cross(vo, e1);
        auto T = vo;
        auto invDet = 1 / det;
        auto u = dot(T, P) * invDet;
        if (!is_non_negative(u) || !is_less_or_equal_to_1(u)) {
            return invalid_hit<Ty>;
        }
        auto v = dot(d, Q) * invDet;
        if (!is_non_negative(v) || !is_less_or_equal_to_1(u + v)) {
            return invalid_hit<Ty>;
        }
        auto t = dot(e2, Q) * invDet;
        if (t > 0) {
            return {binary::make_float(t)};
        }
        return invalid_hit<Ty>;
    }

    template <class T>
    constexpr auto normal(const NormalContext<T>& ctx, const Triangle<T>& tri) {
        auto edge1 = tri.points[1] - tri.points[0];
        auto edge2 = tri.points[2] - tri.points[0];
        return cross(edge1, edge2);
    }

    template <class T = double>
    struct TriangularPyramid {
        BaseObject base;
        math::Vec3<T> points[4];
    };

    constexpr math::Vector<3, std::uint8_t> selector[]{
        {0, 1, 2},
        {0, 1, 3},
        {0, 2, 3},
        {1, 2, 3},
    };

    template <class Ty>
    constexpr HitInfo<Ty> hit(const ray::Ray<Ty>& r, const TriangularPyramid<Ty>& tri) {
        Triangle<Ty> t;
        auto min_point = binary::quiet_nan<Ty>;
        size_t index = 0;
        for (size_t i = 0; i < 4; i++) {
            t.points[0] = tri.points[selector[i][0]];
            t.points[1] = tri.points[selector[i][1]];
            t.points[2] = tri.points[selector[i][2]];
            auto point = hit(r, t);
            if (!point.hit_point.is_nan() && (min_point.is_nan() || point.hit_point.to_float() < min_point.to_float())) {
                min_point = point.hit_point;
                index = i;
            }
        }
        return {min_point, index};
    }

    template <class T>
    constexpr auto normal(const NormalContext<T>& ctx, const TriangularPyramid<T>& tri) {
        Triangle<T> t;
        t.points[0] = tri.points[selector[ctx.hit_info.index % 4][0]];
        t.points[1] = tri.points[selector[ctx.hit_info.index % 4][1]];
        t.points[2] = tri.points[selector[ctx.hit_info.index % 4][2]];
        return normal(ctx, t);
    }

}  // namespace futils::cg::object
