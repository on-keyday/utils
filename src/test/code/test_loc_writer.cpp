/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <code/loc_writer.h>
#include <string>
#include <vector>
#include "code/src_location.h"
#include "core/sequencer.h"
#include "wrap/cout.h"

int main() {
    futils::code::LocWriter<std::string, std::vector, futils::code::SrcLoc> w, t;
    w.writeln_with_loc({.line = 0}, "int main() {");
    {
        auto ind = t.indent_scope();
        t.writeln_with_loc({.line = 1}, "return 0;");
    }
    w.merge(std::move(t));
    w.writeln_with_loc({.line = 2}, "}");
    w.writeln();
    w.write_unformatted_with_loc({.line = 4}, R"(teleport to {
    x: 100,
    y: 200,
})");

    futils::wrap::cout_wrap() << "Generated code:\n";
    futils::wrap::cout_wrap() << w.lines_data().size() << " lines\n";
    std::string source_code;
    for (const auto& line : w.lines_data()) {
        for (size_t i = 0; i < line.indent_level; i++) {
            futils::wrap::cout_wrap() << "    ";
            source_code += "    ";
        }
        futils::wrap::cout_wrap() << line.content;
        source_code += line.content;
        if (line.eol) {
            futils::wrap::cout_wrap() << "\n";
            source_code += "\n";
        }
    }
    futils::wrap::cout_wrap() << "\nLocations:\n";
    for (const auto& raw_loc : w.locs_data()) {
        auto loc = w.adjust_with_indent(4, raw_loc);
        futils::wrap::cout_wrap() << loc.loc.line << " is mapped to from (" << loc.start.line << "," << loc.start.pos << ") to (" << loc.end.line << "," << loc.end.pos << ")\n";
        std::string s;
        auto seq = futils::make_ref_seq(source_code);
        seq.rptr = w.offset_of(raw_loc.start, 4);
        auto end_ptr = w.offset_of(raw_loc.end, 4);
        futils::code::write_src_loc(s, seq, end_ptr - seq.rptr, 10);
        futils::wrap::cout_wrap() << s << "\n";
    }
}