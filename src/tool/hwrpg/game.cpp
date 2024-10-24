#include "save_data.h"
#include "game.h"
#include "expr.h"
#include <strutil/splits.h>
#include <env/env.h>
#include <comb2/composite/string.h>
#include <comb2/composite/number.h>
#include <helper/defer_ex.h>
#include <file/file_stream.h>
#include <escape/escape.h>
#include <strutil/space.h>
static void set_varint_len(save::Varint& v, std::uint64_t len) {
    if (len <= 0x3F) {
        v.prefix(0);
        v.value(len);
    }
    else if (len <= 0x3FFF) {
        v.prefix(1);
        v.value(len);
    }
    else if (len <= 0x3FFFFFFF) {
        v.prefix(2);
        v.value(len);
    }
    else {
        v.prefix(3);
        v.value(len);
    }
}

std::optional<Value> EvalValue::eval(RuntimeState& s) {
    auto str = string();
    if (str) {
        return *str;
    }
    auto n = number();
    if (n) {
        return *n;
    }
    auto b = boolean();
    if (b) {
        return *b;
    }
    auto a = assignable();
    if (a) {
        auto v = s.get_variable(a->name);
        if (!v) {
            // accessing undefined variable is not an error
            // returns null
            return Value();
        }
        return *v;
    }
    auto acc = access();
    if (acc) {
        return expr::eval_access(s, *acc);
    }
    auto arr = array();
    if (arr) {
        return Value(std::move(*arr));
    }
    auto obj = object();
    if (obj) {
        return Value(std::move(*obj));
    }
    if (auto f = func()) {
        return *f;
    }
    return Value();
}
namespace expr {

    EvalState builtin_to_string(RuntimeState& s, Value& target) {
        if (auto str = target.string()) {
            s.stacks.eval_stack.push_back(EvalValue(*str));
            return EvalState::normal;
        }
        if (auto n = target.number()) {
            s.stacks.eval_stack.push_back(EvalValue(futils::number::to_string<std::u32string>(*n)));
            return EvalState::normal;
        }
        if (auto b = target.boolean()) {
            s.stacks.eval_stack.push_back(EvalValue(b->value ? U"true" : U"false"));
            return EvalState::normal;
        }
        if (target.null()) {
            s.stacks.eval_stack.push_back(EvalValue(U"<null>"));
            return EvalState::normal;
        }
        if (auto f = target.func()) {
            s.stacks.eval_stack.push_back(EvalValue(U"<function>"));
            return EvalState::normal;
        }
        if (auto o = target.object()) {
            std::u32string result = U"{";
            bool first = true;
            for (auto& [k, v] : *o) {
                if (!first) {
                    result += U", ";
                }
                first = false;
                result += k;
                result += U": ";
                auto state = builtin_to_string(s, v);
                if (state != EvalState::normal) {
                    return state;
                }
                auto str = s.stacks.eval_stack.back().string();
                s.stacks.eval_stack.pop_back();
                if (!str) {
                    return s.eval_error(U"$builtin.to_string: オブジェクトの値を文字列に変換できません");
                }
                result += *str;
            }
            result += U"}";
            s.stacks.eval_stack.push_back(EvalValue(result));
            return EvalState::normal;
        }
        if (auto arr = target.array()) {
            std::u32string result = U"[";
            bool first = true;
            for (auto& v : *arr) {
                if (!first) {
                    result += U", ";
                }
                first = false;
                auto state = builtin_to_string(s, v);
                if (state != EvalState::normal) {
                    return state;
                }
                auto str = s.stacks.eval_stack.back().string();
                s.stacks.eval_stack.pop_back();
                if (!str) {
                    return s.eval_error(U"$builtin.to_string: 配列の要素を文字列に変換できません");
                }
                result += *str;
            }
            result += U"]";
            s.stacks.eval_stack.push_back(EvalValue(result));
            return EvalState::normal;
        }
        return s.eval_error(U"$builtin.to_string: 不正な型に対して呼び出しました");
    }

    EvalState builtin_parse_int(RuntimeState& s, Value& target, bool only_unsigned) {
        if (auto str = target.string()) {
            std::int64_t n;
            if (!futils::number::parse_integer(*str, n)) {
                s.stacks.eval_stack.push_back(EvalValue(nullptr));
            }
            else {
                if (only_unsigned && n < 0) {
                    s.stacks.eval_stack.push_back(EvalValue(nullptr));
                }
                else {
                    s.stacks.eval_stack.push_back(EvalValue(n));
                }
            }
            return EvalState::normal;
        }
        return s.eval_error(U"$builtin.parse_int: 文字列以外に適用されました");
    }

