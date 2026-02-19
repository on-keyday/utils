/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "rgb.h"

namespace futils::color {
    // https://tecsingularity.com/%E7%94%BB%E5%83%8F%E5%87%A6%E7%90%86/ycbcr/
    template <class T, class U = T>
    struct YUV {
        T y{};
        U u{};
        U v{};
    };

    template <class T, class RT>
    concept yuv_ratio = requires(T t, RGB<RT> r) {
        { t.r_ratio };
        { t.g_ratio };
        { t.b_ratio };
        { t.ratio_scale };
        { t.clip_min };
        { t.clip_max };
    };

    template <class T, class U = T, class RT, yuv_ratio<RT> Rat>
    constexpr YUV<T, U> from_rgb(RGB<RT> rgb, Rat rat) {
        YUV<T, U> yuv;
        auto y = (rat.r_ratio * rgb.r + rat.g_ratio * rgb.g + rat.b_ratio * rgb.b);
        auto u = (decltype(y)(rgb.b) * rat.ratio_scale - y) / (2 * (rat.ratio_scale - rat.b_ratio));
        auto v = (decltype(y)(rgb.r) * rat.ratio_scale - y) / (2 * (rat.ratio_scale - rgb.r_ratio));
        y /= rat.ratio_scale;
        constexpr auto clip = [&](auto t, auto x) -> decltype(t) {
            if (x < rat.clip_min) {
                return rat.clip_min;
            }
            if (x > rat.clip_max) {
                return rat.clip_max;
            }
            return x;
        };
        yuv.y = clip(yuv.y, y);
        yuv.u = clip(yuv.u, u);
        yuv.v = clip(yuv.v, v);
        return yuv;
    }

    struct BT601Ratio {
        static constexpr auto ratio_scale = 1000;
        static constexpr auto r_ratio = 299;
        static constexpr auto g_ratio = 587;
        static constexpr auto b_ratio = 114;
        static constexpr auto clip_min = 0;
        static constexpr auto clip_max = 255;
    };
}  // namespace futils::color
