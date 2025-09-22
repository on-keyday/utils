/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "base_object.h"
#include "binary/float.h"
#include "camera.h"
#include "cg/base_object.h"
#include "light.h"
#include <math/matrix.h>
#include "math/vector.h"
#include "sphere.h"
#include <cstdint>
#include <vector>
#include <number/array.h>
#include "any_object.h"
#include "shading.h"
#include "triangle.h"

namespace futils::cg {

    namespace render {

        template <class T = double, class S = size_t, template <class...> class Container = std::vector>
        struct Renderer {
            camera::Camera<T> camera;
            Container<object::AnyObject<T>> objects;
            Container<light::Light<T>> lights;
            T ambient_light{};
            math::Vec2<S> size;

            constexpr void render(auto&& renderer) const {
                size_t x_max = size[0];
                size_t y_max = size[1];
                for (size_t y = 0; y < y_max; y++) {
                    for (size_t x = 0; x < x_max; x++) {
                        auto w = (camera.look_at - camera.position).normalize();
                        auto u = cross(w, camera.up).normalize();
                        auto v = cross(u, w).normalize();
                        auto normalized_x = (T(x) + 0.5) * 2 / x_max - 1;
                        auto normalized_y = -((T(y) + 0.5) * 2 / y_max - 1);
                        auto direction = (w + u * normalized_x + v * normalized_y).normalize();
                        auto ray = ray::Ray<T>{
                            .origin = camera.position,
                            .direction = direction,
                        };
                        auto tv_min = object::HitInfo<T>{binary::infinity<T>};
                        auto bright_max = binary::make_float(ambient_light);
                        auto color = math::Vec3<std::uint8_t>{};

                        // すべてのオブジェクトと交差判定を行う
                        const object::AnyObject<T>* closest_object = nullptr;
                        for (const auto& object : objects) {
                            auto tv = hit(ray, object);
                            if (!tv.hit_point.is_nan() && tv.hit_point.to_float() < tv_min.hit_point.to_float()) {
                                tv_min = tv;
                                closest_object = &object;
                            }
                        }

                        // 2. もしオブジェクトが見つかったら、すべての光源で照らす
                        if (closest_object) {
                            auto combined_bright = binary::make_float(ambient_light);
                            auto intersection_point = ray.origin + ray.direction * tv_min.hit_point.to_float();
                            object::NormalContext<T> ctx{
                                .ray = ray,
                                .hit_info = tv_min,
                                .intersection_point = intersection_point,
                            };
                            auto normal_vec = normal(ctx, *closest_object);

                            for (const auto& light : lights) {
                                auto shadow_direction = (light.position - intersection_point).normalize();
                                auto shadow_ray = ray::Ray<T>{
                                    .origin = intersection_point + normal_vec * 1e-4,  // Start slightly off the surface
                                    .direction = shadow_direction,
                                };

                                // Check for obstructions
                                bool in_shadow = false;
                                for (const auto& shadow_object : objects) {
                                    // Don't check against the object itself
                                    if (&shadow_object == closest_object) {
                                        continue;
                                    }
                                    auto shadow_tv = hit(shadow_ray, shadow_object);

                                    // If the shadow ray hits another object, it's in shadow
                                    if (!shadow_tv.hit_point.is_nan() && shadow_tv.hit_point.to_float() > 1e-4) {
                                        in_shadow = true;
                                        break;
                                    }
                                }

                                if (!in_shadow) {
                                    auto bright = shading(tv_min, ray, *closest_object, light);
                                    if (!bright.is_nan()) {
                                        combined_bright = binary::make_float(combined_bright.to_float() + bright.to_float());
                                    }
                                }
                            }
                            bright_max = combined_bright;
                            if (bright_max.to_float() > 1) {
                                bright_max = binary::make_float(T(1));
                            }
                            color = object::get_base(*closest_object)->color;
                        }

                        renderer(x, y, tv_min, bright_max, color);
                    }
                }
            }
        };

        namespace test {
            template <class T>
            using FixedArray = number::Array<T, 10>;
            constexpr auto renderer = Renderer<double, size_t, FixedArray>{
                .camera = {
                    .position = {50, 30, -50},
                    .look_at = {0, 0, -10},
                    .up = {0, 1, 0},
                },
                .objects = {
                    .buf = {
                        object::Sphere<>{
                            .base = {.color = {255, 255, 255}},
                            .center = {0, 0, -10},
                            .radius = 10,
                        },
                        object::Sphere<>{
                            .base = {.color = {0, 0, 255}},
                            .center = {15, -10, -30},
                            .radius = 10,
                        },

                        object::TriangularPyramid<>{
                            .base = {.color = {0, 255, 0}},
                            .points = {
                                {-10, -10, -10},
                                {30, 0, 0},
                                {0, 30, 0},
                                {0, 0, 30},
                            },
                        },
                    },
                    .i = 3,
                },
                .lights = {
                    .buf = {
                        {
                            .position = {-10, 50, 30},
                            .intensity = 0.3,
                        },
                        {
                            .position = {10, 0, 50},
                            .intensity = 0.2,

                        },
                        {
                            .position = {0, -50, 0},
                            .intensity = 0.2,

                        },
                        {
                            .position = {10, 50, 5},
                            .intensity = 0.2,
                        },
                        {
                            .position = {0, -50, 50},
                            .intensity = 0.2,
                        },
                        {
                            .position = {50, 30, -50},
                            .intensity = 0.2,
                        },
                    },
                    .i = 6,
                },
                .ambient_light = 0.1,
                .size = {512, 512},
            };

            constexpr auto grayscale_rendering() {
                math::Matrix<512, 512> mat;
                renderer.render([&](size_t x, size_t y, auto t, auto bright, auto color) {
                    if (bright.is_nan()) {
                        mat[y][x] = 0;
                        return;
                    }
                    mat[y][x] = bright.to_float();
                });
                return mat;
            }

        }  // namespace test
    }  // namespace render
}  // namespace futils::cg