    EvalState invoke_builtin(RuntimeState& s, std::vector<Access>& access, std::vector<Value>& args) {
        auto obj = access[0].special_object().value();
        if (obj == SpecialObject::Builtin) {
            if (access.size() == 1) {
                return s.eval_error(U"$builtinを呼び出すことはできません");
            }
            auto key = access[1].key();
            if (!key) {
                return s.eval_error(U"不正な特殊オブジェクトアクセスです($builtin)");
            }
            if (access.size() == 2) {
                if (*key == U"to_string") {
                    if (args.size() != 1) {
                        return s.eval_error(U"$builtin.to_string: 引数の数が正しくありません");
                    }
                    auto& target = args[0];
                    return builtin_to_string(s, target);
                }
                if (*key == U"parse_int") {
                    if (args.size() != 1) {
                        return s.eval_error(U"$builtin.parse_int: 引数の数が正しくありません");
                    }
                    auto& target = args[0];
                    return builtin_parse_int(s, target, false);
                }
                if (*key == U"parse_uint") {
                    if (args.size() != 1) {
                        return s.eval_error(U"$builtin.parse_uint: 引数の数が正しくありません");
                    }
                    auto& target = args[0];
                    return builtin_parse_int(s, target, true);
                }
                if (*key == U"trim") {  // trim front and back spaces
                    if (args.size() != 1) {
                        return s.eval_error(U"$builtin.trim: 引数の数が正しくありません");
                    }
                    auto& target = args[0];
                    if (auto str = target.string()) {
                        std::u32string result;
                        auto it = str->begin();
                        size_t len = str->size();
                        while (it != str->end() && futils::strutil::is_space(*it)) {
                            it++;
                            len--;
                        }
                        size_t start_index = it - str->begin();
                        if (start_index < str->size()) {
                            result = str->substr(start_index, len);
                        }
                        while (!result.empty() && futils::strutil::is_space(result.back())) {
                            result.pop_back();
                        }
                        s.stacks.eval_stack.push_back(EvalValue(result));
                        return EvalState::normal;
                    }
                    return s.eval_error(U"$builtin.trim: 文字列以外に適用されました");
                }
                if (*key == U"split") {
                    if (args.size() != 2 &&
                        args.size() != 3) {
                        return s.eval_error(U"$builtin.split: 引数の数が正しくありません");
                    }
                    auto& target = args[0];
                    auto& sep = args[1];
                    if (auto str = target.string(); str) {
                        if (auto sep_str = sep.string(); sep_str) {
                            size_t n = ~0;
                            if (args.size() == 3) {  // ignore 3rd argument if not a number
                                if (auto n_ = args[2].number(); n_) {
                                    n = *n_;
                                }
                            }
                            auto result = futils::strutil::split<std::u32string>(*str, *sep_str, n);
                            std::vector<Value> arr;
                            for (auto& s : result) {
                                arr.push_back(Value(s));
                            }
                            s.stacks.eval_stack.push_back(EvalValue(arr));
                            return EvalState::normal;
                        }
                    }
                    return s.eval_error(U"$builtin.split: 文字列以外に適用されました");
                }
            }
        }
        return s.eval_error(U"不明な特殊オブジェクトです");
    }

    EvalState recursive_access(Value*& target, std::vector<Access>& access, RuntimeState& s, size_t offset, bool may_create) {
        for (auto i = offset; i < access.size(); i++) {
            auto& a = access[i];
            if (auto key = a.key()) {
                if (auto obj = target->object()) {
                    auto it = obj->find(*key);
                    if (it == obj->end()) {
                        return s.eval_error(U"オブジェクトにキーが見つかりません");
                    }
                    target = &it->second;
                }
                else if (auto arr = target->array(); !may_create && key == U"length" && i == access.size() - 1) {
                    return EvalState::call;  // indicate length access
                }
                else {
                    return s.eval_error(U"オブジェクトでないものにドットアクセスをしました");
                }
            }
            else if (auto index = a.index()) {
                if (auto arr = target->array()) {
                    if (may_create) {
                        if (*index == arr->size()) {  // append behavior
                            arr->push_back(Value());
                        }
                    }
                    if (*index >= arr->size()) {
                        return s.eval_error(U"配列の範囲外にアクセスしました");
                    }
                    target = &(*arr)[*index];
                }
                else {
                    return s.eval_error(U"配列でないものにインデックスアクセスをしました");
                }
            }
            else {
                return s.eval_error(U"不正なアクセスです(bug)");
            }
        }
        return EvalState::normal;
    }

