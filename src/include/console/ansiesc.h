/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <number/to_string.h>
#include <core/byte.h>

namespace utils {
    namespace console::escape {
        enum class CursorMove : byte {
            up,
            down,
            right,
            left,
            top_of_up,
            top_of_down,
            from_left,
        };

        constexpr auto escape_sequence_mark = '\x1b';

        struct max_sequence_buffer {
            char buf[2 + 19 + 1 + 19 + 1 + 1]{};
            byte i = 0;

            constexpr void push_back(byte c) {
                buf[i] = c;
                i++;
            }

            constexpr size_t size() const {
                return i;
            }

            constexpr auto operator[](size_t i) const {
                return buf[i];
            }

            constexpr const char* c_str() const {
                return buf;
            }
        };

        template <class Out>
        constexpr void cursor(Out&& buf, CursorMove m, size_t n) {
            if (int(m) > int(CursorMove::from_left)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, n);
            buf.push_back('A' + int(m));
        }

        template <class Out = max_sequence_buffer>
        constexpr Out cursor(CursorMove m, size_t n) {
            Out o;
            cursor(o, m, n);
            return o;
        }

        template <class Out>
        constexpr void cursor_abs(Out&& buf, size_t n, size_t m) {
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, n);
            buf.push_back(';');
            number::to_string(buf, m);
            buf.push_back('H');
        }

        template <class Out = max_sequence_buffer>
        constexpr Out cursor_abs(size_t n, size_t m) {
            Out o;
            cursor_abs(o, n, m);
            return o;
        }

        enum class ConsoleScroll : byte {
            forward,
            backward,
        };

        template <class Out>
        constexpr void console_scroll(Out&& buf, ConsoleScroll s, size_t n) {
            if (int(s) > int(ConsoleScroll::backward)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, n);
            buf.push_back('S' + int(s));
        }

        template <class Out = max_sequence_buffer>
        constexpr Out console_scroll(ConsoleScroll s, size_t n) {
            Out o;
            console_scroll(o, s, n);
            return o;
        }

        enum class EraseMode : byte {
            back_of_cursor,
            front_of_cursor,
            all,
        };

        enum class EraseTarget : byte {
            screen,
            line,
        };

        template <class Out>
        constexpr void erase(Out&& buf, EraseTarget t, EraseMode s) {
            if (int(s) > int(EraseMode::all) || int(t) > int(EraseTarget::line)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, int(s));
            buf.push_back('J' + int(t));
        }

        template <class Out = max_sequence_buffer>
        constexpr Out console_scroll(EraseTarget t, EraseMode s) {
            Out o;
            erase(o, t, s);
            return o;
        }

        enum class LetterStyle : byte {
            none,
            bold,
            light,
            italic,
            blink,
            rapid_blink,
            inversion,
            hide,
            cancel,
        };

        template <class Out>
        constexpr void letter_style(Out&& buf, LetterStyle s) {
            if (int(s) > int(LetterStyle::cancel)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, int(s));
            buf.push_back('m');
        }

        template <class Out = max_sequence_buffer>
        constexpr Out letter_style(LetterStyle s) {
            Out o;
            letter_style(o, s);
            return o;
        }

        enum class ColorPalette : byte {
            black,
            red,
            green,
            yellow,
            blue,
            magenta,
            cyan,
            white,
        };

        enum class ColorTarget : byte {
            letter,
            background,
        };

        template <class Out>
        constexpr void color(Out&& buf, ColorPalette p, ColorTarget t) {
            if (int(p) > int(ColorPalette::white) || int(t) > int(ColorTarget::background)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, int(p) + (t == ColorTarget::letter ? 30 : 40));
            buf.push_back('m');
        }

        template <class Out = max_sequence_buffer>
        constexpr Out color(ColorPalette p, ColorTarget t) {
            Out o;
            color(o, p, t);
            return o;
        }

        template <class Out>
        constexpr void color_code(Out&& buf, byte code, ColorTarget t) {
            if (int(t) > int(ColorTarget::background)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, 8 + (t == ColorTarget::letter ? 30 : 40));
            buf.push_back(';');
            buf.push_back('5');
            buf.push_back(';');
            number::to_string(buf, code);
            buf.push_back('m');
        }

        template <class Out = max_sequence_buffer>
        constexpr Out color_code(byte code, ColorTarget t) {
            Out o;
            color_code(o, code, t);
            return o;
        }

        template <class Out>
        constexpr void color_rgb(Out&& buf, byte r, byte g, byte b, ColorTarget t) {
            if (int(t) > int(ColorTarget::background)) {
                return;
            }
            buf.push_back(escape_sequence_mark);
            buf.push_back('[');
            number::to_string(buf, 8 + (t == ColorTarget::letter ? 30 : 40));
            buf.push_back(';');
            buf.push_back('2');
            buf.push_back(';');
            number::to_string(buf, r);
            buf.push_back(';');
            number::to_string(buf, g);
            buf.push_back(';');
            number::to_string(buf, b);
            buf.push_back('m');
        }

        template <class Out = max_sequence_buffer>
        constexpr Out color_code(byte r, byte g, byte b, ColorTarget t) {
            Out o;
            color_rgb(o, r, g, b, t);
            return o;
        }

        constexpr auto color_reset = letter_style(LetterStyle::none);

        template <ColorPalette l>
        constexpr auto letter_color = color(l, ColorTarget::letter);

    }  // namespace console::escape
}  // namespace utils
