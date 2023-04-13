/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <cstdint>
#include "../../core/sequencer.h"

namespace utils::minilang::comb {
    enum class CombMode {
        seq,         // sequencial
        repeat,      // repeat
        branch,      // or
        optional,    // optional
        must_match,  // must match
        group,       // group
        other,
    };

    constexpr const char* to_string(CombMode m) noexcept {
        switch (m) {
            case CombMode::seq:
                return "seq";
            case CombMode::repeat:
                return "repeat";
            case CombMode::branch:
                return "branch";
            case CombMode::optional:
                return "optional";
            case CombMode::must_match:
                return "must_match";
            default:
                return "unknown";
        }
    }

    enum class CombStatus {
        match,
        not_match,
        fatal,
    };

    constexpr const char* to_string(CombStatus s) noexcept {
        switch (s) {
            case CombStatus::match:
                return "match";
            case CombStatus::not_match:
                return "not_match";
            case CombStatus::fatal:
                return "fatal";
            default:
                return "unknown";
        }
    }

    enum class CombState {
        match,
        other_branch,
        not_match,
        ignore_not_match,
        repeat_match,
        fatal,
    };

    constexpr const char* to_string(CombState s) noexcept {
        switch (s) {
            case CombState::match:
                return "match";
            case CombState::other_branch:
                return "other_branch";
            case CombState::not_match:
                return "not_match";
            case CombState::ignore_not_match:
                return "ignore_not_match";
            case CombState::repeat_match:
                return "repeat_match";
            case CombState::fatal:
                return "fatal";
            default:
                return "unknown";
        }
    }

    struct Pos {
        size_t begin = 0;
        size_t end = 0;
    };

    struct NullCtx {
        constexpr void branch(CombMode, std::uint64_t*) {}
        constexpr void commit(CombMode, std::uint64_t, CombState) {}

        // constant token
        constexpr void ctoken(const char*, Pos) {}
        // conditional range
        template <class T>
        constexpr void crange(auto, Pos, Sequencer<T>&) {}

        constexpr void group(auto tag, std::uint64_t*) {}
        constexpr void group_end(auto tag, std::uint64_t, CombState) {}
    };

    struct CounterCtx : NullCtx {
        std::uint64_t bid = 0;
        constexpr void branch(CombMode, std::uint64_t* id) {
            *id = bid;
            bid++;
        }
        constexpr void group(auto tag, std::uint64_t* id) {
            *id = bid;
            bid++;
        }
    };

    auto convert_tag_to_string(auto tag) {
        if constexpr (std::is_same_v<decltype(tag), const char*>) {
            return tag;
        }
        else {
            return to_string(tag);
        }
    }

    template <class C, class... Args>
    concept has_report = requires(C c, Args... a) {
                             { c.report(a...) };
                         };

    constexpr void report_error(auto&& c, auto&&... err) {
        if constexpr (has_report<decltype(c), decltype(err)...>) {
            c.report(err...);
        }
    }

}  // namespace utils::minilang::comb
