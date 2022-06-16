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
                const char* record;
                LookUpLevel level;
                IDPolicy policy;
                const char* assgin_context;
                Token parse(Input& input) {
                    Token err;
                    size_t offset_base = 0, offset = 0, index = 0;
                    String str;
                    auto pos = input.pos();
                    if (!read_utf_string(input, &err,
                                         default_utf_callback(check, str, offset_base, offset, index))) {
                        return err;
                    }
                    err = check.endok();
                    if (has_err(err)) {
                        return err;
                    }
                    err = Identifier<String>{std::move(str), pos};
                    if (auto sem = get_semantics(input)) {
                        if (policy == IDPolicy::not_exist_is_error) {
                            if (!sem->exists(record, err, level)) {
                                return simpleErrToken{"identifier not exists"};
                            }
                        }
                    }
                }
            };
        }  // namespace stream
    }      // namespace parser
}  // namespace utils
