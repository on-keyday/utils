/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>

namespace futils::jit {
    enum RelocationType {
        INITIAL_RIP,
        EPILOGUE_RIP,
        SAVE_RIP_RBP_RSP,
        RESET_RIP,
        SET_FIRST_ARG,
    };

    struct RelocationEntry {
        RelocationType type;
        std::uint64_t rewrite_offset = 0;
        std::uint64_t end_offset = 0;
        std::uint64_t address_offset = 0;
    };
}  // namespace futils::jit