/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <concepts>
#include <binary/flags.h>
#include <initializer_list>

namespace futils::json {

    enum class ParseStateDetail : unsigned char {
        start,

        begin_string,
        parse_string,
        escape_string,
        end_string,

        begin_number,
        parse_int_begin,
        parse_int,
        parse_after_zero,
        parse_frac_begin,
        parse_frac,
        parse_exp_begin,
        parse_exp_after_sign,
        parse_exp,
        end_number,

        begin_true,
        end_true,
        begin_false,
        end_false,
        begin_null,
        end_null,

        begin_array,
        begin_array_space,
        begin_end_array,
        parse_array_space,
        parse_array_element,
        parse_array_element_space,
        parse_array_separator,
        end_array,

        begin_object,
        begin_object_space,
        begin_end_object,
        parse_object_space,
        begin_object_key,
        parse_key_string,
        escape_key_string,
        parse_object_key_space,
        parse_object_key_value_separator,
        parse_object_key_value_separator_space,
        parse_object_value,
        parse_object_value_space,
        parse_object_separator,
        end_object,

        invalid_state,
        size_too_small,
        char_out_of_range,
        unexpected_token,
        unexpected_eof,
        stack_overflow,
        unexpected_fail,
    };

    constexpr const char* to_string(ParseStateDetail detail) {
        switch (detail) {
            case ParseStateDetail::start:
                return "start";
            case ParseStateDetail::begin_string:
                return "begin_string";
            case ParseStateDetail::parse_string:
                return "parse_string";
            case ParseStateDetail::escape_string:
                return "escape_string";
            case ParseStateDetail::end_string:
                return "end_string";
            case ParseStateDetail::begin_number:
                return "begin_number";
            case ParseStateDetail::parse_int_begin:
                return "parse_int_begin";
            case ParseStateDetail::parse_int:
                return "parse_int";
            case ParseStateDetail::parse_after_zero:
                return "parse_after_zero";
            case ParseStateDetail::parse_frac_begin:
                return "parse_frac_begin";
            case ParseStateDetail::parse_frac:
                return "parse_frac";
            case ParseStateDetail::parse_exp_begin:
                return "parse_exp_begin";
            case ParseStateDetail::parse_exp_after_sign:
                return "parse_exp_after_sign";
            case ParseStateDetail::parse_exp:
                return "parse_exp";
            case ParseStateDetail::end_number:
                return "end_number";
            case ParseStateDetail::begin_true:
                return "begin_true";
            case ParseStateDetail::end_true:
                return "end_true";
            case ParseStateDetail::begin_false:
                return "begin_false";
            case ParseStateDetail::end_false:
                return "end_false";
            case ParseStateDetail::begin_null:
                return "begin_null";
            case ParseStateDetail::end_null:
                return "end_null";

            case ParseStateDetail::begin_array:
                return "begin_array";
            case ParseStateDetail::begin_array_space:
                return "begin_array_space";
            case ParseStateDetail::begin_end_array:
                return "begin_end_array";
            case ParseStateDetail::parse_array_space:
                return "parse_array_space";
            case ParseStateDetail::parse_array_element:
                return "parse_array_element";
            case ParseStateDetail::parse_array_element_space:
                return "parse_array_element_space";
            case ParseStateDetail::parse_array_separator:
                return "parse_array_separator";
            case ParseStateDetail::end_array:
                return "end_array";

            case ParseStateDetail::begin_object:
                return "begin_object";
            case ParseStateDetail::begin_object_space:
                return "begin_object_space";
            case ParseStateDetail::begin_end_object:
                return "begin_end_object";
            case ParseStateDetail::parse_object_space:
                return "parse_object_space";
            case ParseStateDetail::begin_object_key:

                return "begin_object_key";
            case ParseStateDetail::parse_key_string:
                return "parse_key_string";
            case ParseStateDetail::escape_key_string:
                return "escape_key_string";
            case ParseStateDetail::parse_object_key_space:
                return "parse_object_key_space";
            case ParseStateDetail::parse_object_key_value_separator:
                return "parse_object_key_value_separator";
            case ParseStateDetail::parse_object_key_value_separator_space:
                return "parse_object_key_value_separator_space";
            case ParseStateDetail::parse_object_value:
                return "parse_object_value";
            case ParseStateDetail::parse_object_value_space:
                return "parse_object_value_space";
            case ParseStateDetail::parse_object_separator:
                return "parse_object_separator";
            case ParseStateDetail::end_object:
                return "end_object";
            case ParseStateDetail::invalid_state:
                return "invalid_state";
            case ParseStateDetail::size_too_small:
                return "size_too_small";
            case ParseStateDetail::char_out_of_range:
                return "char_out_of_range";
            case ParseStateDetail::unexpected_token:
                return "unexpected_token";
            case ParseStateDetail::unexpected_eof:
                return "unexpected_eof";
            case ParseStateDetail::stack_overflow:
                return "stack_overflow";
            case ParseStateDetail::unexpected_fail:
                return "unexpected_fail";
            default:
                return "unknown";
        }
    }

    enum class ElementType : unsigned char {
        null,
        boolean,
        string,
        escaped_string,
        key_string,
        escaped_key_string,
        object_field,
        array_element,
        number,  // begin with number, but end with integer or floating
        integer,
        floating,
        array,
        object,
    };