    EvalState assign_access(RuntimeState& s, std::vector<Access>& access, Value& value_) {
        if (access.size() == 0) {
            return s.eval_error(U"不正な代入です");
        }
        if (auto a = access[0].assignable()) {
            Value* target = s.get_variable(a->name);
            if (!target) {
                s.eval_error(U"未定義の変数のフィールド/要素に代入しようとしました");
                return EvalState::error;
            }
            auto state = recursive_access(target, access, s, 1, true);
            if (state != EvalState::normal) {
                return state;
            }
            *target = value_;
            return EvalState::normal;
        }
        else if (auto obj_ = access[0].special_object()) {
            s.special_object_result = false;
            s.special_object_error_reason = U"";
            auto obj = *obj_;
            if (obj == SpecialObject::SaveData) {
                auto value = value_.string();
                if (!value) {
                    return EvalState::normal;
                }
                if (access.size() == 1) {
                    return EvalState::normal;
                }
                auto key = access[1].key();
                if (!key) {
                    return EvalState::normal;
                }
                if (key == U"players") {
                    if (access.size() == 2) {
                        return EvalState::normal;
                    }
                    auto index = access[2].index();
                    if (!index) {
                        return EvalState::normal;
                    }
                    bool add = false;
                    futils::helper::DynDefer remove;
                    if (*index == s.save.players.size()) {
                        if (s.save.players.size() >= 0xff) {
                            s.special_object_error_reason = U"プレイヤーは255人までしか作成できません。";
                            return EvalState::normal;
                        }
                        s.save.players.push_back({});
                        s.save.players_len++;
                        remove = futils::helper::defer_ex([&]() {
                            s.save.players.pop_back();
                            s.save.players_len--;
                        });
                    }
                    if (*index >= s.save.players.size()) {
                        return EvalState::normal;
                    }
                    auto& player = s.save.players[*index];
                    if (access.size() == 3) {
                        return EvalState::normal;
                    }
                    auto key = access[3].key();
                    if (!key) {
                        return EvalState::normal;
                    }
                    if (key == U"name") {
                        if (access.size() != 4) {
                            return EvalState::normal;
                        }
                        if (value->size() == 0 || value->size() > 10) {
                            s.special_object_error_reason = U"名前は1文字以上10文字以下である必要があります。";
                            return EvalState::normal;
                        }
                        player.name.name = futils::utf::convert<std::string>(*value);
                        set_varint_len(player.name.len, player.name.name.size());
                        s.special_object_result = true;
                        remove.cancel();
                        return EvalState::normal;
                    }
                }
                if (key == U"location") {
                    s.save.location.name = futils::utf::convert<std::string>(*value);
                    set_varint_len(s.save.location.len, s.save.location.name.size());
                }
                if (key == U"phase") {
                    s.save.phase.name = futils::utf::convert<std::string>(*value);
                    set_varint_len(s.save.phase.len, s.save.phase.name.size());
                }
            }
            else if (obj == SpecialObject::Global) {
                if (access.size() == 1) {
                    return EvalState::normal;
                }
                auto key = access[1].key();
                if (!key) {
                    return EvalState::normal;
                }
                if (access.size() == 2) {
                    s.global_variables[*key] = value_;
                }
                else {
                    auto it = s.global_variables.find(*key);
                    if (it == s.global_variables.end()) {
                        s.eval_error(U"グローバル変数にキーが見つかりません");
                        return EvalState::error;
                    }
                    auto target = &it->second;
                    auto x = recursive_access(target, access, s, 2, true);
                    if (x != EvalState::normal) {
                        return x;
                    }
                    *target = value_;
                }
            }
            return EvalState::normal;
        }
        s.eval_error(U"不明な代入です");
        return EvalState::error;
    }
    std::optional<Value> eval_access(RuntimeState& s, std::vector<Access>& access) {
        if (auto obj_ = access[0].special_object()) {
            auto null = Value();
            auto obj = *obj_;
            if (obj == SpecialObject::SaveData) {
                if (access.size() == 1) {
                    return null;
                }
                auto key = access[1].key();
                if (!key) {
                    return null;
                }
                if (key == U"players") {
                    if (access.size() == 2) {
                        return null;
                    }
                    auto index = access[2].index();
                    if (!index) {
                        return null;
                    }
                    if (*index >= s.save.players.size()) {
                        return null;
                    }
                    auto& player = s.save.players[*index];
                    if (access.size() == 3) {
                        return null;
                    }
                    auto key = access[3].key();
                    if (!key) {
                        return null;
                    }
                    if (key == U"name") {
                        if (access.size() != 4) {
                            return null;
                        }
                        return futils::utf::convert<std::u32string>(player.name.name);
                    }
                }
                if (key == U"location") {
                    if (access.size() != 2) {
                        return null;
                    }
                    return futils::utf::convert<std::u32string>(s.save.location.name);
                }
                if (key == U"phase") {
                    if (access.size() != 2) {
                        return null;
                    }
                    return Value(futils::utf::convert<std::u32string>(s.save.phase.name));
                }
            }
            else if (obj == SpecialObject::Args) {
                if (access.size() == 1) {
                    return null;
                }
                if (auto key = access[1].key()) {
                    if (*key == U"length") {
                        return Value(static_cast<std::int64_t>(s.stacks.args.size()));
                    }
                }
                auto index = access[1].index();
                if (!index) {
                    return null;
                }
                if (*index >= s.stacks.args.size()) {
                    return null;
                }
                return s.stacks.args[*index];
            }
            else if (obj == SpecialObject::Result) {
                if (access.size() == 1) {
                    return Value(Boolean{s.special_object_result});
                }
                auto key = access[1].key();
                if (!key) {
                    return null;
                }
                if (*key == U"reason") {
                    return Value(s.special_object_error_reason);
                }
            }
            else if (obj == SpecialObject::Input) {
                if (access.size() != 1) {
                    return null;
                }
                return Value(futils::utf::convert<std::u32string>(s.prompt_result));
            }
            else if (obj == SpecialObject::Global) {
                if (access.size() == 1) {
                    return null;
                }
                auto key = access[1].key();
                if (!key) {
                    return null;
                }
                auto it = s.global_variables.find(*key);
                if (it == s.global_variables.end()) {
                    return null;
                }
                auto target = &it->second;
                auto x = recursive_access(target, access, s, 2, false);
                if (x == EvalState::call) {
                    return Value(target->array()->size());
                }
                if (x != EvalState::normal) {
                    return null;
                }
                return *target;
            }
            return null;
        }
        else if (auto assignable = access[0].assignable()) {
            auto v = s.get_variable(assignable->name);
            if (!v) {
                s.eval_error(U"変数" + assignable->name + U"が見つかりません");
                return std::nullopt;
            }
            auto target = v;
            auto x = recursive_access(target, access, s, 1, false);
            if (x == EvalState::call) {
                return Value(target->array()->size());
            }
            if (x != EvalState::normal) {
                return std::nullopt;
            }
            return *target;
        }
        else if (auto obj_lit = access[0].object()) {
            Value obj, *target;
            auto k = access[1].key();
            if (!k) {
                s.eval_error(U"不正なアクセスです(オブジェクト)");
                return std::nullopt;
            }
            auto it = obj_lit->find(*k);
            if (it == obj_lit->end()) {
                s.eval_error(U"オブジェクトにキーが見つかりません");
                return std::nullopt;
            }
            obj = it->second;
            target = &obj;
            auto x = recursive_access(target, access, s, 2, false);
            if (x != EvalState::normal) {
                return std::nullopt;
            }
            return *target;
        }
        else if (auto arr_lit = access[0].array()) {
            auto index = access[1].index();
            if (!index) {
                if (auto key = access[1].key(); key == U"length" && access.size() == 2) {
                    return Value(static_cast<std::int64_t>(arr_lit->size()));
                }
                s.eval_error(U"不正なアクセスです(配列)");
                return std::nullopt;
            }
            if (*index >= arr_lit->size()) {
                s.eval_error(U"配列の範囲外にアクセスしました");
                return std::nullopt;
            }
            auto target = &(*arr_lit)[*index];
            auto x = recursive_access(target, access, s, 2, false);
            if (x == EvalState::call) {
                return Value(target->array()->size());
            }
            if (x != EvalState::normal) {
                return std::nullopt;
            }
            return *target;
        }
        s.eval_error(U"不明なアクセスです");
        return std::nullopt;
    }
}  // namespace expr

