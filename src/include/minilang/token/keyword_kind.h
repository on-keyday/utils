/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// keyword_kind
#pragma once
#include "sort_internal.h"

namespace utils {
    namespace minilang::token {
        enum class KeywordKind {
            as_,
            if_,
            for_,
            while_,
            match_,
            const_,
            let_,
            var_,
            global_,
            static_,
            thread_local_,
            mut_,
            fn_,
            break_,
            continue_,
            return_,
            import_,
            export_,
            yield_,
            async_,
            await_,
            go_,
            goto_,
            loop_,
            chan_,
            trait_,
            interface_,
            class_,
            struct_,
            extends_,
            throw_,
            type_,
            catch_,
            raise_,
            except_,
            func_,
            extern_,
            end_of_mapping,
        };

        constexpr const char* keyword_mapping[int(KeywordKind::end_of_mapping)] = {
            "as",
            "if",
            "for",
            "while",
            "match",
            "const",
            "let",
            "var",
            "global",
            "static",
            "thread_local",
            "mut",
            "fn",
            "break",
            "continue",
            "return",
            "import",
            "export",
            "yield",
            "async",
            "await",
            "go",
            "goto",
            "loop",
            "chan",
            "trait",
            "interface",
            "class",
            "struct",
            "extends",
            "throw",
            "type",
            "catch",
            "raise",
            "except",
            "func",
            "extern",
        };

        static_assert(internal::check_length(keyword_mapping), "mapping failed");

        constexpr auto apply_sorted_keywords(auto&& cb) {
            return internal::apply<KeywordKind, int(KeywordKind::end_of_mapping)>(cb, keyword_mapping);
        }

        constexpr auto keywords_tuple = apply_sorted_keywords([](auto&&... p) {
            return std::make_tuple(p...);
        });

    }  // namespace minilang::token
}  // namespace utils
