/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// Code generated by ifacegen (https://github.com/on-keyday/utils)

#pragma once
#include <cstddef>
#include "../../helper/deref.h"
#include "flag.h"
#include "../../wrap/light/string.h"

#ifndef NOVTABLE__
#ifdef _WIN32
#define NOVTABLE__ __declspec(novtable)
#else
#define NOVTABLE__
#endif
#endif

namespace utils {
    namespace cmdline {
        namespace option {
            struct Value {
               private:
                struct NOVTABLE__ interface__ {
                    virtual const void* raw__(const std::type_info&) const noexcept = 0;
                    virtual const std::type_info& type__() const noexcept = 0;
                    virtual interface__* copy__(void* __storage_box) const = 0;
                    virtual interface__* move__(void* __storage_box) = 0;

                    virtual ~interface__() = default;
                };

                template <class T__>
                struct implements__ : interface__ {
                    T__ t_holder_;

                    template <class V__>
                    implements__(V__&& args)
                        : t_holder_(std::forward<V__>(args)) {}

                    const void* raw__(const std::type_info& info__) const noexcept override {
                        if (info__ != typeid(T__)) {
                            return nullptr;
                        }
                        return static_cast<const void*>(std::addressof(t_holder_));
                    }

                    const std::type_info& type__() const noexcept override {
                        return typeid(T__);
                    }

                    interface__* copy__(void* __storage_box) const override {
                        using gen_type = implements__<T__>;
                        if constexpr (sizeof(gen_type) <= sizeof(void*) * 2 &&
                                      alignof(gen_type) <= alignof(std::max_align_t) &&
                                      std::is_nothrow_move_constructible<T__>::value) {
                            return new (__storage_box) implements__<T__>(t_holder_);
                        }
                        else {
                            return new implements__<T__>(t_holder_);
                        }
                    }

                    interface__* move__(void* __storage_box) override {
                        using gen_type = implements__<T__>;
                        if constexpr (sizeof(gen_type) <= sizeof(void*) * 2 &&
                                      alignof(gen_type) <= alignof(std::max_align_t) &&
                                      std::is_nothrow_move_constructible<T__>::value) {
                            return new (__storage_box) implements__<T__>(std::move(t_holder_));
                        }
                        else {
                            return nullptr;
                        }
                    }
                };

                union {
                    char __storage_box[sizeof(void*) * (1 + (2))]{0};
                    std::max_align_t __align_of;
                    struct {
                        void* __place_holder[2];
                        interface__* iface;
                    };
                };

                template <class T__>
                void new___(T__&& v) {
                    interface__* p = nullptr;
                    using decay_T__ = std::decay_t<T__>;
                    using gen_type = implements__<decay_T__>;
                    if constexpr (sizeof(gen_type) <= sizeof(void*) * 2 &&
                                  alignof(gen_type) <= alignof(std::max_align_t) &&
                                  std::is_nothrow_move_constructible<decay_T__>::value) {
                        p = new (__storage_box) gen_type(std::forward<T__>(v));
                    }
                    else {
                        p = new gen_type(std::forward<T__>(v));
                    }
                    iface = p;
                }

                bool is_local___() const {
                    return static_cast<const void*>(__storage_box) == static_cast<const void*>(iface);
                }

                void delete___() {
                    if (!iface) return;
                    if (!is_local___()) {
                        delete iface;
                    }
                    else {
                        iface->~interface__();
                    }
                    iface = nullptr;
                }

               public:
                constexpr Value() {}

                constexpr Value(std::nullptr_t) {}

                template <class T__>
                Value(T__&& t) {
                    static_assert(!std::is_same<std::decay_t<T__>, Value>::value, "can't accept same type");
                    if (!utils::helper::deref(t)) {
                        return;
                    }
                    new___(std::forward<T__>(t));
                }

                Value(Value&& in) noexcept {
                    // reference implementation: MSVC std::function
                    if (in.is_local___()) {
                        iface = in.iface->move__(__storage_box);
                        in.delete___();
                    }
                    else {
                        iface = in.iface;
                        in.iface = nullptr;
                    }
                }

                Value& operator=(Value&& in) noexcept {
                    if (this == std::addressof(in)) return *this;
                    delete___();
                    // reference implementation: MSVC std::function
                    if (in.is_local___()) {
                        iface = in.iface->move__(__storage_box);
                        in.delete___();
                    }
                    else {
                        iface = in.iface;
                        in.iface = nullptr;
                    }
                    return *this;
                }

                explicit operator bool() const noexcept {
                    return iface != nullptr;
                }

                bool operator==(std::nullptr_t) const noexcept {
                    return iface == nullptr;
                }

                ~Value() {
                    delete___();
                }

                template <class T__>
                const T__* type_assert() const {
                    if (!iface) {
                        return nullptr;
                    }
                    return static_cast<const T__*>(iface->raw__(typeid(T__)));
                }