std::shared_ptr<expr::Expr> parse_expr(futils::Sequencer<futils::view::rvec>& seq) {
    std::shared_ptr<expr::Expr> result;
    size_t layer = 0;
    struct StackData {
        size_t layer;
        std::shared_ptr<expr::Expr> expr;
    };
    std::vector<StackData> stack;
    std::vector<std::vector<std::pair<std::string, expr::BinaryOp>>> layers = {
        {{"*", expr::BinaryOp::mul}, {"/", expr::BinaryOp::div}, {"%", expr::BinaryOp::mod}, {"&", expr::BinaryOp::bit_and}, {"<<", expr::BinaryOp::bit_left_shift}, {">>>", expr::BinaryOp::bit_right_logical_shift}, {">>", expr::BinaryOp::bit_right_arithmetic_shift}},
        {{"+", expr::BinaryOp::add}, {"-", expr::BinaryOp::sub}, {"|", expr::BinaryOp::bit_or}, {"^", expr::BinaryOp::bit_xor}},
        {{"==", expr::BinaryOp::eq}, {"!=", expr::BinaryOp::ne}},
        {{"<=", expr::BinaryOp::le}, {"<", expr::BinaryOp::lt}, {">=", expr::BinaryOp::ge}, {">", expr::BinaryOp::gt}},
        {{"&&", expr::BinaryOp::and_}, {"||", expr::BinaryOp::or_}},
    };
    size_t max_layer = 1 + layers.size() + 1;
    auto skip_space = [&]() {
        while (seq.consume_if(' ') || seq.consume_if('\t')) {
        }
    };
    while (layer < max_layer) {
        if (stack.size() > 0 && stack.back().layer == layer) {
            if (auto assign = std::dynamic_pointer_cast<expr::AssignExpr>(stack.back().expr)) {
                assign->value = result;
                result = stack.back().expr;
                stack.pop_back();
            }
            else if (auto binary = std::dynamic_pointer_cast<expr::BinaryExpr>(stack.back().expr)) {
                binary->right = result;
                result = stack.back().expr;
                stack.pop_back();
            }
            else {
                return nullptr;
            }
        }
        skip_space();
        if (layer == 0) {
            std::vector<expr::UnaryOp> unary_ops;
            while (true) {
                if (seq.seek_if("!")) {
                    unary_ops.push_back(expr::UnaryOp::not_);
                }
                else if (seq.seek_if("+")) {
                    unary_ops.push_back(expr::UnaryOp::plus);
                }
                else if (seq.seek_if("-")) {
                    unary_ops.push_back(expr::UnaryOp::minus);
                }
                else if (seq.seek_if("~")) {
                    unary_ops.push_back(expr::UnaryOp::bit_not);
                }
                else {
                    break;
                }
            }
            if (seq.seek_if("$")) {
                auto begin = seq.rptr;
                auto r = futils::comb2::composite::c_ident(seq, 0, 0);
                if (r != futils::comb2::Status::match) {
                    return nullptr;
                }
                auto end = seq.rptr;
                auto name = seq.buf.buffer.substr(begin, end - begin);
                std::optional<expr::SpecialObject> special;
                if (name == "save_data") {
                    special = expr::SpecialObject::SaveData;
                }
                else if (name == "args") {
                    special = expr::SpecialObject::Args;
                }
                else if (name == "input") {
                    special = expr::SpecialObject::Input;
                }
                else if (name == "result") {
                    special = expr::SpecialObject::Result;
                }
                else if (name == "builtin") {
                    special = expr::SpecialObject::Builtin;
                }
                else if (name == "const") {
                    special = expr::SpecialObject::Const;
                }
                else if (name == "global") {
                    special = expr::SpecialObject::Global;
                }
                if (special) {
                    auto special_obj = std::make_shared<expr::SpecialObjectExpr>();
                    special_obj->value = *special;
                    result = special_obj;
                }
                else {
                    auto ident = std::make_shared<expr::IdentifierExpr>();
                    ident->name = futils::utf::convert<std::u32string>(name);
                    result = ident;
                }
            }
            else if (seq.seek_if("true")) {
                auto bool_lit = std::make_shared<expr::BoolLiteralExpr>();
                bool_lit->value = true;
                result = bool_lit;
            }
            else if (seq.seek_if("false")) {
                auto bool_lit = std::make_shared<expr::BoolLiteralExpr>();
                bool_lit->value = false;
                result = bool_lit;
            }
            else if (seq.seek_if("null")) {
                result = std::make_shared<expr::NullLiteralExpr>();
            }
            else if (seq.match("\"")) {
                auto begin = seq.rptr;
                auto r = futils::comb2::composite::c_str(seq, 0, 0);
                if (r != futils::comb2::Status::match) {
                    return nullptr;
                }
                auto end = seq.rptr;
                auto str_lit = std::make_shared<expr::StringLiteralExpr>();
                auto s = seq.buf.buffer.substr(begin + 1, end - begin - 2);
                std::string value;
                if (!futils::escape::unescape_str(s, value)) {
                    return nullptr;
                }
                str_lit->value = futils::utf::convert<std::u32string>(value);
                result = str_lit;
            }
            else if (futils::number::is_digit(seq.current())) {
                auto begin = seq.rptr;
                auto r = futils::comb2::composite::dec_integer(seq, 0, 0);
                if (r != futils::comb2::Status::match) {
                    return nullptr;
                }
                auto end = seq.rptr;
                auto int_lit = std::make_shared<expr::IntLiteralExpr>();
                if (!futils::number::parse_integer(seq.buf.buffer.substr(begin, end - begin), int_lit->value)) {
                    return nullptr;
                }
                result = int_lit;
            }
            else if (seq.seek_if("(")) {
                result = parse_expr(seq);
                if (!seq.consume_if(')')) {
                    return nullptr;
                }
            }
            else if (seq.seek_if("[")) {
                auto arr = std::make_shared<expr::ArrayLiteralExpr>();
                skip_space();
                while (!seq.seek_if("]")) {
                    auto elem = parse_expr(seq);
                    if (!elem) {
                        return nullptr;
                    }
                    arr->elements.push_back(elem);
                    skip_space();
                    if (!seq.consume_if(',')) {
                        if (!seq.seek_if("]")) {
                            return nullptr;
                        }
                        break;
                    }
                    skip_space();
                }
                result = arr;
            }
            else if (seq.seek_if("{")) {
                auto obj = std::make_shared<expr::ObjectLiteralExpr>();
                skip_space();
                while (!seq.seek_if("}")) {
                    auto begin = seq.rptr;
                    auto s = futils::comb2::composite::c_ident(seq, 0, 0);
                    if (s != futils::comb2::Status::match) {
                        return nullptr;
                    }
                    auto end = seq.rptr;
                    auto key = futils::utf::convert<std::u32string>(seq.buf.buffer.substr(begin, end - begin));
                    skip_space();
                    if (!seq.consume_if(':')) {
                        return nullptr;
                    }
                    auto value = parse_expr(seq);
                    if (!value) {
                        return nullptr;
                    }
                    obj->elements.push_back({key, value});
                    skip_space();
                    if (!seq.consume_if(',')) {
                        if (!seq.seek_if("}")) {
                            return nullptr;
                        }
                        break;
                    }
                    skip_space();
                }
                result = obj;
            }
            else {
                return nullptr;
            }
            while (true) {
                if (seq.seek_if(".")) {
                    skip_space();
                    auto dot_expr = std::make_shared<expr::DotAccessExpr>();
                    dot_expr->left = result;
                    auto begin = seq.rptr;
                    auto r = futils::comb2::composite::c_ident(seq, 0, 0);
                    if (r != futils::comb2::Status::match) {
                        return nullptr;
                    }
                    auto end = seq.rptr;
                    dot_expr->right = futils::utf::convert<std::u32string>(seq.buf.buffer.substr(begin, end - begin));
                    result = dot_expr;
                }
                else if (seq.seek_if("[")) {
                    auto index_expr = std::make_shared<expr::IndexAccessExpr>();
                    index_expr->left = result;
                    index_expr->right = parse_expr(seq);
                    if (!index_expr->right) {
                        return nullptr;
                    }
                    if (!seq.consume_if(']')) {
                        return nullptr;
                    }
                    result = index_expr;
                }
                else if (seq.seek_if("(")) {
                    auto call_expr = std::make_shared<expr::CallExpr>();
                    call_expr->callee = result;
                    skip_space();
                    while (!seq.seek_if(")")) {
                        auto arg = parse_expr(seq);
                        if (!arg) {
                            return nullptr;
                        }
                        call_expr->args.push_back(arg);
                        if (!seq.consume_if(',')) {
                            if (!seq.seek_if(")")) {
                                return nullptr;
                            }
                            break;
                        }
                    }
                    result = call_expr;
                }
                else {
                    break;
                }
            }
            while (unary_ops.size()) {
                auto unary_expr = std::make_shared<expr::UnaryExpr>();
                unary_expr->op = unary_ops.back();
                unary_ops.pop_back();
                unary_expr->value = result;
                result = unary_expr;
            }
        }
        else if (layer == max_layer - 1) {
            if (seq.consume_if('=')) {
                auto assign = std::make_shared<expr::AssignExpr>();
                assign->target = result;
                stack.push_back({layer, assign});
                result = nullptr;
                layer = 0;
                continue;
            }
        }
        else {
            auto& l = layers[layer - 1];
            bool to0 = false;
            for (auto& [op, bin_op] : l) {
                auto ptr = seq.rptr;
                if (seq.seek_if(op)) {
                    if (op.size() == 1 && seq.seek_if(op)) {
                        seq.rptr = ptr;
                        continue;
                    }
                    auto bin_expr = std::make_shared<expr::BinaryExpr>();
                    bin_expr->left = result;
                    bin_expr->op = bin_op;
                    stack.push_back({layer, bin_expr});
                    result = nullptr;
                    layer = 0;
                    to0 = true;
                    break;
                }
            }
            if (to0) {
                continue;
            }
        }
        layer++;
    }
    if (stack.size() > 0) {
        return nullptr;
    }
    return result;
}

