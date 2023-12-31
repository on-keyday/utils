/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
// std
#include <concepts>
#include <utility>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
// other
#include <strutil/append.h>
#include <escape/escape.h>
#include <number/to_string.h>
#include <strutil/equal.h>
#include <helper/disable_self.h>
#include <helper/template_instance.h>

namespace utils {
    namespace json {
        template <class T, class V>
        concept has_json_member = requires(T t) {
            { t.as_json(std::declval<V>()) };
        };

        template <class T, class V>
        concept has_json_adl = requires(T t) {
            { as_json(t, std::declval<V>()) };
        };

        template <class T>
        concept has_deref = requires(T t) {
            { !t } -> std::convertible_to<bool>;
            { *t };
        };

        template <class T>
        concept is_object_like = std::ranges::range<T> && requires(T t) {
            { get<0>(*t.begin()) } -> std::convertible_to<std::string_view>;
            { get<1>(*t.begin()) };
        };

        constexpr auto json_int_min = -9007199254740991;
        constexpr auto json_int_max = 9007199254740991;

        enum class IntTraits : byte {
            int_as_int,
            int_as_str,
            int_as_float,
            overflow_int_as_str,
            overflow_int_as_float,
            u64_as_str,
            u64_as_float,
        };

        template <class Object>
        struct FieldBase {
           protected:
            Object& buffer;
            bool comma = false;
            size_t index = 0;

            constexpr FieldBase(Object& buf)
                : buffer(buf) {}
            constexpr void with_comma(auto&& fn) {
                if (comma) {
                    buffer.comma();
                }
                else {
                    buffer.first_element();
                }
                fn();
                comma = true;
            }
        };

        template <class Object>
        struct ObjectField : FieldBase<Object> {
            constexpr ObjectField(Object& buf)
                : FieldBase<Object>(buf) {}

            constexpr void operator()(auto&& name, auto&& value) {
                this->with_comma([&] {
                    this->buffer.string(name);
                    this->buffer.colon();
                    this->buffer.value(value);
                    this->buffer.set_object_field();
                });
            }

            constexpr ~ObjectField() {
                this->buffer.end_block("}", this->comma);
            }
        };

        template <class Object>
        struct ArrayField : FieldBase<Object> {
            constexpr ArrayField(Object& buf)
                : FieldBase<Object>(buf) {}

            constexpr void operator()(auto&& value) {
                this->with_comma([&] {
                    this->buffer.value(value);
                });
            }

            constexpr ~ArrayField() {
                this->buffer.end_block("]", this->comma);
            }
        };

        template <class String>
        struct RawJSON {
            String raw;
        };

        template <class T, class View>
        concept is_raw_json = helper::is_template_instance_of<T, RawJSON> &&
                              requires(T t) {
                                  { t.raw } -> std::convertible_to<View>;
                              };

        enum class Token : byte {
            none,
            colon,
            open_object,
            object_field,
            object,
        };

        template <class Output = std::string, class StringView = std::string_view, class Indent = const char*>
        struct Stringer {
           private:
            template <class Debug>
            friend struct ObjectField;

            template <class Debug>
            friend struct ArrayField;

            template <class Debug>
            friend struct FieldBase;
            Output buf;
            Indent indent{};
            escape::EscapeFlag escape_flags = escape::EscapeFlag::none;  // for safety

            enum : byte {
                f_html_escape = 0x1,
                f_use_indent = 0x2,
                f_no_colon_space = 0x4,  // for legacy code
            };

            byte flags = 0;

            IntTraits int_traits = IntTraits::int_as_int;

            constexpr void set_flag(byte f, bool s) {
                if (s) {
                    flags |= f;
                }
                else {
                    flags &= ~f;
                }
            }

            constexpr bool use_indent() const {
                return flags & f_use_indent;
            }

            constexpr bool html() const {
                return flags & f_html_escape;
            }

            // internal
            size_t current_indent = 0;
            bool prev_colon = false;

            constexpr void write(auto&&... a) {
                strutil::appends(buf, a...);
            }

            constexpr void comma() {
                write(",");
                if (use_indent()) {
                    write("\n");
                }
            }

            constexpr void write_indent() {
                if (prev_colon) {
                    prev_colon = false;
                    return;  // "key" : <here>
                }
                if (use_indent()) {
                    for (size_t i = 0; i < current_indent; i++) {
                        if constexpr (std::is_integral_v<Indent>) {
                            for (Indent i = 0; i < indent; i++) {
                                buf.push_back(' ');
                            }
                        }
                        else {
                            write(indent);
                        }
                    }
                }
            }

