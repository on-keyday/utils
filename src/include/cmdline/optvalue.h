/*license*/

// optvalue - option value holder
#pragma once

#include <type_traits>
#include <utility>
#include <memory>

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
                virtual const Type* type() const = 0;
                virtual const void* raw() const = 0;
                virtual interface* copy() const = 0;
                virtual ~interface() {}
            };

            template <class T>
            struct implement : interface {
                T value;

                template <class V>
                implement(V&& v)
                    : value(std::forward<V>(v)) {}

                const Type* type() const override {
                    return cmdline::type<T>();
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

            static void del_iface(interface* v) {
                Alloc::delete_value(v);
            }

            interface* iface = nullptr;

           public:
            constexpr OptValue() {}
            constexpr OptValue(std::nullptr_t) {}

            template <class T>
            OptValue(T&& value) {
                iface = make_iface(std::forward<T>(value));
            }

            OptValue(const OptValue& v) {
                if (v.iface) {
                    iface = v.iface->copy();
                }
            }

            OptValue(OptValue&& v) noexcept {
                iface = v.iface;
                v.iface = nullptr;
            }

            OptValue& operator=(const OptValue& v) noexcept {
                del_iface(iface);
                iface = v.iface->copy();
                return *this;
            }

            OptValue& operator=(OptValue&& v) noexcept {
                del_iface(iface);
                iface = v.iface;
                v.iface = nullptr;
                return *this;
            }

            const Type* type() const {
                return iface ? iface->type() : nullptr;
            }

            template <class T>
            T* value() {
                if (cmdline::type<T>() == type()) {
                    return reinterpret_cast<T*>(const_cast<void*>(iface->raw()));
                }
                return nullptr;
            }

            template <class T>
            const T* value() const {
                if (type<T>() == type()) {
                    return reinterpret_cast<const T*>(iface->raw());
                }
                return nullptr;
            }

            operator bool() {
                return iface != nullptr;
            }

            ~OptValue() {
                del_iface(iface);
            }
        };

    }  // namespace cmdline
}  // namespace utils