std::shared_ptr<expr::Expr> parse_expr(futils::view::rvec expr) {
    auto seq = futils::make_cpy_seq(expr);
    return parse_expr(seq);
}

struct ScriptLineMap {
    futils::view::rvec script_name;
    size_t line = 0;
};

struct LineMappedCommand {
    ScriptLineMap line_map;
    std::vector<futils::view::rvec> cmd;
};

struct GameMain {
    RuntimeState state;

    GameMain(TextController& text, save::SaveData& save, futils::wrap::path_string& save_data_path)
        : state{text, save, save_data_path} {
    }

    void report_script_error(std::u32string msg) {
        auto u32script_name = futils::utf::convert<std::u32string>(state.current_script_name);
        auto u32script_line = futils::number::to_string<std::u32string>(state.current_script_line + 1);
        auto e = U"スクリプトエラー: " + u32script_name + U"(" + u32script_line + U"):\n" + msg;
        if (state.eval_error_reason.size()) {
            e += U"\n" + state.eval_error_reason;
        }
        state.text.write_story(e, 1);
        state.game_end_request = GameEndRequest::failure;
    }

    bool result_to_bool(const Value& result, bool& cond) {
        auto r = result.boolean();
        if (!r) {
            report_script_error(U"真偽値が必要です");
            return false;
        }
        cond = r->value;
        return true;
    }

    bool skip_to_spec_end(size_t& line, std::vector<LineMappedCommand>& lines, BlockType type) {
        auto& blocks = state.stacks.blocks;
        if (blocks.empty()) {
            report_script_error(U"while/ifブロックが開かれていません");
            return false;
        }
        for (; line < lines.size(); line++) {
            auto& cmd = lines[line].cmd;
            if (cmd[0] == "end") {
                if (blocks.back().type == type) {
                    return true;  // remain block
                }
                blocks.pop_back();  // remove if block
                if (blocks.empty()) {
                    report_script_error(U"while/ifブロックが開かれていません");
                    return false;
                }
            }
        }
        report_script_error(U"while/ifブロックが閉じられていません");
        return false;
    }

