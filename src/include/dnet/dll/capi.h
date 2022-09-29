/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// capi - capi internal util
#pragma once
#include "../capi/c_context.h"
#include <memory>
#include <atomic>
#include "glheap.h"

namespace utils {
    namespace dnet {
        namespace capi {
            enum ptr_ctrl {
                incref,
                decref,
                getptr,
            };

            template <class T = void>
            struct resource : resource<void> {
                T value;
                std::atomic_uint32_t ref;
                void (*gc)(T*);
                static void* ctrl_impl(void* ptr, ptr_ctrl ct) {
                    auto r = static_cast<resource<T>*>(ptr);
                    if (ct == incref) {
                        r->ref++;
                    }
                    else if (ct == decref) {
                        if (0 == --r->ref) {
                            if (r->gc) {
                                r->gc(r);
                            }
                        }
                    }
                    else if (ct == getptr) {
                        return std::addressof(value);
                    }
                    return nullptr;
                }
                constexpr resource(void (*g)(T*))
                    : ctrl(ctrl_impl), gc(g) {}
                constexpr resource()
                    : resource(delete_with_global_heap<T>) {}
            };

            template <>
            struct resource<void> {
                void* (*ctrl)(void*, ptr_ctrl) = nullptr;
            };

            struct generic_resource {
               protected:
                resource<void>* impl;

               public:
                constexpr generic_resource()
                    : impl(nullptr) {}

                constexpr generic_resource(resource<void>* impl)
                    : impl(impl) {
                    if (impl && impl->ctrl) {
                        impl->ctrl(impl, incref);
                    }
                }

                constexpr generic_resource(const generic_resource& m)
                    : impl(m.impl) {
                    if (impl && impl->ctrl) {
                        impl->ctrl(impl, incref);
                    }
                }

                constexpr generic_resource& operator=(const generic_resource& m) {
                    if (this == &m) {
                        return *this;
                    }
                    if (m.impl && m.impl->ctrl) {
                        m.impl->ctrl(impl, incref);
                    }
                    this->~generic_resource();
                    impl = m.impl;
                    return *this;
                }

                constexpr generic_resource(generic_resource&& m)
                    : impl(std::exchange(impl, nullptr)) {}

                constexpr generic_resource& operator=(generic_resource&& m) {
                    if (this == &m) {
                        return *this;
                    }
                    if (impl != m.impl) {
                        this->~generic_resource();
                    }
                    impl = std::exchange(m.impl, nullptr);
                    return *this;
                }

                constexpr ~generic_resource() {
                    if (impl && impl->ctrl) {
                        impl->ctrl(impl, decref);
                    }
                }

                constexpr void* get() const {
                    if (impl && impl->ctrl) {
                        return impl->ctrl(impl, getptr);
                    }
                    return nullptr;
                }

                constexpr explicit operator bool() const {
                    return impl != nullptr;
                }
            };

            template <class T>
            struct typed_resource : protected generic_resource {
                constexpr typed_resource()
                    : generic_resource() {}

                constexpr typed_resource(resource<T>* impl)
                    : generic_resource(impl) {
                }

                constexpr T* get() const {
                    return static_cast<T*>(generic_resource::get());
                }

                constexpr T* operator->() const {
                    return get();
                }

                constexpr T& operator*() const {
                    return *get();
                }

                constexpr explicit operator bool() const {
                    return impl != nullptr;
                }
            };

            bool add_resource(DnetCContext*, generic_resource);

            template <class T>
            typed_resource<T> make_resource() {
                return typed_resource<T>(new_from_global_heap<resource<T>>());
            }
        }  // namespace capi
    }      // namespace dnet
}  // namespace utils
