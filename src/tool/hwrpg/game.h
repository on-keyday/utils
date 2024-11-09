/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

#include "save_data.h"
#include <json/convert_json.h>
#include <json/json_export.h>
#include <set>
#include "i8n.h"
#include <thread/concurrent_queue.h>
#include "io.h"

int make_embed(const char* target_dir, const char* embed_file, const char* embed_index);
int verify_embed(const char* dataFile, const char* indexFile);
int load_embed(const char* dataFile, const char* indexFile, io::TextController& text);
void unload_embed();
enum class GameEndRequest {
    none,
    exit,
    failure,
    interrupt,
    reload,
    go_title,
};
GameEndRequest run_game(io::TextController& ctrl, save::SaveData& data, const char* dataFile, const char* indexFile, futils::wrap::path_string& save_data_name);

futils::view::rvec read_file(futils::view::rvec file, io::TextController& err);

struct PhaseData {
    std::string name;
    std::string script;

    bool from_json(const futils::json::JSON& json, futils::json::FromFlag flag) {
        JSON_PARAM_BEGIN(*this, json);
        FROM_JSON_PARAM(name, "name", flag);
        FROM_JSON_PARAM(script, "script", flag);
        JSON_PARAM_END();
    }
};

struct GameConfig {
    std::string version;
    std::vector<PhaseData> phases;

    bool from_json(const futils::json::JSON& json, futils::json::FromFlag flag) {
        JSON_PARAM_BEGIN(*this, json);
        FROM_JSON_PARAM(version, "version", flag);
        FROM_JSON_PARAM(phases, "phases", flag);
        JSON_PARAM_END();
    }
};

enum class BlockType {
    while_block,
    if_block,
    func_block,
};
struct Assignable {
    std::u32string name;
};
struct Value;
using ValueMap = std::map<std::u32string, Value>;
using ValueArray = std::vector<Value>;
struct Boolean {
    bool value;
};

struct Scope {
    std::map<std::u32string, std::shared_ptr<Value>> variables;

    std::shared_ptr<Scope> clone() {
        auto scope = std::make_shared<Scope>();
        for (auto& [key, value] : variables) {
            scope->variables[key] = value;
        }
        return scope;
    }
};

struct Func {
    size_t start = 0;
    size_t end = 0;
    std::shared_ptr<Scope> capture;
};

struct Bytes {
    std::string data;
};

namespace expr {

    enum class EvalState {
        normal,
        call,
        jump,  // for $builtin.eval
        error,
    };
}

struct RuntimeState;
namespace foreign {
    struct ForeignLibrary {
        futils::wrap::path_string path;
        void* data = nullptr;
        std::function<void(void*)> unload;

        ~ForeignLibrary() {
            if (unload) {
                unload(data);
            }
        }
    };

    struct ForeignData {
        std::map<futils::wrap::path_string, std::shared_ptr<ForeignLibrary>> libraries;
    };

    struct Callback {
        Func func;
        ValueArray args;
        Value& result;
        size_t count = 0;
    };

    std::shared_ptr<ForeignLibrary> load_foreign(RuntimeState& s, std::u32string& name);
    bool unload_foreign(RuntimeState& s, std::shared_ptr<ForeignLibrary>& lib);
    expr::EvalState call_foreign(RuntimeState& s, std::shared_ptr<ForeignLibrary>& lib, std::u32string& name, std::vector<Value>& args, Value& result, Callback& callback);
    enum class ForeignType {
        library_object,
        library_specific_start,
    };
}  // namespace foreign

struct Foreign {
    foreign::ForeignType type = foreign::ForeignType::library_object;
    std::shared_ptr<void> data;
};

struct Value {
   private:
    std::variant<std::monostate, Foreign, Bytes, std::u32string, std::int64_t, Boolean, Func, ValueArray, ValueMap> value;

   public:
    Value() {}
    Value(std::u32string v)
        : value(v) {
    }

    Value(Bytes v)
        : value(v) {
    }

    Value(Foreign v)
        : value(v) {
    }

    Value(Func v)
        : value(v) {
    }

    Value(std::int64_t v)
        : value(v) {
    }

    Value(Boolean v)
        : value(v) {
    }

    Value(ValueArray v)
        : value(std::move(v)) {
    }

    Value(ValueMap v)
        : value(std::move(v)) {
    }

    bool null() const {
        return std::holds_alternative<std::monostate>(value);
    }