    bool skip_to_end(size_t& line, std::vector<LineMappedCommand>& lines, bool include_else_or_elif) {
        line++;
        size_t layer = 1;
        for (; line < lines.size(); line++) {
            auto& cmd = lines[line].cmd;
            if (cmd[0] == "end") {
                layer--;
                if (layer == 0) {
                    break;
                }
            }
            else if (include_else_or_elif && layer == 1 && (cmd[0] == "else" || cmd[0] == "elif")) {
                layer--;
                break;
            }
            else if (cmd[0] == "while" || cmd[0] == "if" || cmd[0] == "func") {
                layer++;
            }
        }
        if (layer != 0) {
            report_script_error(U"ブロックが閉じられていません");
            return false;
        }
        return true;
    }

    bool do_eval(size_t& line) {
        while (state.stacks.expr_stack.size() > 0) {
            auto expr = state.stacks.expr_stack.back();
            state.stacks.expr_stack.pop_back();
            auto result = expr->eval(state);
            if (result == expr::EvalState::error) {
                report_script_error(U"式の評価に失敗しました");
                return false;
            }
            if (result == expr::EvalState::call) {
                if (state.call_stack.empty()) {  // check callee is set
                    report_script_error(U"関数呼び出しのスタックが空です");
                    return false;
                }
                auto& call = state.call_stack.back();
                auto callee_pos = call.back_pos;
                call.back_pos = line;
                line = callee_pos;
                state.stacks.blocks.push_back({line, BlockType::func_block, nullptr});
                return false;
            }
        }
        return true;
    }

    void do_return(size_t& line, Value&& return_value) {
        if (state.call_stack.empty()) {
            report_script_error(U"関数呼び出しのスタックが空です");
            return;
        }
        auto& call = state.call_stack.back();
        state.stacks = std::move(call.stacks);
        state.stacks.eval_stack.push_back(std::move(return_value));  // return value
        line = call.back_pos - 1;
        state.call_stack.pop_back();
    }

