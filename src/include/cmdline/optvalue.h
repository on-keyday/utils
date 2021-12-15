/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// optvalue - option value holder
#pragma once

#include <type_traits>
#include "../wrap/lite/smart_ptr.h"

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
                virtual void* raw() = 0;
                virtual const void* raw() const = 0;
                virtual interface* copy() const = 0;
                virtual ~interface() {}
            };

            template <class T>
            struct implement : interface {
                T value;

                const Type* type() const override {
                    return cmdline::type<T>();
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

            template <class T>
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

            OptValue(OptValue&& v) {
                iface = v.iface;
                v.iface = nullptr;
            }

            const Type* type() const {
                return iface ? iface->type() : nullptr;
            }

            template <class T>
            T* value() {
                if (cmdline::type<T>() == type()) {
                    return reinterpret_cast<T*>(iface->raw());
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

            ~OptValue() {
                del_iface(iface);
            }
        };

        enum class OptFlag {
            none = 0,
            required = 0x1,        // option must set
            must_assign = 0x2,     // value must set with `=`
            no_option_like = 0x4,  // `somevalue` type disallow string like `option`
            once_in_cmd = 0x8,     // allow only once to set
            need_value = 0x10,
        };

        template <class String>
        struct Option {
            OptValue<> defvalue;
            String help;
            OptFlag flag = OptFlag::none;
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionDesc {
            using option_t = wrap::shared_ptr<Option<String>>;
            Vec<option_t> vec;
            Map<String, option_t> desc;

            bool find(auto& name, option_t& opt) {
                if (auto found = desc.find(name); found != desc.end()) {
                    opt = std::get<1>(*found);
                    return true;
                }
                return false;
            }
        };

        template <class String>
        struct OptionResult {
            using option_t = wrap::shared_ptr<Option<String>>;
            option_t base;
            OptValue<> value;
        };

        template <class String, template <class...> class Vec, template <class...> class Map>
        struct OptionSet {
            Map<String, OptValue<>> result;
        };

    }  // namespace cmdline
}  // namespace utils
