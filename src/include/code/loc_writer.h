/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cstddef>
#include <utility>
#include "helper/defer.h"
#include "helper/defer_ex.h"
#include "strutil/append.h"
#include <string_view>
#include "strutil/per_line.h"

namespace futils::code {
    struct LinePos {
        size_t line;
        size_t pos;
    };

    template <class Loc>
    struct LocEntry {
        Loc loc;
        LinePos start;
        LinePos end;
    };

    template <class String>
    struct Line {
        String content;
        size_t indent_level = 0;
        bool eol = false;
    };

    template <class String, template <class...> class Vec, class Loc>
    struct LocWriter {
       private:
        Vec<LocEntry<Loc>> locs;
        size_t line_count_ = 1;
        size_t indent_level = 0;
        Vec<Line<String>> lines;

        void maybe_init_line() {
            if (lines.empty()) {
                lines.emplace_back(Line<String>{.indent_level = indent_level});
            }
            if (lines.back().eol) {
                lines.emplace_back(Line<String>{.indent_level = indent_level});
            }
        }

       public:
        constexpr LocWriter() = default;

        void reset() {
            locs.clear();
            lines.clear();
            line_count_ = 1;
            indent_level = 0;
        }

        // should not contain '\n'
        // writer does not guarantee args has no '\n'
        void write(auto&&... args) {
            auto handle_for_each = [&](auto&& arg) {
                if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, LocWriter>) {
                    merge(std::forward<decltype(arg)>(arg));
                }
                else {
                    maybe_init_line();
                    strutil::append(lines.back().content, arg);
                }
            };
            (handle_for_each(std::forward<decltype(args)>(args)), ...);
        }

        // should not contain '\n'
        // writer does not guarantee args has no '\n'
        void write_with_loc(const Loc& loc, auto&&... args) {
            auto loc_scope = with_loc_scope(loc);
            write(std::forward<decltype(args)>(args)...);
        }

        auto with_loc_scope(const Loc& loc) {
            maybe_init_line();
            size_t start_line = line_count_;
            size_t start_pos = 0;
            start_pos = lines.back().content.size();
            return helper::defer([this, loc, start_line, start_pos] {
                size_t end_line = line_count_;
                size_t end_pos = 0;
                end_pos = lines.back().content.size();
                locs.push_back(LocEntry{loc, LinePos{start_line, start_pos}, LinePos{end_line, end_pos}});  // 0-based
            });
        }

        void line() {
            maybe_init_line();
            lines.back().eol = true;
            line_count_++;
        }

        void writeln(auto&&... args) {
            write(std::forward<decltype(args)>(args)...);
            line();
        }

        void writeln_with_loc(const Loc& loc, auto&&... args) {
            write_with_loc(loc, std::forward<decltype(args)>(args)...);
            line();
        }

       private:
        void maybe_update_indent_level() {
            if (!lines.empty() && !lines.back().eol && lines.back().content.empty()) {
                lines.back().indent_level = indent_level;
            }
        }

       public:
        auto indent_scope(std::uint32_t i = 1) {
            indent_level += i;
            return helper::defer([this, i] {
                if (indent_level < i) {
                    indent_level = 0;
                }
                else {
                    indent_level -= i;
                }
                maybe_update_indent_level();
            });
        }

        auto indent_scope_ex(std::uint32_t i = 1) {
            indent_level += i;
            return helper::defer_ex([this, i] {
                if (indent_level < i) {
                    indent_level = 0;
                }
                else {
                    indent_level -= i;
                }
                maybe_update_indent_level();
            });
        }

        constexpr void indent_writeln(auto&& a, auto&&... s) {
            if constexpr (std::is_integral_v<std::decay_t<decltype(a)>>) {
                auto ind = indent_scope(a);
                writeln(s...);
            }
            else {
                auto ind = indent_scope();
                writeln(a, s...);
            }
        }

        constexpr void indent_writeln_with_loc(const Loc& loc, auto&& a, auto&&... s) {
            if constexpr (std::is_integral_v<std::decay_t<decltype(a)>>) {
                auto ind = indent_scope(a);
                writeln_with_loc(loc, s...);
            }
            else {
                auto ind = indent_scope();
                writeln_with_loc(loc, a, s...);
            }
        }

