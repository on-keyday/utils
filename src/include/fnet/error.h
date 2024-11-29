/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
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

namespace futils {
    namespace fnet {

        namespace error {

            enum class NumErrMode {
                // print "code=val,category=category"
                use_default,
                // print custom message
                use_custom,
            };

            fnet_dll_export(bool) register_categspec_nummsg(futils::error::Category categ, NumErrMode mode, void (*fn)(helper::IPushBacker<> pb, std::uint64_t code));

            namespace internal {
                fnet_dll_export(void) invoke_categspec(helper::IPushBacker<> pb, futils::error::Category categ, std::uint64_t val);
                fnet_dll_export(NumErrMode) categspec_mode(futils::error::Category categ);

                constexpr void categ_spec_error(auto& out, futils::error::Category categ, std::uint64_t val) {
                    if (!std::is_constant_evaluated()) {
                        invoke_categspec(out, categ, val);
                    }
                }

                constexpr NumErrMode categ_spec_mode(futils::error::Category categ) {
                    if (std::is_constant_evaluated()) {
                        return NumErrMode::use_default;
                    }
                    return categspec_mode(categ);
                }
            }  // namespace internal

            using Category = futils::error::Category;

            struct FormatTraits : futils::error::DefaultFormatTraits<std::uint32_t> {
                static constexpr void num_error(auto&& t, std::uint64_t val, Category c, std::uint32_t s) {
                    auto mode = internal::categ_spec_mode(c);
                    if (mode == NumErrMode::use_default) {
                        futils::error::DefaultFormatTraits<std::uint32_t>::num_error(t, val, c, s);
                        return;
                    }
                    internal::categ_spec_error(t, c, val);
                }
            };

            using Error = futils::error::Error<glheap_allocator<byte>, std::string, std::uint32_t, FormatTraits>;

            using ErrorType = futils::error::ErrorType;

            constexpr auto none = Error();

            static_assert(Error(none).type() == futils::error::ErrorType::null);

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

            static_assert(eof == eof);

            // memory_exhausted is an emergency error
            // avoid heap allocation and free memory if this occurred.
            // this is common and exceptional error in program so that this has special ErrorType.
            // if memory exhausting is occurred while Error object construction,
            // Error object will replaced with this
            constexpr auto memory_exhausted = Error(Category::bad_alloc);

            constexpr auto block = Error("BLOCK", Category::lib, fnet_generic_error);

            fnet_dll_export(Error) Errno();

            constexpr size_t sizeof_error = sizeof(Error);

            using ErrList = futils::error::ErrList<Error>;

        }  // namespace error
        template <class T>
        using expected = helper::either::expected<T, error::Error>;

        constexpr auto unexpect(auto&&... a) {
            return helper::either::unexpected<error::Error>(std::in_place, std::forward<decltype(a)>(a)...);
        }

        constexpr auto callback_with_error(auto&& f, auto&&... args) {
            if constexpr (std::is_void_v<decltype(f(std::forward<decltype(args)>(args)...))>) {
                f(std::forward<decltype(args)>(args)...);
                return expected<void>();
            }
            else if constexpr (helper::is_template_instance_of<decltype(f(std::forward<decltype(args)>(args)...)), helper::either::expected>) {
                return f(std::forward<decltype(args)>(args)...);
            }
            else if constexpr (std::is_convertible_v<decltype(f(std::forward<decltype(args)>(args)...)), error::Error>) {
                if (auto err = f(std::forward<decltype(args)>(args)...)) {
                    return expected<void>(unexpect(err));
                }
                return expected<void>();
            }
        }

    }  // namespace fnet
}  // namespace futils
