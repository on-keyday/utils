/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// ssa - static single assignment form
#pragma once
#include "../token/token.h"
#include <list>
#include "../../number/to_string.h"

namespace utils {
    namespace minilang::ssa {
        enum class Kind {};

        enum class Simplity {
            unknown,     // unknown
            constant,    // constant (known at compile time)
            runtime,     // runtime value
            control,     // control structure
            definition,  // definition
        };

        struct Istr {
            const Kind kind;
            Simplity simplity;

           protected:
            Istr(Kind k, Simplity simp = Simplity::unknown)
                : kind(k), simplity(simp) {}
        };

        constexpr Kind make_user_defined_kind(size_t i) {
            return Kind(i);
        }

        template <class T, Kind kind>
        constexpr auto make_to_Istr() {
            return [](const std::shared_ptr<Istr>& istr) -> std::shared_ptr<T> {
                if (istr && istr->kind == kind) {
                    return std::static_pointer_cast<T>(istr);
                }
                return nullptr;
            };
        }

        struct Value {
            std::string name;
            std::shared_ptr<Istr> istr;
        };

        // binary operator
        struct BinOp : Istr {
            std::shared_ptr<Value> left;
            std::shared_ptr<Value> right;

           protected:
            using Istr::Istr;
        };

        // unary operator
        struct Primitive : Istr {
           protected:
            using Istr::Istr;
        };

        // unary operator
        struct UnaryOp : Istr {
            std::shared_ptr<Value> target;

           protected:
            using Istr::Istr;
        };

        struct Pseudo : Istr {
           protected:
            using Istr::Istr;
        };

        template <template <class...> class Vec = std::list>
        struct SSA {
            size_t seq = 0;
            std::string block_id;
            Vec<std::shared_ptr<Value>> istrs;

            std::string get_tmp() {
                return "tmp_" + block_id + utils::number::to_string<std::string>(seq++);
            }

            template <class I>
            std::shared_ptr<I> istr() {
                return std::make_shared<I>();
            }

            std::shared_ptr<Value> value(const std::shared_ptr<Istr>& ist) {
                auto value = std::make_shared<Value>();
                value->name = get_tmp();
                value->istr = ist;
                istrs.push_back(value);
                return value;
            }

            std::shared_ptr<Value> named_value(const auto& name, const std::shared_ptr<Istr>& ist) {
                auto value = std::make_shared<Value>();
                value->name = name;
                value->istr = ist;
                istrs.push_back(value);
                return value;
            }

            std::shared_ptr<Value> annon_value(const std::shared_ptr<Istr>& ist) {
                return named_value("", ist);
            }
        };

    }  // namespace minilang::ssa
}  // namespace utils