    std::optional<std::u32string> string() const {
        if (auto p = std::get_if<std::u32string>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    const Bytes* bytes() const {
        if (auto p = std::get_if<Bytes>(&value)) {
            return p;
        }
        return nullptr;
    }

    const Foreign* foreign() const {
        if (auto p = std::get_if<Foreign>(&value)) {
            return p;
        }
        return nullptr;
    }

    std::optional<std::int64_t> number() const {
        if (auto p = std::get_if<std::int64_t>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    std::optional<Boolean> boolean() const {
        if (auto p = std::get_if<Boolean>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    const Func* func() const {
        if (auto p = std::get_if<Func>(&value)) {
            return p;
        }
        return nullptr;
    }

    Func* func() {
        if (auto p = std::get_if<Func>(&value)) {
            return p;
        }
        return nullptr;
    }

    ValueArray* array() {
        if (auto p = std::get_if<ValueArray>(&value)) {
            return p;
        }
        return nullptr;
    }

    ValueMap* object() {
        if (auto p = std::get_if<ValueMap>(&value)) {
            return p;
        }
        return nullptr;
    }
};

namespace expr {
    struct Expr;
    enum class SpecialObject {
        SaveData,
        Args,
        Input,
        Output,
        Result,
        Builtin,
        Const,
        Global,
    };

    struct Access {
       private:
        std::variant<SpecialObject, ValueArray, ValueMap, Assignable, std::uint64_t, std::u32string> value;

       public:
        Access(std::uint64_t v)
            : value(v) {
        }
        Access(std::u32string v)
            : value(v) {
        }

        Access(SpecialObject v)
            : value(v) {
        }

        Access(Assignable v)
            : value(v) {
        }

        Access(ValueMap v)
            : value(std::move(v)) {
        }

        Access(ValueArray v)
            : value(std::move(v)) {
        }

        std::optional<Assignable> assignable() const {
            if (auto p = std::get_if<Assignable>(&value)) {
                return *p;
            }
            return std::nullopt;
        }

        std::optional<SpecialObject> special_object() const {
            if (auto p = std::get_if<SpecialObject>(&value)) {
                return *p;
            }
            return std::nullopt;
        }

        std::optional<std::u32string> key() const {
            if (auto p = std::get_if<std::u32string>(&value)) {
                return *p;
            }
            return std::nullopt;
        }

        std::optional<size_t> index() const {
            if (auto p = std::get_if<size_t>(&value)) {
                return *p;
            }
            return std::nullopt;
        }

        ValueMap* object() {
            if (auto p = std::get_if<ValueMap>(&value)) {
                return p;
            }
            return nullptr;
        }

        ValueArray* array() {
            if (auto p = std::get_if<ValueArray>(&value)) {
                return p;
            }
            return nullptr;
        }
    };

}  // namespace expr

struct Block {
    size_t start;
    BlockType type;
    std::shared_ptr<expr::Expr> condition;
};

struct RuntimeState;

struct EvalValue {
   private:
    std::variant<Value,
                 Assignable,
                 std::vector<expr::Access>>
        value;

   public:
    EvalValue(std::nullptr_t) {}

    EvalValue(std::u32string v)
        : value(Value(v)) {
    }

    EvalValue(Assignable v)
        : value(v) {
    }

    EvalValue(std::int64_t v)
        : value(Value(v)) {
    }

    EvalValue(Boolean v)
        : value(Value(v)) {
    }

    EvalValue(Func v)
        : value(Value(v)) {
    }

    EvalValue(Bytes v)
        : value(Value(v)) {
    }

    EvalValue(Value v)
        : value(std::move(v)) {
    }

    EvalValue(std::vector<expr::Access> v)
        : value(std::move(v)) {
    }

    EvalValue(ValueArray v)
        : value(Value(std::move(v))) {
    }

    EvalValue(ValueMap v)
        : value(Value(std::move(v))) {
    }

    Value* get_value() {
        if (auto p = std::get_if<Value>(&value)) {
            return p;
        }
        return nullptr;
    }

    const Value* get_value() const {
        if (auto p = std::get_if<Value>(&value)) {
            return p;
        }
        return nullptr;
    }

    bool null() const {
        if (auto p = get_value()) {
            return p->null();
        }
        return false;
    }

    std::optional<std::u32string> string() const {
        if (auto p = get_value()) {
            return p->string();
        }
        return std::nullopt;
    }

    std::optional<Assignable> assignable() const {
        if (auto p = std::get_if<Assignable>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    std::optional<std::vector<expr::Access>> access() const {
        if (auto p = std::get_if<std::vector<expr::Access>>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    std::optional<std::int64_t> number() const {
        if (auto p = get_value()) {
            return p->number();
        }
        return std::nullopt;
    }

    std::optional<Boolean> boolean() const {
        if (auto p = get_value()) {
            return p->boolean();
        }
        return std::nullopt;
    }

    ValueArray* array() {
        if (auto p = get_value()) {
            return p->array();
        }
        return nullptr;
    }

    ValueMap* object() {
        if (auto p = get_value()) {
            return p->object();
        }
        return nullptr;
    }

    const Func* func() const {
        if (auto p = get_value()) {
            return p->func();
        }
        return nullptr;
    }

    Func* func() {
        if (auto p = get_value()) {
            return p->func();
        }
        return nullptr;
    }

    std::optional<Value> eval(RuntimeState& s);
};

struct EvalStacks {
    std::vector<std::shared_ptr<expr::Expr>> expr_stack;
    std::vector<EvalValue> eval_stack;
    std::shared_ptr<expr::Expr> root_expr;

    void clear() {
        expr_stack.clear();
        eval_stack.clear();
        root_expr.reset();
    }
};

struct Stacks {
    std::vector<Block> blocks;
    std::vector<Value> args;
    EvalStacks eval;

    void clear() {
        blocks.clear();
        args.clear();
        eval.clear();
    }
};
struct CallStack {
    size_t back_pos;
    Stacks stacks;
    std::shared_ptr<Scope> function_scope;
    bool no_return_value = false;
};

struct ScriptLineMap {
    futils::view::rvec script_name;
    size_t line = 0;
};

struct LineMappedCommand {
    ScriptLineMap line_map;
    std::vector<futils::view::rvec> cmd;
};

struct BuiltinEvalExpr {
    futils::view::swap_wvec expr;
    size_t back_line = 0;
    EvalStacks stacks;
    size_t call_stack_size = 0;
    size_t foreign_callback_count = 0;

    BuiltinEvalExpr(futils::view::swap_wvec e, size_t l, EvalStacks s, size_t c, size_t f)
        : expr(std::move(e)), back_line(l), stacks(std::move(s)), call_stack_size(c), foreign_callback_count(f) {
    }

    BuiltinEvalExpr(BuiltinEvalExpr&&) = default;

    ~BuiltinEvalExpr() {
        if (!expr.w.null()) {
            delete[] expr.w.data();
            expr.w = {};
        }
    }
};

struct ForeignCallbackStack {
    std::shared_ptr<foreign::ForeignLibrary> lib;
    std::u32string func_name;
    ValueArray args;
    Value result;
    size_t back_pos;
    size_t callback_count = 0;
};

struct AsyncCallback {
    Func func;
    std::vector<Value> args;
};

template <class T>
using AsyncChannel = futils::thread::MultiProducerChannelBuffer<T, std::allocator<T>>;

using AsyncFuncChannel = AsyncChannel<AsyncCallback>;

struct RuntimeState {
    io::TextController& text;
    save::SaveData& save;
    futils::wrap::path_string& save_data_path;
    Value save_data_storage;
    // this is for debug
    futils::view::rvec current_script_name;
    size_t current_script_line = 0;
    std::shared_ptr<AsyncFuncChannel> async_callback_channel;

    bool jump_from_if = false;
    size_t text_speed = 100;
    size_t after_wait = 1000;
    size_t delay_offset = 0;
    std::u32string story_text;
    std::u32string prompt_result;
    std::map<std::u32string, Value> global_variables;
    std::shared_ptr<foreign::ForeignData> foreign_data;

    GameEndRequest game_end_request = GameEndRequest::none;
    GameConfig config;

    std::vector<CallStack> call_stack;

    std::vector<ForeignCallbackStack> foreign_callback_stack;

    Stacks stacks;

    bool special_object_result = false;
    std::u32string special_object_error_reason;
    std::u32string eval_error_reason;

    // this indicates actual commends and their index
    std::vector<LineMappedCommand> cmds;
    size_t cmd_line = 0;

    std::vector<BuiltinEvalExpr> builtin_eval_stack;

    expr::EvalState eval_error(std::u32string_view reason) {
        eval_error_reason = reason;
        return expr::EvalState::error;
    }

    expr::EvalState eval_error(i8n::Message msg, std::u32string_view reason) {
        eval_error_reason = reason;
        return expr::EvalState::error;
    }

    Value* get_variable(const std::u32string& s) {
        if (call_stack.size()) {
            auto scope = call_stack.back().function_scope;
            if (scope) {
                if (auto it = scope->variables.find(s); it != scope->variables.end()) {
                    return it->second.get();
                }
            }
        }
        if (auto it = global_variables.find(s); it != global_variables.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void set_variable(const std::u32string& s, Value value) {
        if (call_stack.size()) {
            auto scope = call_stack.back().function_scope;
            if (auto it = scope->variables.find(s); it != scope->variables.end()) {
                *it->second = value;
                return;
            }
            scope->variables[s] = std::make_shared<Value>(std::move(value));
            return;
        }
        global_variables[s] = value;
    }
};
