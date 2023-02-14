/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error - error type
#pragma once
#include "dll/dllh.h"
#include "dll/glheap.h"
#include <concepts>
#include "../view/iovec.h"
#include "../helper/defer.h"
#include "../helper/pushbacker.h"
#include "../helper/appender.h"
#include "../number/to_string.h"

namespace utils {
    namespace dnet::error {
        enum class ErrorType : byte {
            null,
            memexh,
            number,
            str,
            wrap,
        };

        enum class ErrorCategory {
            noerr = 0,
            syserr = 0x1,
            liberr = 0x100,
            dneterr = 0x101,
            cryptoerr = 0x102,
            quicerr = 0x103,
            sslerr = 0x104,
            apperr = 0x1000,
            internalerr = 0x10000,
            validationerr = 0x10001,
        };

        constexpr const char* categoly_string(ErrorCategory categ) {
            switch (categ) {
                case ErrorCategory::syserr:
                    return "system";
                case ErrorCategory::liberr:
                    return "lib";
                case ErrorCategory::dneterr:
                    return "libdnet";
                case ErrorCategory::cryptoerr:
                    return "libcrypto";
                case ErrorCategory::sslerr:
                    return "libssl";
                case ErrorCategory::quicerr:
                    return "quic";
                case ErrorCategory::apperr:
                    return "application";
                case ErrorCategory::internalerr:
                    return "internal";
                case ErrorCategory::validationerr:
                    return "validation";
                default:
                    return nullptr;
            }
        }

        struct Error;

        struct ErrorTraits {
            bool has_category = false;
            bool has_equal = false;
            bool has_errnum = false;
            bool has_unwrap = false;
        };

        enum class NumErrMode {
            // print "code=val,categoly=categ"
            use_default,
            // print "code=val,categoly="
            use_code,
            // print ""
            use_nothing,
        };

        dnet_dll_export(bool) register_categspec_nummsg(ErrorCategory categ, NumErrMode mode, void (*fn)(helper::IPushBacker<> pb, std::uint64_t code));

        namespace internal {
            dnet_dll_export(void) invoke_categspec(helper::IPushBacker<> pb, ErrorCategory categ, std::uint64_t val);
            dnet_dll_export(NumErrMode) categspec_mode(ErrorCategory categ);

            constexpr void categ_spec_error(auto& out, ErrorCategory categ, std::uint64_t val) {
                if (!std::is_constant_evaluated()) {
                    invoke_categspec(out, categ, val);
                }
            }

            constexpr NumErrMode categ_spec_mode(ErrorCategory categ) {
                if (std::is_constant_evaluated()) {
                    return NumErrMode::use_default;
                }
                return categspec_mode(categ);
            }

            template <class T>
            concept has_error = requires(T t) {
                                    { t.error(std::declval<helper::IPushBacker<>>()) };
                                };

            template <class T>
            concept has_category = requires(T t) {
                                       { t.category() } -> std::same_as<ErrorCategory>;
                                   };

            template <class T>
            concept has_errnum = requires(T t) {
                                     { t.errnum() } -> std::convertible_to<std::uint64_t>;
                                 };

            template <class T>
            concept has_equal = requires(const T& t) {
                                    { t == t } -> std::convertible_to<bool>;
                                };

            template <class T>
            concept has_unwrap = requires(T t) {
                                     { t.unwrap() } -> std::same_as<Error>;
                                 };

            enum class WrapCtrlOrder {
                call,
                del,
                ref,
                category,
                unwrap,
                reflect,
                errnum,
                traits,
            };

            template <class T>
            auto eqfn() {
                return [](const T* p1, const T* p2) {
                    if constexpr (has_equal<decltype(p1->fn)>) {
                        return p1->fn == p2->fn;
                    }
                    return false;
                };
            }

            struct Wrap {
                void* ptr = nullptr;
                std::uintptr_t (*error)(void*, helper::IPushBacker<> pb, WrapCtrlOrder, void* unwrap_s) = nullptr;
                bool (*cmp)(const void*, const void*) = nullptr;
                constexpr Wrap() {}

