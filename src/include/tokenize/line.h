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
        constexpr LineKind match_line(Sequencer<T>& seq, const char*& line) {
            if (seq.seek_if("\r\n")) {
                line = "\r\n";
                return LineKind::crlf;
            }
            else if (seq.seek_if("\r")) {
                line = "\r";
                return LineKind::cr;
            }
            else if (seq.seek_if("\n")) {
                line = "\n";
                return LineKind::lf;
            }
            return LineKind::unknown;
        }

        template <class T>
        constexpr bool read_line(Sequencer<T>& seq, LineKind& kind, size_t& count) {
            const char* line;
            kind = match_line(seq, line);
            if (kind == LineKind::unknown) {
                return false;
            }
            count = 1;
            while (seq.seek_if(line)) {
                count++;
            }
            return true;
        }
    }  // namespace tokenize
}  // namespace utils