    void run_command(std::vector<futils::view::rvec>& cmd, size_t& line, std::vector<LineMappedCommand>& lines) {
        auto& inst = cmd[0];
        if (inst == "reload_script") {
            state.game_end_request = GameEndRequest::reload;
        }
        else if (inst == "clear_screen") {
            state.text.clear_screen();
        }
        else if (inst == "speed" || inst == "wait" || inst == "delay_offset") {
            if (cmd.size() < 2) {
                report_script_error(U"引数が足りません");
                return;
            }
            size_t val = 0;
            if (!futils::number::parse_integer(cmd[1], val)) {
                report_script_error(U"数値を指定してください");
                return;
            }
            if (inst == "speed") {
                state.text_speed = val;
            }
            else if (inst == "wait") {
                state.after_wait = val;
            }
            else if (inst == "delay_offset") {
                state.delay_offset = val;
            }
        }
        else if (inst == "layout") {
            if (cmd.size() < 2) {
                report_script_error(U"引数が足りません");
                return;
            }
            if (cmd[1] == "center") {
                state.text.layout.reset_layout(center_layout);
            }
            else if (cmd[1] == "text") {
                state.text.layout.reset_layout(text_layout);
            }
            else {
                report_script_error(U"不明なレイアウトです");
            }
        }
        else if (inst == "new_text" || inst == "new_prompt" || inst == "add_text" || inst == "text" || inst == "prompt") {
            bool new_text = false;
            if (cmd.size() >= 2) {
                if (inst == "add_text") {
                    state.delay_offset = state.story_text.size();
                    state.story_text += futils::utf::convert<std::u32string>(cmd[1]);
                }
                else {
                    state.story_text = futils::utf::convert<std::u32string>(cmd[1]);
                    if (inst == "new_text" || inst == "new_prompt") {
                        state.delay_offset = 0;
                    }
                }
                size_t offset = 1;
                while (line + offset < lines.size()) {
                    auto& next = lines[line + offset];
                    if (next.cmd[0] == ">") {
                        state.story_text += U"\n";
                        if (next.cmd.size() > 1) {
                            state.story_text += futils::utf::convert<std::u32string>(next.cmd[1]);
                        }
                        offset++;
                    }
                    else {
                        break;
                    }
                }
                line += offset - 1;
                state.story_text = futils::env::expand<std::u32string>(state.story_text, [&](auto&& out, auto&& read) {
                    std::u32string var_name;
                    if (!read(var_name)) {
                        return;  // error
                    }
                    auto v = state.get_variable(var_name);
                    if (!v) {
                        return;  // not found
                    }
                    if (auto s = v->string()) {
                        futils::strutil::append(out, *s);
                    }
                    else if (auto f = v->func()) {
                        futils::strutil::append(out, "<function at ");
                        auto src = futils::utf::convert<std::u32string>(lines[f->start].line_map.script_name);
                        auto l = lines[f->start].line_map.line + 1;
                        futils::strutil::append(out, src);
                        futils::strutil::append(out, ":");
                        futils::number::to_string(out, l);
                        futils::strutil::append(out, ">");
                    }
                    else if (auto b = v->boolean()) {
                        futils::strutil::append(out, b->value ? U"true" : U"false");
                    }
                    else if (auto i = v->number()) {
                        futils::number::to_string(out, *i);
                    }
                    else if (auto i = v->null()) {
                        futils::strutil::append(out, "<null>");
                    }
                    else {
                        futils::strutil::append(out, "<unknown>");
                    }
                });
                new_text = true;
            }
            auto speed = state.text_speed;
            auto after_wait = state.after_wait;
            auto delay_offset = state.delay_offset;
            if (!new_text) {
                speed = 0;
                after_wait = 0;
                delay_offset = 0;
            }
            if (inst == "new_text" || inst == "add_text" || inst == "text") {
                state.text.write_story(state.story_text, speed, after_wait, delay_offset);
            }
            else {
                state.prompt_result = state.text.write_prompt(state.story_text, speed, after_wait, delay_offset);
            }
        }
        else if (inst == "exit") {
            state.game_end_request = GameEndRequest::exit;
        }
        else if (inst == "save") {
            save(true);
        }
        else if (inst == "assert" || inst == "eval" ||
                 inst == "if" || inst == "elif" || inst == "while" ||
                 inst == "return") {
            if (inst == "elif" && !state.jump_from_if) {
                if (state.stacks.blocks.empty() || state.stacks.blocks.back().type != BlockType::if_block) {
                    report_script_error(U"elifがifブロックの外にあります");
                    return;
                }
                state.stacks.blocks.pop_back();
                // as end of if block
                skip_to_end(line, lines, false);
                return;
            }
            if (state.stacks.eval_stack.empty()) {
                if (cmd.size() < 2) {
                    if (inst == "return") {
                        do_return(line, Value());
                        return;
                    }
                    report_script_error(U"引数が足りません");
                    return;
                }
                auto expr = parse_expr(cmd[1]);
                if (!expr) {
                    report_script_error(U"式の解析に失敗しました");
                    return;
                }
                expr->push(state);
                state.stacks.root_expr = expr;
            }
            if (!state.stacks.root_expr) {
                report_script_error(U"式が指定されていません");
                return;
            }
            if (!do_eval(line)) {
                return;  // error or incomplete
            }
            if (state.stacks.eval_stack.size() != 1 ||
                state.stacks.expr_stack.size() != 0) {
                report_script_error(U"スタックが正しく処理されていません");
                return;
            }
            auto result = state.stacks.eval_stack.back();
            state.stacks.eval_stack.pop_back();
            if (inst == "if" || inst == "elif" || inst == "while") {
                bool cond = false;
                auto r = result.eval(state);
                if (!r) {
                    report_script_error(U"式の評価に失敗しました");
                    return;
                }
                if (!result_to_bool(*r, cond)) {
                    return;
                }
                if (!cond) {
                    skip_to_end(line, lines, inst == "if" || inst == "elif");
                    if (lines[line].cmd[0] == "elif") {
                        line--;
                        state.jump_from_if = true;
                    }
                    else {
                        state.jump_from_if = false;
                    }
                    if (lines[line].cmd[0] == "else") {
                        state.stacks.blocks.push_back({line, BlockType::if_block, nullptr});
                    }
                    return;
                }
                state.jump_from_if = false;
                if (inst == "if" || inst == "elif") {
                    state.stacks.blocks.push_back({line, BlockType::if_block, std::move(state.stacks.root_expr)});
                }
                else {
                    state.stacks.blocks.push_back({line, BlockType::while_block, std::move(state.stacks.root_expr)});
                }
            }
            else if (inst == "assert") {
                bool cond = false;
                auto r = result.eval(state);
                if (!r) {
                    report_script_error(U"式の評価に失敗しました");
                    return;
                }
                if (result_to_bool(*r, cond)) {
                    return;
                }
                if (!cond) {
                    report_script_error(U"アサーションエラー");
                    return;
                }
            }
            else if (inst == "return") {
                auto r = result.eval(state);
                if (!r) {
                    report_script_error(U"式の評価に失敗しました");
                    return;
                }
                do_return(line, std::move(*r));
            }
        }
        else if (inst == "break" || inst == "continue") {
            if (!skip_to_spec_end(line, lines, BlockType::while_block)) {
                return;  // error
            }
            if (inst == "break") {
                state.stacks.blocks.pop_back();
                return;
            }
            line = state.stacks.blocks.back().start - 1;  // reenter
        }
        else if (inst == "else") {
            if (state.stacks.blocks.empty() || state.stacks.blocks.back().type != BlockType::if_block) {
                report_script_error(U"elseがifブロックの外にあります");
                return;
            }
            state.stacks.blocks.pop_back();
            // as end of if block
            skip_to_end(line, lines, false);
        }
        else if (inst == "end") {
            if (state.stacks.blocks.empty()) {
                report_script_error(U"ブロックが開かれていません");
                return;
            }
            auto type = state.stacks.blocks.back().type;
            if (type == BlockType::while_block) {
                line = state.stacks.blocks.back().start - 1;  // reenter
            }
            else if (type == BlockType::if_block) {
                state.stacks.blocks.pop_back();
            }
            else if (type == BlockType::func_block) {
                do_return(line, Value());
            }
        }
        else if (inst == "phase") {
            if (cmd.size() < 2) {
                report_script_error(U"引数が足りません");
                return;
            }
            state.save.phase.name = futils::utf::convert<std::string>(cmd[1]);
            set_varint_len(state.save.phase.len, state.save.phase.name.size());
        }
        else if (inst == "func") {
            if (cmd.size() < 2) {
                report_script_error(U"引数が足りません");
                return;
            }
            auto name = cmd[1];
            while (name.size() && (name[0] == U' ' || name[0] == U'\t')) {
                name = name.substr(1);
            }
            if (name.size() && (name.back() == U' ' || name.back() == U'\t')) {
                name = name.substr(0, name.size() - 1);
            }
            auto seq = futils::make_cpy_seq(name);
            auto res = futils::comb2::composite::c_ident(seq, 0, 0);
            if (res != futils::comb2::Status::match || seq.rptr != name.size()) {
                report_script_error(U"関数名が不正です");
                return;
            }
            auto u32name = futils::utf::convert<std::u32string>(name);
            Func func;
            func.start = line;
            if (!skip_to_end(line, lines, false)) {
                return;
            }
            func.end = line;
            if (state.call_stack.size()) {
                // capture current scope
                func.capture = state.call_stack.back().function_scope->clone();
            }
            state.set_variable(u32name, Value(std::move(func)));
        }
        else {
            report_script_error(U"不明なコマンド: " + futils::utf::convert<std::u32string>(inst));
        }
    }

