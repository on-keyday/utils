/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "traverse.h"
#include <deque>

namespace combl::istr {
    struct Value;

    enum class EvalResult {
        ok,
        do_callback,
        suspend,
        exit,
        fail,
    };

    struct Callback {
        trvs::FuncAttr* callee;
        std::vector<Value> args;
    };

    struct BuiltinFunc {
        std::shared_ptr<void> ctx;
        EvalResult (*eval_builtin)(std::shared_ptr<void>& ctx, Callback& cb, Value& ret, std::vector<Value>& args, std::uint64_t call_time);
    };

    struct Reference {
        std::shared_ptr<Value> value;
    };

    struct Value {
        std::variant<std::monostate, std::string, std::int64_t, std::uint64_t, std::weak_ptr<Value>, trvs::FuncAttr*, BuiltinFunc*, Reference> value;
        std::shared_ptr<trvs::Expr> node;

        const std::int64_t* as_int64() const {
            if (value.index() == 2) {
                return &std::get<2>(value);
            }
            return nullptr;
        }

        const std::uint64_t* as_uint64() const {
            if (value.index() == 3) {
                return &std::get<3>(value);
            }
            return nullptr;
        }

        const std::string* as_string() const {
            if (value.index() == 1) {
                return &std::get<1>(value);
            }
            return nullptr;
        }

        bool as_int(auto& val) const {
            if (auto v = as_int64()) {
                val = *v;
                return true;
            }
            if (auto v = as_uint64(); v && *v <= (~std::uint64_t(0) >> 1)) {
                val = *v;
                return true;
            }
            return false;
        }

        bool as_uint(auto& val) const {
            if (auto v = as_int64(); v && *v >= 0) {
                val = *v;
                return true;
            }
            if (auto v = as_uint64()) {
                val = *v;
                return true;
            }
            return false;
        }

        std::string to_string() const {
            if (auto v = as_int64()) {
                return utils::number::to_string<std::string>(*v);
            }
            else if (auto v = as_uint64()) {
                return utils::number::to_string<std::string>(*v);
            }
            else if (auto v = as_string()) {
                std::string str;
                utils::escape::escape_str(*v, str, utils::escape::EscapeFlag::hex | utils::escape::EscapeFlag::utf16);
                return "\"" + str + "\"";
            }
            else if (value.index() == 4) {
                auto l = std::get<4>(value).lock();
                if (!l) {
                    return "<invalid weak ref>";
                }
                return l->to_string();
            }
            else if (value.index() == 5) {
                return "<function>";
            }
            else if (value.index() == 6) {
                return "<builtin>";
            }
            return "<invalid value>";
        }
    };

    struct Op {
        std::shared_ptr<trvs::Binary> node;
    };

    struct Callee {
        std::shared_ptr<trvs::CallExpr> call;
    };
    struct Arg {};
    struct Call {};

    struct BuiltinRentrant {
        std::shared_ptr<trvs::CallExpr> node;
        BuiltinFunc* callee = nullptr;
        Value ret;
        std::vector<Value> args;
        size_t count = 0;
    };

    struct Init {
        std::weak_ptr<Value> ptr;
        std::shared_ptr<trvs::Let> node;
    };

    struct Ret {
        trvs::Pos retpos;
    };

    struct Loop {
        std::shared_ptr<trvs::For> node;
    };

    struct Instruction {
        std::variant<Value, Op, Arg, Callee, Call, Init, Ret, Loop, BuiltinRentrant> istr;
    };

    struct CallWait {
        std::shared_ptr<trvs::CallExpr> node;
        std::variant<trvs::FuncAttr*, BuiltinFunc*> callee;
        std::vector<Value> args;
    };

    using Istrs = std::deque<Instruction>;

    struct Eval {
        Istrs istrs;
        std::vector<Value> value;
        std::vector<CallWait> call;
    };

}  // namespace combl::istr