            constexpr void colon() {
                if (flags & f_no_colon_space) {
                    write(":");
                }
                else {
                    write(": ");
                }
                prev_colon = true;
            }

            constexpr void first_element() {
                if (use_indent()) {
                    write("\n");
                    current_indent++;
                }
            }

            constexpr void set_object_field() {
                if (prev_colon) {
                    null();
                }
            }

            constexpr void end_block(const char* v, bool comma) {
                if (use_indent()) {
                    if (comma) {
                        write("\n");
                    }
                    current_indent--;
                    if (comma) {
                        write_indent();
                    }
                }
                write(v);
            }

            constexpr void apply_value(auto&& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, Stringer>) {
                    write(value.buf);
                    prev_colon = false;
                }
                else if constexpr (is_raw_json<T, StringView>) {
                    write(value.raw);
                    prev_colon = false;
                }
                else if constexpr (std::invocable<T, Stringer&>) {
                    std::invoke(value, *this);
                }
                else if constexpr (std::invocable<T>) {
                    std::invoke(value);
                }
                else if constexpr (has_json_member<T, Stringer&>) {
                    value.as_json(*this);
                }
                else if constexpr (has_json_adl<T, Stringer&>) {
                    as_json(value, *this);
                }
                else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    null();
                }
                else if constexpr (std::is_same_v<T, std::uint64_t>) {
                    number(value);
                }
                else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                    number(std::int64_t(value));
                }
                else if constexpr (std::is_floating_point_v<T>) {
                    number(double(value));
                }
                else if constexpr (std::is_convertible_v<T, StringView>) {
                    string(value);
                }
                else if constexpr (has_deref<T>) {
                    !value ? null() : apply_value(*value);
                }
                else if constexpr (std::is_convertible_v<T, void*>) {
                    !value ? null() : number(std::uintptr_t(value));
                }
                else if constexpr (std::is_convertible_v<T, bool>) {
                    boolean(bool(value));
                }
                else if constexpr (is_object_like<T>) {
                    object(value);
                }
                else if constexpr (std::ranges::range<T>) {
                    array(value);
                }
                else {
                    static_assert(has_json_member<T, Stringer&>);
                }
            }

           public:
            constexpr Stringer() = default;

            template <class V, helper_disable_self(Stringer, V)>
            constexpr Stringer(V&& v)
                : buf(std::forward<V>(v)) {}

            [[nodiscard]] constexpr ObjectField<Stringer> object() {
                write_indent();
                write("{");
                return ObjectField<Stringer>{*this};
            }

            template <is_object_like T>
            constexpr void object(T&& value) {
                auto field = object();
                for (auto& v : value) {
                    field(get<0>(v), get<1>(v));
                }
            }

            [[nodiscard]] constexpr ArrayField<Stringer> array() {
                write_indent();
                write("[");
                return ArrayField<Stringer>{*this};
            }

            template <std::ranges::range T>
            constexpr void array(T&& value) {
                auto field = array();
                for (auto& v : value) {
                    field(v);
                }
            }

           private:
            constexpr bool handle_int(auto value) {
                const bool overflow = value < json_int_min || value > json_int_max;
                if (int_traits == IntTraits::int_as_float ||
                    (std::is_same_v<decltype(value), std::uint64_t> && int_traits == IntTraits::u64_as_float)) {
                    number(double(value));
                    return true;
                }
                if (int_traits == IntTraits::overflow_int_as_float) {
                    if (overflow) {
                        number(double(value));
                        return true;
                    }
                }
                write_indent();  // ^float v str
                if (int_traits == IntTraits::int_as_str ||
                    (std::is_same_v<decltype(value), std::uint64_t> && int_traits == IntTraits::u64_as_str)) {
                    write("\"");
                    utils::number::to_string(buf, value);  // to optimize
                    write("\"");
                    return true;
                }
                if (int_traits == IntTraits::overflow_int_as_str) {
                    if (overflow) {
                        write("\"");
                        utils::number::to_string(buf, value);  // to optimize
                        write("\"");
                        return true;
                    }
                }
                return false;
            }

           public:
            constexpr void number(std::int64_t value) {
                if (handle_int(value)) {
                    return;
                }
                utils::number::to_string(buf, value);  // to optimize
            }

            constexpr void number(std::uint64_t value) {
                if (handle_int(value)) {
                    return;
                }
                utils::number::to_string(buf, value);  // to optimize
            }

            constexpr void number(double value) {
                write_indent();
                utils::number::to_string(buf, value);  // to optimize
            }

            constexpr void set_indent(const Indent& s) {
                indent = s;
                set_flag(f_use_indent, true);
            }

            constexpr void unset_indent() {
                set_flag(f_use_indent, false);
            }

            constexpr void set_html_escape(bool h) {
                set_flag(f_html_escape, h);
            }

            // for legacy code
            constexpr void set_no_colon_space(bool h) {
                set_flag(f_no_colon_space, h);
            }

            constexpr void set_int_traits(IntTraits tr) {
                int_traits = tr;
            }

            constexpr void set_utf_escape(bool utf, bool upper = false, bool replace = true) {
                escape_flags = escape::EscapeFlag::none;
                if (utf) {
                    escape_flags |= escape::EscapeFlag::utf16;
                }
                if (upper) {
                    escape_flags |= escape::EscapeFlag::upper;
                }
                if (!replace) {
                    escape_flags |= escape::EscapeFlag::no_replacement;
                }
            }

            constexpr void string(StringView value) {
                write_indent();
                if (html()) {
                    write("\"");
                    escape::escape_str(value, buf, escape_flags, escape::json_set(any(escape_flags & escape::EscapeFlag::upper)));
                    write("\"");
                }
                else {
                    write("\"");
                    escape::escape_str(value, buf, escape_flags, utils::escape::json_set_no_html());  // to optimize
                    write("\"");
                }
            }

            constexpr void null() {
                write_indent();
                write("null");
            }

            constexpr void boolean(bool v) {
                write_indent();
                write(v ? "true" : "false");
            }

            constexpr void value(auto&& v) {
                apply_value(v);
            }

            constexpr Output& out() {
                return buf;
            }
        };

        namespace test {
            constexpr bool check_stringer() {
                using vec = number::Array<char, 200, true>;
                vec buf{};
                Stringer<vec&, const char*, const char*> s{buf};

                // test
                s.value(nullptr);
                if (!strutil::equal(s.out(), "null")) {
                    return false;
                }
                s.out().i = 0;
                {
                    auto field = s.object();
                    field("key", "value");
                    field("object", 20);
                }
                if (!strutil::equal(s.out(), R"({"key": "value","object": 20})")) {
                    return false;
                }
                s.out().i = 0;
                s.set_indent("");
                {
                    auto field = s.object();
                    field("key", "value");
                    field("object", 20);
                    field("child", [&] {
                        auto field = s.array();
                        field(true);
                        field("hey");
                    });
                }
                if (!strutil::equal(s.out(), R"({
"key": "value",
"object": 20,
"child": [
true,
"hey"
]
})")) {
                    return false;
                }
                s.out().i = 0;
                s.set_indent("    ");
                {
                    auto field = s.object();
                    field("key", "value");
                    field("object", 20);
                    field("child", [&] {
                        auto field = s.array();
                        field(true);
                        field("hey");
                    });
                }
                if (!strutil::equal(s.out(), R"({
    "key": "value",
    "object": 20,
    "child": [
        true,
        "hey"
    ]
})")) {
                    return false;
                }

                // escape test
                s.out().i = 0;
                s.set_utf_escape(true, true);
                s.set_html_escape(true);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object\/yes\r\n\t\f\b\uD83C\uDF85\u003C\u003E")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(true, true);
                s.set_html_escape(false);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object/yes\r\n\t\f\b\uD83C\uDF85<>")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(true, false);
                s.set_html_escape(true);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object\/yes\r\n\t\f\b\ud83c\udf85\u003c\u003e")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(true, false);
                s.set_html_escape(false);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object/yes\r\n\t\f\b\ud83c\udf85<>")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(false, true);
                s.set_html_escape(true);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object\/yes\r\n\t\f\b🎅\u003C\u003E")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(false, true);
                s.set_html_escape(false);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object/yes\r\n\t\f\b🎅<>")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(false, false);
                s.set_html_escape(true);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object\/yes\r\n\t\f\b🎅\u003c\u003e")")) {
                    return false;
                }

                s.out().i = 0;
                s.set_utf_escape(false, false);
                s.set_html_escape(false);
                s.value("object/yes\r\n\t\f\b🎅<>");
                if (!strutil::equal(s.out(), R"("object/yes\r\n\t\f\b🎅<>")")) {
                    return false;
                }

                s.out().i = 0;
                s.value("\xff\xfe");
                if (!strutil::equal(s.out(), R"("��")")) {
                    return false;
                }
                s.out().i = 0;
                s.set_utf_escape(true, false, false);
                s.value(" ");
                if (!strutil::equal(s.out(), R"(" ")")) {
                    [](auto... arg) { throw "error"; }(s.out().i);
                    return false;
                }
                return true;
            }

            static_assert(check_stringer());
        }  // namespace test
    }      // namespace json
}  // namespace utils