                template <class T>
                Wrap(T&& t) {
                    auto alloc = [](size_t size, size_t align) {
                        return alloc_normal(size, align, DNET_DEBUG_MEMORY_LOCINFO(true, size, align));
                    };
                    auto del = [](void* p) {
                        free_normal(p, DNET_DEBUG_MEMORY_LOCINFO(false, 0, 0));
                    };
                    auto tmp = helper::wrapfn_ptr<std::atomic_uint32_t>(
                        std::forward<decltype(t)>(t),
                        alloc, del);
                    if (!tmp) {
                        return;
                    }
                    auto cmpf = +eqfn<std::decay_t<decltype(*tmp)>>();
                    ptr = tmp;
                    std::get<0>(tmp->optional) = 1;
                    cmp = reinterpret_cast<bool (*)(const void*, const void*)>(cmpf);
                    error = [](void* p, helper::IPushBacker<> pb, WrapCtrlOrder ord, void* unwrap_s) -> std::uintptr_t {
                        auto f = decltype(tmp)(p);
                        if (ord == WrapCtrlOrder::ref) {
                            ++std::get<0>(f->optional);
                            return 0;
                        }
                        if (ord == WrapCtrlOrder::del) {
                            if (--std::get<0>(f->optional) == 0) {
                                delete_with_global_heap(f, DNET_DEBUG_MEMORY_LOCINFO(true, sizeof(decltype(*f)), alignof(decltype(*f))));
                            }
                            return 0;
                        }
                        if (ord == WrapCtrlOrder::category) {
                            if constexpr (has_category<decltype(f->fn)>) {
                                return std::uintptr_t(f->fn.category());
                            }
                            return std::uintptr_t(ErrorCategory::apperr);
                        }
                        if (ord == WrapCtrlOrder::unwrap) {
                            if constexpr (has_unwrap<decltype(f->fn)>) {
                                auto& w = *(Error*)unwrap_s;
                                w = f->fn.unwrap();
                            }
                            return 0;
                        }
                        if (ord == WrapCtrlOrder::reflect) {
                            return std::uintptr_t(std::addressof(f->fn));
                        }
                        if (ord == WrapCtrlOrder::errnum) {
                            if constexpr (has_errnum<decltype(f->fn)>) {
                                return f->fn.errnum();
                            }
                            return 0;
                        }
                        if (ord == WrapCtrlOrder::traits) {
                            ErrorTraits& attr = *(ErrorTraits*)unwrap_s;
                            attr.has_equal = has_equal<decltype(f->fn)>;
                            attr.has_category = has_category<decltype(f->fn)>;
                            attr.has_errnum = has_errnum<decltype(f->fn)>;
                            attr.has_unwrap = has_unwrap<decltype(f->fn)>;
                            return 0;
                        }
                        f->fn.error(pb);
                        return 0;
                    };
                }

                constexpr ~Wrap() {
                    if (error) {
                        helper::NopPushBacker nop{};
                        error(ptr, nop, WrapCtrlOrder::del, nullptr);
                    }
                }
            };
            template <class T>
            struct WithCateg {
                T value;
                ErrorCategory categ;
            };

            union ErrType {
                WithCateg<view::basic_rvec<char, byte>> str;
                WithCateg<std::uint64_t> code;
                Wrap w;
                constexpr ~ErrType() {}
            };

            struct Memexh {};
            struct None {
                constexpr operator bool() const {
                    return false;
                }
            };
        }  // namespace internal

        /* Error is golang like error interface
           object should implement void error(helper::IPushBacker<>)
           specialized to number,const char*,and char* (heap str)
           support Error copy by reference count of internal pointer
           also support Error unwrap() like golang errors.Unwrap()
           reflection like T* as() function also support

           user defined Error MUST implement
           + void error(helper::IPushbacker<>)
           user defined Error MAY implement
           + error::Error unwrap()
           + ErrorCategory category()
           + std::uint64_t errnum()
        */
        struct Error {
           private:
            internal::ErrType detail{};
            ErrorType type_ = ErrorType::null;

            constexpr void move(Error&& from) {
                if (from.type_ == ErrorType::null || from.type_ == ErrorType::memexh) {
                    type_ = from.type_;
                }
                else if (from.type_ == ErrorType::number) {
                    detail.code = std::exchange(from.detail.code, internal::WithCateg<std::uint64_t>{});
                    type_ = ErrorType::number;
                }
                else if (from.type_ == ErrorType::str) {
                    detail.str = std::exchange(from.detail.str, internal::WithCateg<view::basic_rvec<char, byte>>{});
                    type_ = ErrorType::str;
                }
                else if (from.type_ == ErrorType::wrap) {
                    detail.w.ptr = std::exchange(from.detail.w.ptr, nullptr);
                    detail.w.error = std::exchange(from.detail.w.error, nullptr);
                    detail.w.cmp = std::exchange(from.detail.w.cmp, nullptr);
                    type_ = ErrorType::wrap;
                }
                from.type_ = ErrorType::null;
            }