    void save(bool via_command) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        if (auto err = state.save.encode(w)) {
            if (via_command) {
                state.special_object_result = false;
                state.special_object_error_reason = U"セーブデータのエンコードに失敗しました\n" + err.error<std::u32string>();
                return;
            }
            state.text.write_story(U"failed to encode save data: " + err.error<std::u32string>(), 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        auto res = futils::file::File::create(state.save_data_path);
        if (!res) {
            if (via_command) {
                state.special_object_result = false;
                state.special_object_error_reason = U"セーブデータファイルの作成に失敗しました\n" + res.error().error<std::u32string>();
                return;
            }
            state.text.write_story(U"failed to open save data file", 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        futils::file::FileStream<std::string> fs{*res};
        futils::binary::writer buffer_writer{fs.get_direct_write_handler(), &fs};
        if (!buffer_writer.write(w.written())) {
            if (via_command) {
                state.special_object_result = false;
                state.special_object_error_reason = U"セーブデータの書き込みに失敗しました\n" + fs.error.error<std::u32string>();
                return;
            }
            state.text.write_story(U"failed to write save data", 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        if (via_command) {
            state.special_object_result = true;
        }
    }

    void load_script(std::vector<futils::view::rvec>& recursive_check, futils::view::rvec script, futils::view::rvec name, std::vector<LineMappedCommand>& cmds) {
        for (auto& check : recursive_check) {
            if (check == name) {
                report_script_error(U"循環参照があります");
                return;
            }
        }
        auto lines = futils::strutil::lines<futils::view::rvec>(script);
        for (size_t i = 0; i < lines.size(); i++) {
            auto& l = lines[i];
            while (!l.empty() && (l[0] == U' ' || l[0] == U'\t')) {
                l = l.substr(1);
            }
            if (l.empty() || l[0] == U'#') {
                continue;
            }
            auto cmd = futils::strutil::split<futils::view::rvec>(l, " ", 1);
            if (cmd.empty()) {
                continue;
            }
            state.current_script_line = i;
            state.current_script_name = name;
            if (cmd[0] == "include") {
                if (cmd.size() < 2) {
                    report_script_error(U"引数が足りません");
                    return;
                }
                auto include = read_file(cmd[1], state.text);
                if (include.empty()) {
                    return;
                }
                recursive_check.push_back(name);
                load_script(recursive_check, include, cmd[1], cmds);
                if (state.game_end_request != GameEndRequest::none) {
                    return;
                }
                recursive_check.pop_back();
            }
            // expression, so interpret this line as eval command (syntax sugar)
            else if (cmd[0][0] == '$' || cmd[0] == "true" || cmd[0] == "null" || cmd[0] == "false" ||
                     futils::number::is_digit(cmd[0][0]) || cmd[0][0] == '"' || cmd[0][0] == '[' || cmd[0][0] == '{' ||
                     cmd[0][0] == '(' ||
                     cmd[0][0] == '!' || cmd[0][0] == '+' || cmd[0][0] == '-' || cmd[0][0] == '~') {
                cmds.push_back({{name, i}, {"eval", l}});
            }
            else {
                cmds.push_back({{name, i}, cmd});
            }
        }
    }

    void run_script(futils::view::rvec script) {
        state.stacks.clear();
        state.global_variables.clear();
        state.call_stack.clear();
        state.jump_from_if = false;
        std::vector<LineMappedCommand> cmds;
        std::vector<futils::view::rvec> recursive_check;
        load_script(recursive_check, script, state.current_script_name, cmds);
        if (state.game_end_request != GameEndRequest::none) {
            return;
        }
        size_t c = 0;
        auto prev_phase = state.save.phase.name;
        while (c < cmds.size()) {
            auto& cmd = cmds[c];
            state.current_script_line = cmd.line_map.line;
            state.current_script_name = cmd.line_map.script_name;
            run_command(cmd.cmd, c, cmds);
            if (signaled || state.game_end_request != GameEndRequest::none) {
                return;
            }
            if (prev_phase != state.save.phase.name) {
                return;
            }
            c++;
        }
    }

    void run_phase(futils::view::rvec phase, bool try_debug) {
        auto it = std::find_if(state.config.phases.begin(), state.config.phases.end(), [&](const auto& p) { return p.name == phase; });
        if (it == state.config.phases.end()) {
            if (try_debug) {
                return;
            }
            state.text.write_story(U"failed to find phase " + futils::utf::convert<std::u32string>(phase), 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        auto script = read_file(it->script, state.text);
        if (script.empty()) {
            if (try_debug) {
                return;
            }
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        state.current_script_name = it->script;
        state.current_script_line = 0;
        if (try_debug) {
            state.game_end_request = GameEndRequest::none;
        }
        run_script(script);
    }

    void main_loop() {
        for (;;) {
            run_phase(state.save.phase.name, false);
            if (state.game_end_request == GameEndRequest::failure) {
                run_phase("debug", true);
                if (state.game_end_request == GameEndRequest::failure) {
                    return;
                }
            }
            if (signaled) {
                signaled = false;
                state.game_end_request = GameEndRequest::interrupt;
                run_phase("debug", true);
                if (signaled) {
                    state.game_end_request = GameEndRequest::interrupt;
                }
                if (state.game_end_request == GameEndRequest::interrupt) {
                    state.text.write_story(U"interrupted!!\nゲームを終了します....", 1);
                    return;
                }
                if (state.game_end_request == GameEndRequest::failure) {
                    return;
                }
            }
            if (state.game_end_request == GameEndRequest::exit) {
                state.text.write_story(U"ゲームを終了します....", 1);
                return;
            }
            if (state.game_end_request == GameEndRequest::reload) {
                return;
            }
        }
    }

    void game_start() {
        auto data = read_file("config.json", state.text);
        if (data.empty()) {
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        auto json = futils::json::parse<futils::json::JSON>(data, true);
        if (json.is_undef()) {
            state.text.write_story(U"failed to parse config.json", 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        if (!futils::json::convert_from_json(json, state.config)) {
            state.text.write_story(U"failed to convert config.json", 1);
            state.game_end_request = GameEndRequest::failure;
            return;
        }
        main_loop();
    }
};

GameEndRequest run_game(TextController& ctrl, save::SaveData& data, const char* dataFile, const char* indexFile, futils::wrap::path_string& save_data_name) {
    auto res = load_embed(dataFile, indexFile, ctrl);
    if (res != 0) {
        return GameEndRequest::failure;
    }
    GameMain game{ctrl, data, save_data_name};
    game.game_start();
    return game.state.game_end_request;
}
