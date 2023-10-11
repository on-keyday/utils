/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// error - error type
#pragma once
#include "dll/dllh.h"
#include <error/error.h>
#include "dll/allocator.h"
#include <string>
#include <helper/expected.h>

namespace utils {
    namespace fnet {

        namespace error {
            /*
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
                fneterr = 0x101,
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
                    case ErrorCategory::fneterr:
                        return "libfnet";
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
            */

            enum class NumErrMode {
                // print "code=val,category=category"
                use_default,
                // print "code=val,category="
                use_code,
                // print ""
                use_nothing,
            };

            fnet_dll_export(bool) register_categspec_nummsg(utils::error::Category categ, NumErrMode mode, void (*fn)(helper::IPushBacker<> pb, std::uint64_t code));

            namespace internal {
                fnet_dll_export(void) invoke_categspec(helper::IPushBacker<> pb, utils::error::Category categ, std::uint64_t val);
                fnet_dll_export(NumErrMode) categspec_mode(utils::error::Category categ);

                constexpr void categ_spec_error(auto& out, utils::error::Category categ, std::uint64_t val) {
                    if (!std::is_constant_evaluated()) {
                        invoke_categspec(out, categ, val);
                    }
                }

                constexpr NumErrMode categ_spec_mode(utils::error::Category categ) {
                    if (std::is_constant_evaluated()) {
                        return NumErrMode::use_default;
                    }
                    return categspec_mode(categ);
                }
            }  // namespace internal

            /*
            namespace internal {

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

                template <class Obj>
                struct WrapManage {
                    Obj fn;
                    std::atomic_uint32_t ref = 1;
                };

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
                        auto tmp = alloc(sizeof(WrapManage<std::decay_t<T>>), alignof(WrapManage<std::decay_t<T>>));
                        if (!tmp) {
                            return;
                        }
                        auto d = helper::defer([&] {
                            del(tmp);
                        });
                        auto obj = new (tmp) WrapManage<std::decay_t<T>>{std::forward<T>(t)};
                        auto cmpf = +eqfn<std::decay_t<decltype(*obj)>>();
                        ptr = tmp;
                        d.cancel();
                        cmp = reinterpret_cast<bool (*)(const void*, const void*)>(cmpf);
                        error = [](void* p, helper::IPushBacker<> pb, WrapCtrlOrder ord, void* unwrap_s) -> std::uintptr_t {
                            WrapManage<std::decay_t<T>>* f = static_cast<WrapManage<std::decay_t<T>>*>(p);
                            if (ord == WrapCtrlOrder::ref) {
                                ++f->ref;
                                return 0;
                            }
                            if (ord == WrapCtrlOrder::del) {
                                if (--f->ref == 0) {
                                    delete_glheap(f);
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
                    WithCateg<view::basic_rvec<char>> str;
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

            * Error is golang like error interface
               object should implement void error(helper::IPushBacker<>)
               specialized to number and const char*
               support Error copy by reference count of internal pointer
               also support Error unwrap() like golang errors.Unwrap()
               reflection like T* as() function also support

               user defined Error MUST implement
               + void error(helper::IPushbacker<>)
               user defined Error MAY implement
               + error::Error unwrap()
               + ErrorCategory category()
               + std::uint64_t errnum()
            *
            struct Error {
                using error_buffer_type = std::basic_string<char, std::char_traits<char>, glheap_allocator<char>>;

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
                        detail.str = std::exchange(from.detail.str, internal::WithCateg<view::basic_rvec<char>>{});
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
                    detail.str.value = view::basic_rvec<char>(data);
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
                        strutil::append(t, "<null>");
                    }
                    else if (type_ == ErrorType::memexh) {
                        strutil::append(t, "!<memexh>");
                    }
                    else if (type_ == ErrorType::str) {
                        strutil::append(t, detail.str.value);
                    }
                    else if (type_ == ErrorType::number) {
                        auto mode = internal::categ_spec_mode(detail.code.categ);
                        if (mode != NumErrMode::use_nothing) {
                            strutil::append(t, "code=");
                            number::to_string(t, detail.code.value);
                            strutil::append(t, ",categoly=");
                            if (mode == NumErrMode::use_default) {
                                if (auto msg = categoly_string(detail.code.categ)) {
                                    strutil::append(t, msg);
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
                        using WrapT = internal::WrapManage<T>;
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
            */

            using Error = utils::error::Error<glheap_allocator<byte>, std::string>;

            using Category = utils::error::Category;
            using ErrorType = utils::error::ErrorType;

            constexpr auto none = Error();

            static_assert(Error(none).type() == utils::error::ErrorType::null);

            enum {
                fnet_generic_error = 0x100,
                fnet_address_error = 0x101,
                fnet_network_error = 0x102,
                fnet_async_error = 0x103,
                fnet_usage_error = 0x104,
                fnet_lib_load_error = 0x105,

                fnet_quic_error = 0x1000,
                fnet_quic_transport_error = 0x1001,
                fnet_quic_version_error = 0x1002,
                fnet_quic_implementation_bug = 0x1003,
                fnet_quic_user_arg_error = 0x1004,

                fnet_quic_crypto_arg_error = 0x1102,
                fnet_quic_crypto_op_error = 0x1103,

                fnet_quic_packet_number_error = 0x1201,
                fnet_quic_packet_number_decode_error = 0x1202,
                fnet_quic_packet_number_encode_error = 0x1203,

                fnet_quic_packet_error = 0x1301,

                fnet_quic_connection_id_error = 0x1401,

                fnet_quic_stream_error = 0x1501,

                fnet_quic_transport_parameter_error = 0x1601,

                fnet_tls_error = 0x2000,
                fnet_tls_SSL_library_error = 0x2001,
                fnet_tls_implementation_bug = 0x2002,
                fnet_tls_not_supported = 0x2003,
                fnet_tls_usage_error = 0x2004,
                fnet_tls_lib_type_error = 0x2005,

                fnet_ip_error = 0x3000,
                fnet_ip_header_error = 0x3001,
                fnet_ip_checksum_error = 0x3002,

                fnet_icmp_error = 0x4000,

            };

            constexpr auto eof = Error("EOF", Category::lib, fnet_generic_error);

            // memory_exhausted is an emrgency error
            // avoid heap allocation and free memory if this occurred.
            // this is common and exceptional error in program so that this has special ErrorType.
            // if memory exhausting is occurred while Error object construction,
            // Error object will replaced with this
            constexpr auto memory_exhausted = Error(Category::bad_alloc);

            constexpr auto block = Error("BLOCK", Category::lib, fnet_generic_error);

            fnet_dll_export(Error) Errno();

            constexpr size_t sizeof_error = sizeof(Error);

        }  // namespace error
        template <class T>
        using expected = helper::either::expected<T, error::Error>;

        constexpr auto unexpect(auto&&... a) {
            return helper::either::unexpected<error::Error>(std::in_place, std::forward<decltype(a)>(a)...);
        }

    }  // namespace fnet
}  // namespace utils