            void copy(const Error& from) {
                if (from.type_ == ErrorType::null || from.type_ == ErrorType::memexh) {
                    type_ = from.type_;
                }
                else if (from.type_ == ErrorType::number) {
                    detail.code = from.detail.code;
                    type_ = ErrorType::number;
                }
                else if (from.type_ == ErrorType::str) {
                    detail.str = from.detail.str;
                    type_ = ErrorType::str;
                }
                else if (from.type_ == ErrorType::wrap) {
                    helper::NopPushBacker nop;
                    from.detail.w.error(from.detail.w.ptr, nop, internal::WrapCtrlOrder::ref, nullptr);
                    detail.w.ptr = from.detail.w.ptr;
                    detail.w.error = from.detail.w.error;
                    detail.w.cmp = from.detail.w.cmp;
                    type_ = ErrorType::wrap;
                }
            }

            constexpr void destruct() {
                if (type_ == ErrorType::wrap) {
                    detail.w.~Wrap();
                }
                type_ = ErrorType::null;
            }

           public:
            constexpr Error() = default;

            constexpr Error(const internal::Memexh&)
                : type_(ErrorType::memexh) {}

            constexpr Error(const internal::None&)
                : Error() {}

            constexpr Error(Error&& from) {
                move(std::move(from));
            }

            Error(const Error& from) {
                copy(from);
            }

            constexpr Error& operator=(Error&& from) {
                if (this == &from) {
                    return *this;
                }
                destruct();
                move(std::move(from));
                return *this;
            }

            Error& operator=(const Error& from) {
                if (this == &from) {
                    return *this;
                }
                destruct();
                copy(from);
                return *this;
            }

            constexpr explicit Error(std::uint64_t code, ErrorCategory categ = ErrorCategory::apperr)
                : type_(ErrorType::number) {
                detail.code.value = code;
                detail.code.categ = categ;
            }

            constexpr explicit Error(const char* data, ErrorCategory categ = ErrorCategory::apperr, std::int64_t len = -1)
                : type_(ErrorType::str) {
                detail.str.value = view::basic_rvec<char, byte>(data);
                detail.str.categ = categ;
            }

            template <class T>
                requires internal::has_error<T> && (!std::is_same_v<std::decay_t<T>, Error>)
            Error(T&& t)
                : type_(ErrorType::wrap) {
                new (&detail.w) internal::Wrap(std::forward<decltype(t)>(t));
                if (!detail.w.error) {
                    type_ = ErrorType::memexh;
                }
            }

            constexpr ErrorType type() const {
                return type_;
            }

            template <class T>
            constexpr void error(T&& t) const {
                if (type_ == ErrorType::null) {
                    helper::append(t, "<null>");
                }
                else if (type_ == ErrorType::memexh) {
                    helper::append(t, "!<memexh>");
                }
                else if (type_ == ErrorType::str) {
                    helper::append(t, detail.str.value);
                }
                else if (type_ == ErrorType::number) {
                    auto mode = internal::categ_spec_mode(detail.code.categ);
                    if (mode != NumErrMode::use_nothing) {
                        helper::append(t, "code=");
                        number::to_string(t, detail.code.value);
                        helper::append(t, ",categoly=");
                        if (mode == NumErrMode::use_default) {
                            if (auto msg = categoly_string(detail.code.categ)) {
                                helper::append(t, msg);
                            }
                            else {
                                number::to_string(t, size_t(detail.code.categ));
                            }
                        }
                    }
                    internal::categ_spec_error(t, detail.code.categ, detail.code.value);
                }
                else if (type_ == ErrorType::wrap) {
                    detail.w.error(detail.w.ptr, t, internal::WrapCtrlOrder::call, nullptr);
                }
            }

            template <class T>
            constexpr T error() const {
                T t;
                error(t);
                return t;
            }