        void merge(LocWriter&& other) {
            maybe_init_line();
            size_t column_offset = 0;
            if (!other.lines.empty()) {
                for (auto& line : other.lines) {
                    line.indent_level += indent_level;
                    if (lines.back().eol) {
                        lines.push_back(std::move(line));
                    }
                    else {
                        if (lines.back().content.empty()) {
                            lines.back().indent_level = line.indent_level;
                        }
                        column_offset = lines.back().content.size();
                        lines.back().content += std::move(line.content);
                        lines.back().eol = line.eol;
                    }
                }
            }
            size_t line_offset = line_count_ - 1;
            for (auto& loc : other.locs) {
                if (loc.start.line == 1) {
                    loc.start.pos += column_offset;
                }
                if (loc.end.line == 1) {
                    loc.end.pos += column_offset;
                }
                loc.start.line += line_offset;
                loc.end.line += line_offset;
                locs.push_back(std::move(loc));
            }
            line_count_ += other.line_count_ - 1;
            other.reset();
        }

        void merge(const LocWriter& other) {
            maybe_init_line();
            size_t column_offset = 0;
            if (!other.lines.empty()) {
                for (const auto& line : other.lines) {
                    Line<String> new_line = line;
                    new_line.indent_level += indent_level;
                    if (lines.back().eol) {
                        lines.push_back(std::move(new_line));
                    }
                    else {
                        if (lines.back().content.empty()) {
                            lines.back().indent_level = new_line.indent_level;
                        }
                        column_offset = lines.back().content.size();
                        lines.back().content += new_line.content;
                        lines.back().eol = new_line.eol;
                    }
                }
            }
            size_t line_offset = line_count_ - 1;
            for (const auto& loc : other.locs) {
                LocEntry<Loc> new_loc = loc;
                if (new_loc.start.line == 1) {
                    new_loc.start.pos += column_offset;
                }
                if (new_loc.end.line == 1) {
                    new_loc.end.pos += column_offset;
                }
                new_loc.start.line += line_offset;
                new_loc.end.line += line_offset;
                locs.push_back(std::move(new_loc));
            }
            line_count_ += other.line_count_ - 1;
        }

        template <class View = std::string_view>
        constexpr void write_unformatted(auto&& v) {
            auto cur = View(v);
            int count = -1;
            strutil::per_line(
                cur,
                [&](View view) {
                    int ind = strutil::count_indent(view);
                    if (ind >= 0 && (count == -1 || ind < count)) {
                        count = ind;
                    }
                },
                [] {});
            strutil::per_line(
                cur,
                [&](View view) {
                    if (view.size() < count) {
                        write("");
                        return;
                    }
                    write(view.substr(count));
                },
                [this] {
                    line();
                });
        }

        template <class View = std::string_view>
        constexpr void write_unformatted_with_loc(const Loc& loc, auto&& v) {
            auto loc_scope = with_loc_scope(loc);
            write_unformatted<View>(std::forward<decltype(v)>(v));
        }

        constexpr size_t line_count() const {
            return line_count_;
        }

        constexpr const Vec<Line<String>>& lines_data() const {
            return lines;
        }

        constexpr String to_string(const char* indent = "    ") const {
            String r;
            for (const auto& line : lines) {
                for (size_t i = 0; i < line.indent_level; i++) {
                    strutil::append(r, indent);
                }
                strutil::append(r, line.content);
                if (line.eol) {
                    strutil::append(r, "\n");
                }
            }
            return r;
        }

        constexpr bool empty() const {
            return lines.empty() || (lines.size() == 1 && lines[0].content.empty());
        }

        const size_t str_size() const {
            size_t size = 0;
            for (const auto& line : lines) {
                size += line.content.size();
                if (line.eol) {
                    size += 1;
                }
            }
            return size;
        }

        constexpr const Vec<LocEntry<Loc>>& locs_data() const {
            return locs;
        }

        LocEntry<Loc> adjust_with_indent(size_t indent_space_count, const LocEntry<Loc>& loc) const {
            LocEntry<Loc> r = loc;
            if (lines.size() < loc.start.line) {
                return r;
            }
            r.start.pos += indent_space_count * lines[loc.start.line - 1].indent_level;
            if (lines.size() < loc.end.line) {
                return r;
            }
            r.end.pos += indent_space_count * lines[loc.end.line - 1].indent_level;
            return r;
        }

        size_t offset_of(const LinePos& pos, size_t indent_space_count) const {
            if (lines.size() < pos.line) {
                return 0;
            }
            size_t offset = 0;
            for (size_t i = 0; i < pos.line - 1; i++) {
                offset += indent_space_count * lines[i].indent_level;
                offset += lines[i].content.size();
                if (lines[i].eol) {
                    offset += 1;
                }
            }
            offset += indent_space_count * lines[pos.line - 1].indent_level;
            if (lines[pos.line - 1].content.size() + 1 < pos.pos) {
                offset += lines[pos.line - 1].content.size();
            }
            else {
                offset += pos.pos;
            }
            return offset;
        }
    };
}  // namespace futils::code