    constexpr const char* to_string(ElementType type) {
        switch (type) {
            case ElementType::null:
                return "null";
            case ElementType::boolean:
                return "boolean";
            case ElementType::string:
                return "string";
            case ElementType::escaped_string:
                return "escaped_string";
            case ElementType::key_string:
                return "key_string";
            case ElementType::escaped_key_string:
                return "escaped_key_string";
            case ElementType::object_field:
                return "object_field";
            case ElementType::array_element:
                return "array_element";
            case ElementType::number:
                return "number";
            case ElementType::integer:
                return "integer";
            case ElementType::floating:
                return "floating";
            case ElementType::array:
                return "array";
            case ElementType::object:
                return "object";
            default:
                return "unknown";
        }
    }

    template <class P>
    concept Reader = requires(P p) {
        { p.pop_next() };
        { p.readable_size() } -> std::convertible_to<size_t>;
        { p.template consume<0>("") } -> std::convertible_to<bool>;
        { p.eof() } -> std::convertible_to<bool>;
    };

    template <class P>
    concept StateManager = requires(P p) {
        { p.push_state(ParseStateDetail{}) } -> std::convertible_to<bool>;
        { p.pop_state() } -> std::convertible_to<ParseStateDetail>;
        { p.save_begin(ElementType{}) } -> std::convertible_to<bool>;
        { p.save_end(ElementType{}) } -> std::convertible_to<bool>;
    };

    template <class P>
    concept ReaderStateManager = Reader<P> && StateManager<P>;

    struct ParserState {
       private:
        binary::flags_t<std::uint32_t, 24, 6, 1, 1> value;
        bits_flag_alias_method(value, 0, prev_char_);
        bits_flag_alias_method_with_enum(value, 1, state_, ParseStateDetail);
        bits_flag_alias_method(value, 2, cached);
        bits_flag_alias_method(value, 3, has_escaped);

       public:
        constexpr void read(auto& reader) {
            if (use_cache()) {
                return;
            }
            prev_char_(reader.pop_next());
        }

        constexpr void reset() {
            value = 0;
        }

        constexpr ParseStateDetail state() const {
            return state_();
        }

        constexpr void set_state(ParseStateDetail new_state) {
            state_(new_state);
        }
        constexpr std::uint32_t prev_char() const {
            return prev_char_();
        }

        constexpr void unread() {
            cached(true);
        }

        constexpr size_t readable_size(auto& reader) {
            return reader.readable_size() + (cached() ? 1 : 0);
        }

        constexpr void set_escaped() {
            has_escaped(true);
        }

        constexpr bool get_and_clear_escaped() {
            if (has_escaped()) {
                has_escaped(false);
                return true;
            }
            return false;
        }

       private:
        constexpr bool use_cache() {
            if (cached()) {
                cached(false);
                return true;
            }
            return false;
        }
    };

    enum class ParseResult {
        end,
        suspend,
        error,
        require_nest,
    };

    constexpr const char* to_string(ParseResult result) {
        switch (result) {
            case ParseResult::end:
                return "end";
            case ParseResult::suspend:
                return "suspend";
            case ParseResult::error:
                return "error";
            case ParseResult::require_nest:
                return "require_nest";
            default:
                return "unknown";
        }
    }

    template <ReaderStateManager T>
    struct Parser {
        T reader;
        ParserState state;

       private:
        constexpr std::uint32_t get_next() {
            state.read(reader);
            return state.prev_char();
        }

        constexpr bool save_begin(ElementType type) {
            if (!reader.save_begin(type)) {
                state.set_state(ParseStateDetail::unexpected_fail);
                return false;
            }
            return true;
        }

        constexpr bool save_end(ElementType type) {
            if (!reader.save_end(type)) {
                state.set_state(ParseStateDetail::unexpected_fail);
                return false;
            }
            return true;
        }

        constexpr void next_start() {
            switch (get_next()) {
                case '\"':
                    state.set_state(ParseStateDetail::begin_string);
                    save_begin(ElementType::string);
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '-':
                    state.set_state(ParseStateDetail::begin_number);
                    save_begin(ElementType::number);
                    break;
                case 't':
                    state.set_state(ParseStateDetail::begin_true);
                    save_begin(ElementType::boolean);
                    break;
                case 'f':
                    state.set_state(ParseStateDetail::begin_false);
                    save_begin(ElementType::boolean);
                    break;
                case 'n':
                    state.set_state(ParseStateDetail::begin_null);
                    save_begin(ElementType::null);
                    break;
                case '[':
                    state.set_state(ParseStateDetail::begin_array);
                    save_begin(ElementType::array);
                    break;
                case '{':
                    state.set_state(ParseStateDetail::begin_object);
                    save_begin(ElementType::object);
                    break;
                default:
                    state.set_state(ParseStateDetail::unexpected_token);
            }
        }

        constexpr bool may_skip_space(size_t& size) {
            while (size) {
                auto prev_char = get_next();
                if (prev_char == ' ' || prev_char == '\t' || prev_char == '\n' || prev_char == '\r') {
                    size--;
                    continue;
                }
                state.unread();
                return true;
            }
            return false;
        }

