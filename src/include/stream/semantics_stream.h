/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// semantics_stream - token with semantics stream
#pragma once
#include "utf_stream.h"

namespace utils {
    namespace parser {
        namespace stream {
            enum LookUpLevel {
                level_current,
                level_function,
                level_global,
                level_all,
            };

            constexpr auto tokenIdent = "identifier";

            template <class String>
            struct Identifier {
                String str;
                size_t pos;
                void token(PB pb) {
                    helper::append(pb, str);
                }
                TokenInfo info() {
                    TokenInfo info{};
                    info.kind = tokenIdent;
                }
            };

            enum class IDPolicy {
                not_exist_is_error,
                assignment_makes_new,
                anyway_create,
            };

            template <class String, class Check>
            struct IdentifierParser {
                Check check;
                Token parse(Input& input) {
                    String str;
                    auto pos = input.pos();
                    auto err = read_default_utf(input, str, check);
                    if (has_err(err)) {
                        return err;
                    }
                    err = Identifier<String>{std::move(str), pos};
                    // TODO(on-keyday): add semantics rule
                    return err;
                }
            };

            template <class String, class Check>
            auto make_ident(Check check) {
                return IdentifierParser<String, Check>{std::move(check)};
            }
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