                template <class T__>
                T__* type_assert() {
                    if (!iface) {
                        return nullptr;
                    }
                    return static_cast<T__*>(const_cast<void*>(iface->raw__(typeid(T__))));
                }

                const std::type_info& type_id() const {
                    if (!iface) {
                        return typeid(void);
                    }
                    return iface->type__();
                }

                Value(const Value& in) {
                    if (in.iface) {
                        iface = in.iface->copy__(__storage_box);
                    }
                }

                Value& operator=(const Value& in) {
                    if (std::addressof(in) == this) return *this;
                    delete___();
                    if (in.iface) {
                        iface = in.iface->copy__(__storage_box);
                    }
                    return *this;
                }

                Value(Value& in)
                    : Value(const_cast<const Value&>(in)) {}

                Value& operator=(Value& in) {
                    return *this = const_cast<const Value&>(in);
                }
            };

            struct OptParser {
               private:
                struct NOVTABLE__ interface__ {
                    virtual bool parse(SafeVal<Value>& val, CmdParseState& ctx, bool reserved, size_t count) = 0;
                    virtual interface__* move__(void* __storage_box) = 0;

                    virtual ~interface__() = default;
                };

                template <class T__>
                struct implements__ : interface__ {
                    T__ t_holder_;

                    template <class V__>
                    implements__(V__&& args)
                        : t_holder_(std::forward<V__>(args)) {}

                    bool parse(SafeVal<Value>& val, CmdParseState& ctx, bool reserved, size_t count) override {
                        auto t_ptr_ = utils::helper::deref(this->t_holder_);
                        if (!t_ptr_) {
                            return bool{};
                        }
                        return t_ptr_->parse(val, ctx, reserved, count);
                    }

                    interface__* move__(void* __storage_box) override {
                        using gen_type = implements__<T__>;
                        if constexpr (sizeof(gen_type) <= sizeof(void*) * 3 &&
                                      alignof(gen_type) <= alignof(std::max_align_t) &&
                                      std::is_nothrow_move_constructible<T__>::value) {
                            return new (__storage_box) implements__<T__>(std::move(t_holder_));
                        }
                        else {
                            return nullptr;
                        }
                    }
                };

                union {
                    char __storage_box[sizeof(void*) * (1 + (3))]{0};
                    std::max_align_t __align_of;
                    struct {
                        void* __place_holder[3];
                        interface__* iface;
                    };
                };

                template <class T__>
                void new___(T__&& v) {
                    interface__* p = nullptr;
                    using decay_T__ = std::decay_t<T__>;
                    using gen_type = implements__<decay_T__>;
                    if constexpr (sizeof(gen_type) <= sizeof(void*) * 3 &&
                                  alignof(gen_type) <= alignof(std::max_align_t) &&
                                  std::is_nothrow_move_constructible<decay_T__>::value) {
                        p = new (__storage_box) gen_type(std::forward<T__>(v));
                    }
                    else {
                        p = new gen_type(std::forward<T__>(v));
                    }
                    iface = p;
                }

                bool is_local___() const {
                    return static_cast<const void*>(__storage_box) == static_cast<const void*>(iface);
                }

                void delete___() {
                    if (!iface) return;
                    if (!is_local___()) {
                        delete iface;
                    }
                    else {
                        iface->~interface__();
                    }
                    iface = nullptr;
                }

               public:
                constexpr OptParser() {}

                constexpr OptParser(std::nullptr_t) {}

                template <class T__>
                OptParser(T__&& t) {
                    static_assert(!std::is_same<std::decay_t<T__>, OptParser>::value, "can't accept same type");
                    if (!utils::helper::deref(t)) {
                        return;
                    }
                    new___(std::forward<T__>(t));
                }

                OptParser(OptParser&& in) noexcept {
                    // reference implementation: MSVC std::function
                    if (in.is_local___()) {
                        iface = in.iface->move__(__storage_box);
                        in.delete___();
                    }
                    else {
                        iface = in.iface;
                        in.iface = nullptr;
                    }
                }

                OptParser& operator=(OptParser&& in) noexcept {
                    if (this == std::addressof(in)) return *this;
                    delete___();
                    // reference implementation: MSVC std::function
                    if (in.is_local___()) {
                        iface = in.iface->move__(__storage_box);
                        in.delete___();
                    }
                    else {
                        iface = in.iface;
                        in.iface = nullptr;
                    }
                    return *this;
                }

                explicit operator bool() const noexcept {
                    return iface != nullptr;
                }

                bool operator==(std::nullptr_t) const noexcept {
                    return iface == nullptr;
                }

                ~OptParser() {
                    delete___();
                }

                bool parse(SafeVal<Value>& val, CmdParseState& ctx, bool reserved, size_t count) {
                    return iface ? iface->parse(val, ctx, reserved, count) : bool{};
                }

                OptParser(const OptParser&) = delete;

                OptParser& operator=(const OptParser&) = delete;

                OptParser(OptParser&) = delete;

                OptParser& operator=(OptParser&) = delete;
            };

        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
