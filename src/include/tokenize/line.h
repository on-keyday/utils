/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// line - define what is line
#pragma once

#include <cstdint>

#include "../core/sequencer.h"
#include "../wrap/light/enum.h"

namespace utils {
    namespace tokenize {
        enum class LineKind : std::uint8_t {
            unknown = 0,
            lf = 1,
            cr = 2,
            crlf = cr | lf,
        };

        DEFINE_ENUM_FLAGOP(LineKind)

        template <class T>
        constexpr LineKind match_line(Sequencer<T>& seq, const char** line) {
            if (seq.match("\r\n")) {
                if (line) {
                    *line = "\r\n";
                }
                return LineKind::crlf;
            }
            else if (seq.match("\r")) {
                if (line) {
                    *line = "\r";
                }
                return LineKind::cr;
            }
            else if (seq.match("\n")) {
                if (line) {
                    *line = "\n";
                }
                return LineKind::lf;
            }
            return LineKind::unknown;
        }

        template <class T>
        constexpr bool read_line(Sequencer<T>& seq, LineKind& kind, size_t& count) {
            const char* line;
            kind = match_line(seq, &line);
            if (kind == LineKind::unknown) {
                return false;
            }
            count = 0;
            while (seq.seek_if(line)) {
                count++;
            }
            return true;
        }

        template <class String>
        struct Line : Token<String> {
            LineKind line = LineKind::unknown;
            size_t count = 0;

            constexpr Line()
                : Token<String>(TokenKind::line) {}

            String to_string() const override {
                String cmp;
                for (size_t i = 0; i < count; i++) {
                    if (any(line & LineKind::cr)) {
                        cmp.push_back('\r');
                    }
                    if (any(line & LineKind::lf)) {
                        cmp.push_back('\n');
                    }
                }
                return cmp;
            }

            bool has(const String& str) const override {
                return to_string() == str;
            }
        };
    }  // namespace tokenize
}  // namespace utils
