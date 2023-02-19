/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// errcode - application defined error
#pragma once
namespace utils {
    namespace dnet {
        constexpr auto apperr = 1 << 29;
        constexpr auto invalid_async_head = apperr + 1;
        constexpr auto invalid_get_ptrs_result = apperr + 2;
        constexpr auto invalid_ol_ptr_position = apperr + 3;
        constexpr auto operation_imcomplete = apperr + 4;
        constexpr auto no_resource = apperr + 5;
        constexpr auto invalid_addr = apperr + 6;
        constexpr auto non_invalid_socket = apperr + 7;
        constexpr auto not_supported = apperr + 8;
        constexpr auto libload_failed = apperr + 9;
        constexpr auto invalid_tls = apperr + 10;
        constexpr auto should_setup_ssl = apperr + 11;
        constexpr auto set_ssl_value_failed = apperr + 12;
        constexpr auto ssl_blocking = apperr + 13;
        constexpr auto bio_operation_failed = apperr + 14;
        constexpr auto invalid_argument = apperr + 15;
        constexpr auto should_setup_quic = apperr + 16;
    }  // namespace dnet
}  // namespace utils