        constexpr void may_eof() {
            if (reader.eof()) {
                switch (state.state()) {
                    case ParseStateDetail::begin_number:
                    case ParseStateDetail::parse_int:
                    case ParseStateDetail::parse_after_zero:
                        state.set_state(ParseStateDetail::end_number);
                        save_end(ElementType::number);
                        break;
                    case ParseStateDetail::parse_frac:
                    case ParseStateDetail::parse_exp:
                        state.set_state(ParseStateDetail::end_number);
                        save_end(ElementType::number);
                        break;
                    default:
                        break;
                }
            }
        }

        constexpr void parse_number() {
            switch (state.state()) {
                case ParseStateDetail::begin_number: {  // at here, after next_start(), so we have a char
                    // may be there are a minus sign
                    auto prev_char = state.prev_char();
                    if (prev_char != '-') {  // if a numeric char
                        state.unread();
                    }
                    state.set_state(ParseStateDetail::parse_int_begin);
                    break;
                }
                case ParseStateDetail::parse_int_begin: {
                    auto c = get_next();
                    if (c == '0') {
                        state.set_state(ParseStateDetail::parse_after_zero);
                    }
                    else if (c >= '1' && c <= '9') {
                        state.set_state(ParseStateDetail::parse_int);
                    }
                    else {
                        state.set_state(ParseStateDetail::char_out_of_range);
                    }
                    break;
                }
                case ParseStateDetail::parse_int: {
                    auto c = get_next();
                    if (c == '.') {
                        state.set_state(ParseStateDetail::parse_frac_begin);
                    }
                    else if (c == 'e' || c == 'E') {
                        state.set_state(ParseStateDetail::parse_exp_begin);
                    }
                    else if (c < '0' || c > '9') {
                        // if not a number, then we are at the end of number
                        state.set_state(ParseStateDetail::end_number);
                        state.unread();
                        save_end(ElementType::integer);
                    }
                    break;
                }
                case ParseStateDetail::parse_after_zero: {
                    auto c = get_next();
                    if (c == '.') {
                        state.set_state(ParseStateDetail::parse_frac_begin);
                    }
                    else if (c == 'e' || c == 'E') {
                        state.set_state(ParseStateDetail::parse_exp_begin);
                    }
                    else {  // if not a number, then we are at the end of number
                        state.set_state(ParseStateDetail::end_number);
                        state.unread();
                        save_end(ElementType::integer);
                    }
                    break;
                }
                case ParseStateDetail::parse_frac_begin: {
                    auto c = get_next();
                    if (c >= '0' && c <= '9') {  // at least one digit after '.'
                        state.set_state(ParseStateDetail::parse_frac);
                    }
                    else {
                        state.set_state(ParseStateDetail::char_out_of_range);
                    }
                    break;
                }
                case ParseStateDetail::parse_frac: {
                    auto c = get_next();
                    if (c == 'e' || c == 'E') {
                        state.set_state(ParseStateDetail::parse_exp_begin);
                    }
                    else if (c < '0' || c > '9') {  // if not a number, then we are at the end of number
                        state.set_state(ParseStateDetail::end_number);
                        save_end(ElementType::floating);
                        state.unread();
                    }
                    break;
                }
                case ParseStateDetail::parse_exp_begin: {  // after 'e' or 'E'
                    auto c = get_next();
                    if (c == '+' || c == '-') {
                        state.set_state(ParseStateDetail::parse_exp_after_sign);
                    }
                    else if (c >= '0' && c <= '9') {
                        state.set_state(ParseStateDetail::parse_exp);
                    }
                    else {
                        state.set_state(ParseStateDetail::char_out_of_range);
                    }
                    break;
                }
                case ParseStateDetail::parse_exp_after_sign: {
                    auto c = get_next();
                    if (c >= '0' && c <= '9') {  // at least one digit after 'e' or 'E'
                        state.set_state(ParseStateDetail::parse_exp);
                    }
                    else {
                        state.set_state(ParseStateDetail::char_out_of_range);
                    }
                    break;
                }
                case ParseStateDetail::parse_exp: {
                    auto c = get_next();
                    if (c < '0' || c > '9') {  // if not a number, then we are at the end of number
                        state.set_state(ParseStateDetail::end_number);
                        save_end(ElementType::floating);
                        state.unread();
                    }
                    break;
                }
                default:
                    state.set_state(ParseStateDetail::invalid_state);
                    break;
            }
        }

        constexpr void parse_string() {
            switch (state.state()) {
                case ParseStateDetail::begin_string: {
                    state.set_state(ParseStateDetail::parse_string);
                    [[fallthrough]];
                }
                case ParseStateDetail::parse_string:
                case ParseStateDetail::parse_key_string: {
                    auto prev_char = state.prev_char();
                    auto current_char = get_next();
                    if (current_char == '\"') {
                        auto elem_type = state.state() == ParseStateDetail::parse_string
                                             ? (state.get_and_clear_escaped() ? ElementType::escaped_string : ElementType::string)
                                             : (state.get_and_clear_escaped() ? ElementType::escaped_key_string : ElementType::key_string);
                        state.set_state(state.state() == ParseStateDetail::parse_string ? ParseStateDetail::end_string : ParseStateDetail::parse_object_key_space);
                        save_end(elem_type);
                    }
                    else if (current_char == '\\') {
                        state.set_state(state.state() == ParseStateDetail::parse_string ? ParseStateDetail::escape_string : ParseStateDetail::escape_key_string);
                        state.set_escaped();
                    }
                    break;
                }
                case ParseStateDetail::escape_string:
                case ParseStateDetail::escape_key_string: {
                    get_next();
                    state.set_state(state.state() == ParseStateDetail::escape_string ? ParseStateDetail::parse_string : ParseStateDetail::parse_key_string);
                    break;
                }
                default:
                    state.set_state(ParseStateDetail::invalid_state);
                    break;
            }
        }

