/*license*/

// line - define what is line
#pragma once

#include <cstdint>

#include "../core/sequencer.h"

namespace utils {
    namespace tokenize {
        enum class LineKind : std::uint8_t {
            unknown = 0,
            lf = 1,
            cr = 2,
            crlf = 3,
        };

        template <class T>
        constexpr LineKind match_line(Sequencer<T>& seq, const char** line) {
            if (seq.match("\r\n")) {
                if (line) {
                    line = "\r\n";
                }
                return LineKind::crlf;
            }
            else if (seq.match("\r")) {
                if (line) {
                    line = "\r";
                }
                return LineKind::cr;
            }
            else if (seq.match("\n")) {
                if (line) {
                    line = "\n";
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
    }  // namespace tokenize
}  // namespace utils
