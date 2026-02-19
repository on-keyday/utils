/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <variant>
#include "base_object.h"
#include "cg/base_object.h"
#include "cg/ray.h"
#include "sphere.h"
#include "triangle.h"

namespace futils::cg::object {
    template <class T>
    using AnyObject = std::variant<Sphere<T>, Triangle<T>, TriangularPyramid<T>>;

    template <class T>
    BaseObject* get_base(AnyObject<T>& t) {
        return std::visit([&](auto& v) {
            return std::addressof(v.base);
        },
                          t);
    }

    template <class T>
    const BaseObject* get_base(const AnyObject<T>& t) {
        return std::visit([&](auto& v) {
            return std::addressof(v.base);
        },
                          t);
    }

    template <class Ty>
    constexpr HitInfo<Ty> hit(const ray::Ray<Ty>& ray, const AnyObject<Ty>& object) {
        return std::visit([&](auto& object) {
            return hit(ray, object);  // tvのみを計算
        },
                          object);
    }

    template <class T>
    constexpr auto normal(const NormalContext<T>& ctx, const AnyObject<T>& object) {
        return std::visit([&](auto& object) {
            return normal(ctx, object);  // tvのみを計算
        },
                          object);
    }
}  // namespace futils::cg::object