        constexpr void parse_array(size_t& size, bool& inner) {
            switch (state.state()) {
                case ParseStateDetail::begin_array:
                    state.set_state(ParseStateDetail::begin_array_space);
                    [[fallthrough]];
                case ParseStateDetail::begin_array_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::begin_end_array);
                    break;
                case ParseStateDetail::parse_array_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    if (!save_begin(ElementType::array_element)) {
                        return;
                    }
                    state.set_state(ParseStateDetail::parse_array_element);
                    inner = true;
                    break;
                case ParseStateDetail::begin_end_array: {
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == ']') {
                        state.set_state(ParseStateDetail::end_array);
                        save_end(ElementType::array);
                        inner = true;
                        break;
                    }
                    state.unread();
                    state.set_state(ParseStateDetail::parse_array_element);
                    inner = true;

                    break;
                }
                case ParseStateDetail::parse_array_element_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::parse_array_separator);
                    inner = size >= 1;
                    break;
                case ParseStateDetail::parse_array_separator: {
                    if (size < 1) {
                        return;
                    }
                    if (!save_end(ElementType::array_element)) {
                        return;
                    }
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == ']') {
                        state.set_state(ParseStateDetail::end_array);
                        save_end(ElementType::array);
                        inner = true;
                        break;
                    }
                    else if (prev_char == ',') {
                        state.set_state(ParseStateDetail::begin_array_space);
                        inner = true;
                        break;
                    }
                    else {
                        state.set_state(ParseStateDetail::unexpected_token);
                    }
                }
                default:
                    state.set_state(ParseStateDetail::invalid_state);
                    break;
            }
        }

        constexpr void parse_object(size_t& size, bool& inner) {
            switch (state.state()) {
                case ParseStateDetail::begin_object:
                    state.set_state(ParseStateDetail::begin_object_space);
                    [[fallthrough]];
                case ParseStateDetail::begin_object_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::begin_end_object);
                    break;
                case ParseStateDetail::begin_end_object: {
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == '}') {
                        state.set_state(ParseStateDetail::end_object);
                        save_end(ElementType::object);
                        inner = true;
                        break;
                    }
                    state.unread();
                    state.set_state(ParseStateDetail::begin_object_key);
                    inner = true;
                    break;
                }
                case ParseStateDetail::parse_object_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::begin_object_key);
                    inner = true;
                    break;
                case ParseStateDetail::begin_object_key: {
                    if (!save_begin(ElementType::object_field)) {
                        return;
                    }
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == '\"') {
                        state.set_state(ParseStateDetail::parse_key_string);
                        save_begin(ElementType::key_string);
                        inner = true;
                    }
                    else {
                        state.set_state(ParseStateDetail::unexpected_token);
                    }
                    break;
                }
                case ParseStateDetail::parse_object_key_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::parse_object_key_value_separator);
                    inner = size >= 1;
                    break;
                case ParseStateDetail::parse_object_key_value_separator: {
                    if (size < 1) {
                        return;
                    }
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == ':') {
                        state.set_state(ParseStateDetail::parse_object_key_value_separator_space);
                        inner = true;
                    }
                    else {
                        state.set_state(ParseStateDetail::unexpected_token);
                    }
                    break;
                }
                case ParseStateDetail::parse_object_key_value_separator_space: {
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::parse_object_value);
                    inner = true;
                    break;
                }
                case ParseStateDetail::parse_object_value_space:
                    if (!may_skip_space(size)) {
                        break;
                    }
                    state.set_state(ParseStateDetail::parse_object_separator);
                    inner = size >= 1;
                    break;
                case ParseStateDetail::parse_object_separator: {
                    if (size < 1) {
                        return;
                    }
                    if (!save_end(ElementType::object_field)) {
                        return;
                    }
                    size--;
                    auto prev_char = get_next();
                    if (prev_char == '}') {
                        state.set_state(ParseStateDetail::end_object);
                        save_end(ElementType::object);
                        inner = true;
                    }
                    else if (prev_char == ',') {
                        state.set_state(ParseStateDetail::parse_object_space);
                        inner = true;
                    }
                    else {
                        state.set_state(ParseStateDetail::unexpected_token);
                    }
                    break;
                }
                default:
                    state.set_state(ParseStateDetail::invalid_state);
                    break;
            }
        }

        constexpr ParseStateDetail next_impl() {
            while (true) {
                size_t size = state.readable_size(reader);
                if (size == 0) {
                    may_eof();
                    return state.state();
                }
                bool inner = true;
                while (inner) {
                    inner = false;
                    switch (state.state()) {
                        case ParseStateDetail::start:
                            next_start();
                            break;
                        case ParseStateDetail::begin_string:
                        case ParseStateDetail::parse_string:
                        case ParseStateDetail::parse_key_string:
                        case ParseStateDetail::escape_string:
                        case ParseStateDetail::escape_key_string:
                            parse_string();
                            break;
                        case ParseStateDetail::begin_number:
                        case ParseStateDetail::parse_int_begin:
                        case ParseStateDetail::parse_int:
                        case ParseStateDetail::parse_after_zero:
                        case ParseStateDetail::parse_frac_begin:
                        case ParseStateDetail::parse_frac:
                        case ParseStateDetail::parse_exp_begin:
                        case ParseStateDetail::parse_exp_after_sign:
                        case ParseStateDetail::parse_exp:
                            parse_number();
                            break;
                        case ParseStateDetail::begin_true:
                            if (size < 3) {
                                return ParseStateDetail::size_too_small;
                            }
                            if (!reader.template consume<3>("rue")) {
                                state.set_state(ParseStateDetail::unexpected_token);
                            }
                            else {
                                state.set_state(ParseStateDetail::end_true);
                                save_end(ElementType::boolean);
                            }
                            break;
                        case ParseStateDetail::begin_false:
                            if (size < 4) {
                                return ParseStateDetail::size_too_small;
                            }
                            if (!reader.template consume<4>("alse")) {
                                state.set_state(ParseStateDetail::unexpected_token);
                            }
                            else {
                                state.set_state(ParseStateDetail::end_false);
                                save_end(ElementType::boolean);
                            }
                            break;
                        case ParseStateDetail::begin_null:
                            if (size < 3) {
                                return ParseStateDetail::size_too_small;
                            }
                            if (!reader.template consume<3>("ull")) {
                                state.set_state(ParseStateDetail::unexpected_token);
                            }
                            else {
                                state.set_state(ParseStateDetail::end_null);
                                save_end(ElementType::null);
                            }
                            break;
                        case ParseStateDetail::begin_array:
                        case ParseStateDetail::begin_array_space:
                        case ParseStateDetail::begin_end_array:
                        case ParseStateDetail::parse_array_space:
                        case ParseStateDetail::parse_array_element_space:
                        case ParseStateDetail::parse_array_separator:
                            parse_array(size, inner);
                            break;
                        case ParseStateDetail::begin_object:
                        case ParseStateDetail::begin_object_space:
                        case ParseStateDetail::begin_end_object:
                        case ParseStateDetail::parse_object_space:
                        case ParseStateDetail::parse_object_key_space:
                        case ParseStateDetail::begin_object_key:
                        case ParseStateDetail::parse_object_key_value_separator:
                        case ParseStateDetail::parse_object_key_value_separator_space:
                        case ParseStateDetail::parse_object_value_space:
                        case ParseStateDetail::parse_object_separator:
                            parse_object(size, inner);
                            break;
                        default:
                            return state.state();
                    }
                }
            }
        }

       public:
        constexpr bool enter_nest() {
            auto state = this->state.state();
            if (state == ParseStateDetail::parse_array_element || state == ParseStateDetail::parse_object_value) {
                if (!reader.push_state(state)) {
                    this->state.set_state(ParseStateDetail::stack_overflow);
                    return false;
                }
                this->state.set_state(ParseStateDetail::start);
                return true;
            }
            return false;
        }

        constexpr bool leave_nest() {
            switch (this->state.state()) {
                case ParseStateDetail::end_string:
                case ParseStateDetail::end_number:
                case ParseStateDetail::end_true:
                case ParseStateDetail::end_false:
                case ParseStateDetail::end_null:
                case ParseStateDetail::end_array:
                case ParseStateDetail::end_object: {
                    auto state = this->reader.pop_state();
                    if (state == ParseStateDetail::parse_array_element) {
                        this->state.set_state(ParseStateDetail::parse_array_element_space);
                        return true;
                    }
                    else if (state == ParseStateDetail::parse_object_value) {
                        this->state.set_state(ParseStateDetail::parse_object_value_space);
                        return true;
                    }
                    else {
                        this->state.set_state(ParseStateDetail::unexpected_token);
                        return false;
                    }
                }
                default:
                    return false;
            }
        }

        constexpr void skip_space() {
            size_t size = state.readable_size(reader);
            if (size > 0) {
                may_skip_space(size);
            }
            if (size == 0) {
                may_eof();
            }
        }

        constexpr ParseResult next() {
            auto impl = next_impl();
            switch (impl) {
                case ParseStateDetail::end_string:
                case ParseStateDetail::end_number:
                case ParseStateDetail::end_true:
                case ParseStateDetail::end_false:
                case ParseStateDetail::end_null:
                case ParseStateDetail::end_array:
                case ParseStateDetail::end_object:
                    return ParseResult::end;
                case ParseStateDetail::parse_array_element:
                case ParseStateDetail::parse_object_value:
                    return ParseResult::require_nest;
                case ParseStateDetail::unexpected_token:
                case ParseStateDetail::unexpected_eof:
                case ParseStateDetail::char_out_of_range:
                case ParseStateDetail::invalid_state:
                case ParseStateDetail::stack_overflow:
                case ParseStateDetail::unexpected_fail:
                    return ParseResult::error;
                default:
                    if (reader.eof()) {
                        return ParseResult::error;
                    }
                    else {
                        return ParseResult::suspend;
                    }
            }
        }

        constexpr ParseResult parse() {
            for (;;) {
                auto result = next();
                if (result == ParseResult::end) {
                    if (!leave_nest()) {
                        return ParseResult::end;
                    }
                }
                else if (result == ParseResult::require_nest) {
                    if (!enter_nest()) {
                        return ParseResult::error;
                    }
                }
                else {
                    return result;
                }
            }
        }
    };
    namespace test {
        struct BeginEndRecord {
            size_t pos = 0;
            ElementType type = ElementType::null;
            bool begin = false;
            unsigned char c = 0;
        };

        struct CharReader {
            const char* str;
            size_t size;
            size_t pos = 0;
            ParseStateDetail nest[10] = {};
            size_t nest_pos = 0;
            constexpr static size_t max_record = 50;
            BeginEndRecord begin_end_record[max_record] = {};
            size_t begin_end_record_pos = 0;

            constexpr CharReader(const char* s) : str(s), pos(0) {
                for (size = 0; str[size] != '\0'; ++size) {
                }
            }

            constexpr char pop_next() {
                if (pos >= size) {
                    return 0;
                }
                return str[pos++];
            }

            constexpr size_t readable_size() const {
                return size - pos;
            }

            template <size_t N>
            constexpr bool consume(const char* s) {
                if (readable_size() < N) {
                    return false;
                }
                for (size_t i = 0; i < N; ++i) {
                    if (str[pos + i] != s[i]) {
                        return false;
                    }
                }
                pos += N;
                return true;
            }

            constexpr bool push_state(ParseStateDetail state) {
                if (nest_pos < 10) {
                    nest[nest_pos++] = state;
                }
                else {
                    return false;
                }
                return true;
            }

            constexpr ParseStateDetail pop_state() {
                if (nest_pos > 0) {
                    return nest[--nest_pos];
                }
                return ParseStateDetail::start;
            }

            constexpr bool eof() const {
                return pos >= size;
            }

            constexpr bool save_begin(ElementType typ) {
                if (begin_end_record_pos < max_record) {
                    begin_end_record[begin_end_record_pos].pos = pos - 1;
                    begin_end_record[begin_end_record_pos].type = typ;
                    begin_end_record[begin_end_record_pos].begin = true;
                    begin_end_record[begin_end_record_pos].c = str[pos - 1];
                    ++begin_end_record_pos;
                }
                return true;
            }
            constexpr bool save_end(ElementType typ) {
                if (begin_end_record_pos < max_record) {
                    begin_end_record[begin_end_record_pos].pos = pos - 1;
                    begin_end_record[begin_end_record_pos].type = typ;
                    begin_end_record[begin_end_record_pos].begin = false;
                    begin_end_record[begin_end_record_pos].c = str[pos - 1];
                    ++begin_end_record_pos;
                }
                return true;
            }
        };

        constexpr bool test_parser_primitive() {
            auto test = [](const char* json_str, ParseStateDetail expected_state) {
                CharReader reader(json_str);
                Parser<CharReader> p{reader};
                p.next();
                ParseStateDetail state = p.state.state();
                if (state != expected_state) {
                    [](auto... v) {
                        throw "error";
                    }(state, expected_state, p);
                }
                return true;
            };
            // Test cases
            bool success = test("\"hello\\\"\\\\tanosuke\\ \\n \\r \\t \\u0000 \\\\\\\" \"", ParseStateDetail::end_string) &&
                           test("0", ParseStateDetail::end_number) &&
                           test("0.0", ParseStateDetail::end_number) &&
                           test("0.0e+10", ParseStateDetail::end_number) &&
                           test("0e+10", ParseStateDetail::end_number) &&
                           test("123", ParseStateDetail::end_number) &&
                           test("123.456", ParseStateDetail::end_number) &&
                           test("-123.456e+10", ParseStateDetail::end_number) &&
                           test("123.456E13", ParseStateDetail::end_number) &&
                           test("123e30", ParseStateDetail::end_number) &&
                           test("true", ParseStateDetail::end_true) &&
                           test("false", ParseStateDetail::end_false) &&
                           test("null", ParseStateDetail::end_null) &&
                           test("[]", ParseStateDetail::end_array) &&
                           test("{}", ParseStateDetail::end_object) &&
                           test("[    ]", ParseStateDetail::end_array) &&
                           test("[1, 2, 3]", ParseStateDetail::parse_array_element) &&
                           test("{    }", ParseStateDetail::end_object) &&
                           test("{\"key\": ", ParseStateDetail::parse_object_key_value_separator_space) &&
                           test("{\"key\": \"value\"}", ParseStateDetail::parse_object_value);

            bool error_case = test("12.e10", ParseStateDetail::char_out_of_range) &&
                              test("12.0e", ParseStateDetail::parse_exp_begin) &&
                              test("12.0e+", ParseStateDetail::parse_exp_after_sign) &&
                              test("12.0e-", ParseStateDetail::parse_exp_after_sign) &&
                              test("12.0e+a", ParseStateDetail::char_out_of_range) &&
                              test("hello", ParseStateDetail::unexpected_token) &&
                              test("tone", ParseStateDetail::unexpected_token) &&
                              test("faly", ParseStateDetail::begin_false) &&
                              test("nul", ParseStateDetail::begin_null) &&
                              test("[ ", ParseStateDetail::begin_array_space) &&
                              test("{ ", ParseStateDetail::begin_object_space);
            return success && error_case;
        }

        static_assert(test_parser_primitive(), "Parser test failed");

        constexpr bool test_parser_nest() {
            auto test = [](const char* json_str, std::initializer_list<BeginEndRecord> expected) {
                CharReader reader(json_str);
                Parser<CharReader&> p{reader};
                auto throw_ = [](auto... v) {
                    throw "error";
                };
                while (true) {
                    auto state = p.next();
                    if (state == ParseResult::end) {
                        if (p.leave_nest()) {
                            continue;
                        }
                        else {
                            break;
                        }
                    }
                    if (state == ParseResult::require_nest) {
                        if (!p.enter_nest()) {
                            return false;
                        }
                    }
                    else if (state == ParseResult::suspend) {
                        continue;
                    }
                    else if (state == ParseResult::error) {
                        throw_(p, p.reader.pos, p.state.state(), p.state.prev_char());
                    }
                }
                for (size_t i = 0; i < reader.begin_end_record_pos; ++i) {
                    if (i >= expected.size()) {
                        throw_(i);
                    }
                    auto& record = reader.begin_end_record[i];
                    auto& expect = expected.begin()[i];
                    if (record.pos != expect.pos ||
                        record.type != expect.type ||
                        record.begin != expect.begin ||
                        record.c != expect.c) {
                        throw_(expect, record, to_string(record.type));
                    }
                }
                return true;
            };
            test(R"({"key": [1, 2, 3], "key2": {"key3": "value\n"} })",
                 {
                     {0, ElementType::object, true, '{'},
                     {1, ElementType::object_field, true, '\"'},
                     {1, ElementType::key_string, true, '\"'},
                     {5, ElementType::key_string, false, '\"'},
                     {8, ElementType::array, true, '['},
                     {9, ElementType::number, true, '1'},
                     {10, ElementType::integer, false, ','},        // End 1
                     {10, ElementType::array_element, false, ','},  // End element 1
                     {12, ElementType::number, true, '2'},
                     {13, ElementType::integer, false, ','},        // End 2
                     {13, ElementType::array_element, false, ','},  // End element 2
                     {15, ElementType::number, true, '3'},
                     {16, ElementType::integer, false, ']'},        // End 3
                     {16, ElementType::array_element, false, ']'},  // End element 3
                     {16, ElementType::array, false, ']'},          // End array
                     {17, ElementType::object_field, false, ','},   // End field 1
                     {19, ElementType::object_field, true, '\"'},   // Start field 2
                     {19, ElementType::key_string, true, '\"'},
                     {24, ElementType::key_string, false, '\"'},
                     {27, ElementType::object, true, '{'},
                     {28, ElementType::object_field, true, '\"'},
                     {28, ElementType::key_string, true, '\"'},
                     {33, ElementType::key_string, false, '\"'},
                     {36, ElementType::string, true, '\"'},
                     {44, ElementType::escaped_string, false, '\"'},  // End string value
                     {45, ElementType::object_field, false, '}'},     // End field 3
                     {45, ElementType::object, false, '}'},           // End inner object
                     {47, ElementType::object_field, false, '}'},     // End field 2
                     {47, ElementType::object, false, '}'},           // End outer object
                 });
            test(R"([{"nested_key": [true, false, null]}, {"another_key": {"inner_key": 123.45}}])",
                 {
                     {0, ElementType::array, true, '['},
                     {1, ElementType::object, true, '{'},         // Start element 1 (object)
                     {2, ElementType::object_field, true, '\"'},  // Start field 1 (nested_key)
                     {2, ElementType::key_string, true, '\"'},
                     {13, ElementType::key_string, false, '\"'},
                     {16, ElementType::array, true, '['},
                     {17, ElementType::boolean, true, 't'},
                     {20, ElementType::boolean, false, 'e'},        // End true
                     {21, ElementType::array_element, false, ','},  // End element true
                     {23, ElementType::boolean, true, 'f'},
                     {27, ElementType::boolean, false, 'e'},        // End false
                     {28, ElementType::array_element, false, ','},  // End element false
                     {30, ElementType::null, true, 'n'},
                     {33, ElementType::null, false, 'l'},           // End null
                     {34, ElementType::array_element, false, ']'},  // End element null
                     {34, ElementType::array, false, ']'},          // End inner array
                     {35, ElementType::object_field, false, '}'},   // End field 1 (nested_key)
                     {35, ElementType::object, false, '}'},         // End first object
                     {36, ElementType::array_element, false, ','},  // End element object 1
                     {38, ElementType::object, true, '{'},          // Start element 2 (object)
                     {39, ElementType::object_field, true, '\"'},   // Start field 2 (another_key)
                     {39, ElementType::key_string, true, '\"'},
                     {51, ElementType::key_string, false, '\"'},
                     {54, ElementType::object, true, '{'},         // Start inner object
                     {55, ElementType::object_field, true, '\"'},  // Start field 3 (inner_key)
                     {55, ElementType::key_string, true, '\"'},
                     {65, ElementType::key_string, false, '\"'},
                     {68, ElementType::number, true, '1'},
                     {74, ElementType::floating, false, '}'},       // End number 123.45
                     {74, ElementType::object_field, false, '}'},   // End field 3 (inner_key)
                     {74, ElementType::object, false, '}'},         // End inner object
                     {75, ElementType::object_field, false, '}'},   // End field 2 (another_key) - Assuming triggered by ']'
                     {75, ElementType::object, false, '}'},         // End second object - Assuming triggered by ']'
                     {76, ElementType::array_element, false, ']'},  // End element object 2
                     {76, ElementType::array, false, ']'},          // End outer array
                 });
            return true;
        }

        static_assert(test_parser_nest(), "Parser test failed");
    }  // namespace test

    template <class C, class R>
    concept Constructor = requires(C c, R r) {
        { c.init_object(r) } -> std::convertible_to<bool>;
        { c.init_array(r) } -> std::convertible_to<bool>;
        { c.init_string(r) } -> std::convertible_to<bool>;
        { c.init_escaped_string(r) } -> std::convertible_to<bool>;
        { c.init_key_string(r) } -> std::convertible_to<bool>;
        { c.init_escaped_key_string(r) } -> std::convertible_to<bool>;
        { c.init_integer(r) } -> std::convertible_to<bool>;
        { c.init_floating(r) } -> std::convertible_to<bool>;
        { c.init_boolean(r) } -> std::convertible_to<bool>;
        { c.init_null(r) } -> std::convertible_to<bool>;
        { c.add_array_element(r) } -> std::convertible_to<bool>;
        { c.add_object_field(r) } -> std::convertible_to<bool>;
    };

    template <class B>
    struct BytesLikeReader {
        B bytes;
        size_t size = 0;
        size_t pos = 0;
        size_t saved_begin = 0;

        constexpr auto pop_next() {
            return eof() ? 0 : bytes[pos++];
        }

        constexpr size_t readable_size() {
            return size - pos;
        }

        template <size_t n>
        constexpr decltype(auto) consume(const char* s) {
            if (readable_size() < n) {
                return false;
            }
            for (size_t i = 0; i < n; ++i) {
                if (bytes[pos + i] != s[i]) {
                    return false;
                }
            }
            pos += n;
            return true;
        }

        constexpr bool eof() {
            return pos >= size;
        }

        constexpr void save_begin() {
            saved_begin = pos - 1;
        }

        // range [begin,end)
        constexpr std::pair<size_t, size_t> range(ElementType typ) {
            auto end = pos;
            if (typ == ElementType::integer || typ == ElementType::floating) {
                end--;
            }
            return {saved_begin, end};
        }
    };

    template <class T>
    concept has_max_size = requires(T t) {
        { t.max_size() } -> std::convertible_to<size_t>;
    };

    template <Reader R, class StateStack, class JSONConstructor>
        requires Constructor<JSONConstructor, R>
    struct GenericConstructor {
        R r;
        JSONConstructor json_constructor;
        StateStack state_stack;
        size_t max_stack_size = 0;

        constexpr void update_max_stack_size() {
            if (state_stack.size() > max_stack_size) {
                max_stack_size = state_stack.size();
            }
        }

        constexpr bool check_stack_size() {
            if constexpr (has_max_size<StateStack>) {
                if (state_stack.size() >= state_stack.max_size()) {
                    return false;
                }
            }
            return true;
        }

        constexpr decltype(auto) pop_next() {
            return r.pop_next();
        }

        constexpr decltype(auto) readable_size() {
            return r.readable_size();
        }

        template <size_t n>
        constexpr decltype(auto) consume(const char* s) {
            return r.template consume<n>(s);
        }

        constexpr decltype(auto) eof() {
            return r.eof();
        }

        constexpr bool push_state(ParseStateDetail d) {
            if (!check_stack_size()) {
                return false;
            }
            state_stack.push_back(d);
            update_max_stack_size();
            return true;
        }

        constexpr ParseStateDetail pop_state() {
            if (state_stack.empty()) {
                return ParseStateDetail::start;
            }
            auto s = state_stack.back();
            state_stack.pop_back();
            return s;
        }

        constexpr bool save_begin(ElementType typ) {
            if (typ == ElementType::object_field || typ == ElementType::array_element) {
                return true;
            }
            if (typ == ElementType::object) {
                return json_constructor.init_object(r);
            }
            else if (typ == ElementType::array) {
                return json_constructor.init_array(r);
            }
            else {
                r.save_begin();
                return true;
            }
        }

        constexpr bool save_end(ElementType typ) {
            switch (typ) {
                case ElementType::object:
                case ElementType::array:
                    return true;
                case ElementType::boolean:
                    return json_constructor.init_boolean(r);
                case ElementType::null:
                    return json_constructor.init_null(r);
                case ElementType::string:
                    return json_constructor.init_string(r);
                case ElementType::integer:
                    return json_constructor.init_integer(r);
                case ElementType::floating:
                    return json_constructor.init_floating(r);
                case ElementType::escaped_string:
                    return json_constructor.init_escaped_string(r);
                case ElementType::array_element:
                    return json_constructor.add_array_element(r);
                case ElementType::key_string:
                    return json_constructor.init_key_string(r);
                case ElementType::escaped_key_string:
                    return json_constructor.init_escaped_key_string(r);
                case ElementType::object_field:
                    return json_constructor.add_object_field(r);
                default:
                    return false;
            }
        }
    };

}  // namespace futils::json