            constexpr ErrorCategory category() const {
                if (type_ == ErrorType::number) {
                    return detail.code.categ;
                }
                if (type_ == ErrorType::str) {
                    return detail.str.categ;
                }
                if (type_ == ErrorType::memexh) {
                    return ErrorCategory::syserr;
                }
                if (type_ == ErrorType::null) {
                    return ErrorCategory::noerr;
                }
                if (type_ == ErrorType::wrap) {
                    helper::NopPushBacker nop;
                    auto res = detail.w.error(detail.w.ptr, nop, internal::WrapCtrlOrder::category, nullptr);
                    return ErrorCategory(res);
                }
                return ErrorCategory::apperr;
            }

            constexpr std::uint64_t errnum() const {
                if (type_ == ErrorType::number) {
                    return detail.code.value;
                }
                if (type_ == ErrorType::wrap) {
                    helper::NopPushBacker nop;
                    return detail.w.error(detail.w.ptr, nop, internal::WrapCtrlOrder::errnum, nullptr);
                }
                return 0;
            }

            constexpr bool is_error() const {
                return type_ != ErrorType::null;
            }

            constexpr bool is_noerr() const {
                return !is_error();
            }

            constexpr explicit operator bool() const {
                return is_error();
            }

            constexpr friend bool operator==(const Error& left, const Error& right) {
                if (left.type_ != right.type_) {
                    return false;
                }
                if (left.type_ == ErrorType::null || left.type_ == ErrorType::memexh) {
                    return true;
                }
                if (left.type_ == ErrorType::number) {
                    return left.detail.code.value == right.detail.code.value &&
                           left.detail.code.categ == right.detail.code.categ;
                }
                if (left.type_ == ErrorType::str) {
                    if (left.detail.str.categ != right.detail.str.categ) {
                        return false;
                    }
                    return left.detail.str.value == right.detail.str.value;
                }
                if (left.type_ == ErrorType::wrap) {
                    if (left.detail.w.cmp != right.detail.w.cmp) {
                        return false;
                    }
                    if (left.detail.w.ptr == right.detail.w.ptr) {
                        return true;
                    }
                    return left.detail.w.cmp(left.detail.w.ptr, right.detail.w.ptr);
                }
                return false;
            }

            constexpr Error unwrap() const {
                if (type_ == ErrorType::wrap) {
                    Error err;
                    helper::NopPushBacker nop;
                    detail.w.error(detail.w.ptr, nop, internal::WrapCtrlOrder::unwrap, &err);
                    return err;
                }
                return Error();
            }

            constexpr ~Error() {
                destruct();
            }

            template <class T>
            T* as() const {
                if (type_ == ErrorType::wrap) {
                    using WrapT = helper::WrapPtr<T, std::atomic_uint32_t>;
                    auto cmpf = reinterpret_cast<bool (*)(const void*, const void*)>(+internal::eqfn<WrapT>());
                    if (detail.w.cmp == cmpf) {
                        helper::NopPushBacker nop;
                        auto void_v = reinterpret_cast<void*>(detail.w.error(detail.w.ptr, nop, internal::WrapCtrlOrder::reflect, nullptr));
                        return static_cast<T*>(void_v);
                    }
                }
                return nullptr;
            }

            constexpr ErrorTraits traits() const {
                if (type_ == ErrorType::null || type_ == ErrorType::memexh ||
                    type_ == ErrorType::str) {
                    return ErrorTraits{.has_category = true, .has_equal = true};
                }
                if (type_ == ErrorType::number) {
                    return ErrorTraits{.has_category = true, .has_equal = true, .has_errnum = true};
                }
                if (type_ == ErrorType::wrap) {
                    ErrorTraits trait;
                    helper::NopPushBacker nop;
                    detail.w.error(detail.w.ptr, nop, internal::WrapCtrlOrder::traits, &trait);
                    return trait;
                }
                return {};
            }
        };

        constexpr auto none = internal::None{};

        static_assert(Error(none).type() == ErrorType::null);

        constexpr auto eof = Error("EOF", ErrorCategory::syserr);

        // memory_exhausted is an emrgency error
        // avoid heap allocation and free memory if this occurred.
        // this is common and exceptional error in program so that this has special ErrorType.
        // if memory exhausting is occurred while Error object construction,
        // Error object will replaced with this
        constexpr auto memory_exhausted = internal::Memexh{};

        constexpr auto block = Error("BLOCK", ErrorCategory::syserr);

        constexpr auto unimplemented = Error("UNIMPLEMENTED", ErrorCategory::dneterr);

    }  // namespace dnet::error
}  // namespace utils
