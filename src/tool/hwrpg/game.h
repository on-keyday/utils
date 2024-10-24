/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <wrap/cout.h>
#include <wrap/cin.h>
#include <console/window.h>
#include <console/ansiesc.h>
#include <filesystem>
#include <thread>
#include "save_data.h"
#include <json/convert_json.h>
#include <json/json_export.h>
#include <set>

extern futils::wrap::UtfOut& cout;
extern futils::wrap::UtfIn& cin;
extern bool signaled;
constexpr auto text_layout = futils::console::make_layout_data<80, 10>(2);
constexpr auto center_layout = futils::console::make_layout_data<80, 10>(4);
namespace ansiesc = futils::console::escape;
namespace fs = std::filesystem;
struct TextController {
    futils::console::Window layout;
    void write_text(futils::view::rvec data) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        layout.draw(w, data);
        cout << w.written();
    }

    void write_title(futils::view::rvec title) {
        write_text(title);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

   private:
    void move_back() {
        cout << ansiesc::cursor(ansiesc::CursorMove::up, layout.dy());
    }

   public:
    auto write_story(std::u32string_view text, size_t delay = 100, size_t after_wait = 1000, size_t delay_offset = 0) {
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        move_back();
        if (delay_offset == 0) {
            write_text("");
        }
        if (delay == 0) {
            move_back();
            write_text(futils::utf::convert<std::string>(text));
            std::this_thread::sleep_for(std::chrono::milliseconds(after_wait));
            return;
        }
        std::string output;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        for (size_t i = 0; i < text.size(); i++) {
            if (i < delay_offset) {
                continue;
            }
            output = futils::utf::convert<std::string>(text.substr(0, i + 1));
            move_back();
            write_text(output);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(after_wait));
    };

    auto show_cursor() {
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
    }

    void clear_screen() {
        cout << ansiesc::erase(ansiesc::EraseTarget::screen, ansiesc::EraseMode::all);
        cout << ansiesc::cursor_abs(0, 0);
    }

    auto write_prompt(std::u32string_view text, size_t delay = 100, size_t after_wait = 1000, size_t delay_offset = 0) {
        write_story(text, delay, after_wait, delay_offset);
        cout << ansiesc::erase(ansiesc::EraseTarget::line, ansiesc::EraseMode::back_of_cursor);
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::show);
        cout << "> ";
        std::string answer;
        cin >> answer;
        while (!answer.empty() && (answer.back() == '\n' || answer.back() == '\r')) {
            answer.pop_back();
        }
        cout << ansiesc::cursor_visibility(ansiesc::CursorVisibility::hide);
        cout << ansiesc::cursor(ansiesc::CursorMove::up, 1);
        return answer;
    };
};

int make_embed(const char* target_dir, const char* embed_file, const char* embed_index);
int verify_embed(const char* dataFile, const char* indexFile);
int load_embed(const char* dataFile, const char* indexFile, TextController& text);
void unload_embed();
enum class GameEndRequest {
    none,
    exit,
    failure,
    interrupt,
    reload,
};
GameEndRequest run_game(TextController& ctrl, save::SaveData& data, const char* dataFile, const char* indexFile, futils::wrap::path_string& save_data_name);

futils::view::rvec read_file(futils::view::rvec file, TextController& err);

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

struct Value {
   private:
    std::variant<std::monostate, std::u32string, std::int64_t, Boolean, Func, ValueArray, ValueMap> value;

   public:
    Value() {}
    Value(std::u32string v)
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

    std::optional<Func> func() const {
        if (auto p = std::get_if<Func>(&value)) {
            return *p;
        }
        return std::nullopt;
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

    enum class EvalState {
        normal,
        call,
        error,
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
    std::variant<std::monostate, std::u32string, std::int64_t, Boolean, Func,
                 Assignable,
                 std::vector<expr::Access>,
                 ValueArray,
                 ValueMap>
        value;

   public:
    EvalValue(std::nullptr_t) {}

    EvalValue(std::u32string v)
        : value(v) {
    }

    EvalValue(Assignable v)
        : value(v) {
    }

    EvalValue(std::int64_t v)
        : value(v) {
    }

    EvalValue(Boolean v)
        : value(v) {
    }

    EvalValue(Func v)
        : value(v) {
    }

    EvalValue(Value v) {
        if (v.null()) {
            value = std::monostate();
        }
        else if (auto str = v.string()) {
            value = std::move(*str);
        }
        else if (auto n = v.number()) {
            value = *n;
        }
        else if (auto b = v.boolean()) {
            value = *b;
        }
        else if (auto f = v.func()) {
            value = *f;
        }
        else if (auto a = v.array()) {
            value = std::move(*a);
        }
        else if (auto o = v.object()) {
            value = std::move(*o);
        }
    }

    EvalValue(std::vector<expr::Access> v)
        : value(std::move(v)) {
    }

    EvalValue(std::vector<Value> v)
        : value(std::move(v)) {
    }

    EvalValue(std::map<std::u32string, Value> v)
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

    std::optional<Func> func() const {
        if (auto p = std::get_if<Func>(&value)) {
            return *p;
        }
        return std::nullopt;
    }

    std::optional<Value> eval(RuntimeState& s);
};

struct Stacks {
    std::vector<Block> blocks;
    std::vector<Value> args;
    std::vector<std::shared_ptr<expr::Expr>> expr_stack;
    std::vector<EvalValue> eval_stack;
    std::shared_ptr<expr::Expr> root_expr;

    void clear() {
        blocks.clear();
        args.clear();
        expr_stack.clear();
        eval_stack.clear();
        root_expr.reset();
    }
};
struct CallStack {
    size_t back_pos;
    Stacks stacks;
    std::shared_ptr<Scope> function_scope;
};

struct RuntimeState {
    TextController& text;
    save::SaveData& save;
    futils::wrap::path_string& save_data_path;
    futils::view::rvec current_script_name;
    size_t current_script_line = 0;
    bool jump_from_if = false;
    size_t text_speed = 100;
    size_t after_wait = 1000;
    size_t delay_offset = 0;
    std::u32string story_text;
    std::string prompt_result;
    std::map<std::u32string, Value> global_variables;

    GameEndRequest game_end_request = GameEndRequest::none;
    GameConfig config;

    std::vector<CallStack> call_stack;

    Stacks stacks;

    bool special_object_result = false;
    std::u32string special_object_error_reason;
    std::u32string eval_error_reason;

    expr::EvalState eval_error(std::u32string_view reason) {
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
