/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optvalue - option value holder
#pragma once

#include <type_traits>

namespace utils {
    namespace cmdline {
        struct Type {};

        template <class T>
        const Type* type() {
            static Type type_;
            return &type_;
        }

        struct Allocate {
            template <class T, class... Args>
            static T* new_value(Args&&... args) {
                return new T(std::forward<Args>(args)...);
            }

            template <class T>
            static void delete_value(T* v) {
                delete v;
            }
        };

        template <class Alloc = Allocate>
        struct OptValue {
           private:
            struct interface {
                virtual Type* type() const = 0;
                virtual void* raw() = 0;
                virtual const void* raw() const = 0;
                virtual interface* copy() const = 0;
            };

            template <class T>
            struct implement : interface {
                T value;

                Type* type() const override {
                    return type<T>();
                }

                void* raw() override {
                    return reinterpret_cast<void*>(std::addressof(value));
                }
                const void* raw() const override {
                    return reinterpret_cast<const void*>(std::addressof(value));
                }

                interface* copy() const override {
                    return make_iface(value);
                }
            };

            template <class T>
            static interface* make_iface(T&& value) {
                return Alloc::template new_value<implement<std::decay_t<T>>>(std::forward<T>(value));
            }

            interface* iface = nullptr;

           public:
            template <class T>
            OptValue(T&& value) {
                iface = make_iface(std::forward<T>(value));
            }

            Type* type() const {
                return iface ? iface->type() : nullptr;
            }

            template <class T>
            T* value() {
                if (type<T>() == type()) {
                    return reinterpret_cast<T*>(iface->raw());
                }
                return nullptr;
            }
        };

    }  // namespace cmdline
}  // namespace utils
