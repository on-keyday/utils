/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "../minilang/comb/indent.h"
#include "../minilang/comb/token.h"
#include "../minilang/comb/unicode.h"

#include "../comb2/composite/indent.h"
#include "../comb2/composite/range.h"
#include "../comb2/composite/number.h"
#include "../comb2/basic/group.h"
#include "../comb2/composite/string.h"
#include "../helper/defer.h"

namespace futils::yaml::parser {
    namespace cps = comb2::composite;
    using namespace comb2::ops;
    namespace ctxs = comb2::ctxs;
    using Status = comb2::Status;

    constexpr auto c_printable = cps::tab | cps::line_feed | cps::carriage_return |
                                 range(byte(0x20), byte(0x7E)) |
                                 ulit(byte(0x85)) |
                                 urange(byte(0xA0), std::uint16_t(0xD7FF)) |
                                 urange(std::uint16_t(0xE000), std::uint16_t(0xFFFD)) |
                                 urange(0x010000, 0x10FFFF);
    constexpr auto nb_json = cps::tab |
                             urange(byte(0x20), 0x10FFFF);

    constexpr auto c_byte_order_mark = cps::byte_order_mark;

    constexpr auto c_sequence_entry = '-'_l;
    constexpr auto c_mapping_key = '?'_l;
    constexpr auto c_mapping_value = ':'_l;

    constexpr auto c_collect_entry = ','_l;
    constexpr auto c_sequence_start = '['_l;
    constexpr auto c_sequence_end = ']'_l;
    constexpr auto c_mapping_start = '{'_l;
    constexpr auto c_mapping_end = '}'_l;

    constexpr auto c_comment = '#'_l;

    constexpr auto c_anchor = '&'_l;
    constexpr auto c_alias = '*'_l;
    constexpr auto c_tag = '!'_l;

    constexpr auto c_literal = '|'_l;
    constexpr auto c_folded = '>'_l;

    constexpr auto c_single_quote = '\''_l;
    constexpr auto c_double_quote = '"'_l;

    constexpr auto c_directive = '%';

    constexpr auto c_reserved = '@'_l | '`'_l;

    constexpr auto c_indicator =
        c_sequence_entry |
        c_mapping_key |
        c_mapping_value |
        c_collect_entry |
        c_sequence_start |
        c_sequence_end |
        c_mapping_start |
        c_mapping_end |
        c_comment |
        c_anchor |
        c_alias |
        c_tag |
        c_literal |
        c_folded |
        c_single_quote |
        c_double_quote |
        c_directive |
        c_reserved;

    constexpr auto c_flow_indicator =
        c_collect_entry |
        c_sequence_start |
        c_sequence_end |
        c_mapping_start |
        c_mapping_end;

    constexpr auto b_line_feed = cps::line_feed;
    constexpr auto b_carriage_return = cps::carriage_return;
    constexpr auto b_char = cps::line_feed | cps::carriage_return;

    constexpr auto nb_char = not_(c_byte_order_mark | b_line_feed | b_carriage_return) & c_printable;

    constexpr auto b_break = (b_carriage_return & b_line_feed) | b_carriage_return | b_line_feed;

    constexpr auto b_as_line_feed = str("b-as-line-feed", b_break);

    constexpr auto b_non_content = str("b-non-content", b_break);

    constexpr auto s_space = cps::space;

    constexpr auto s_tab = cps::tab;

    constexpr auto s_white = s_space | s_tab;

    constexpr auto ns_char = not_(s_white) & nb_char;

    constexpr auto ns_dec_digit = cps::digit_number;

    constexpr auto ns_ascii_letter = cps::alphabet;
    constexpr auto ns_hex_digit = cps::hexadecimal_number;
    constexpr auto ns_word_char = ns_dec_digit | ns_ascii_letter | lit('-');

    constexpr auto ns_uri_char = (lit('%') & +cps::hex2_str) | ns_word_char | oneof("#;/?:@&=+$,_.!~*'()[]");

    constexpr auto ns_tag_char = not_(c_tag | c_flow_indicator) & ns_uri_char;

    constexpr auto c_escape = cps::back_slash;
    constexpr auto ns_esc_null = '0'_l;
    constexpr auto ns_esc_bell = 'a'_l;
    constexpr auto ns_esc_backspace = 'b'_l;
    constexpr auto ns_esc_horizontal_tab = 't'_l | cps::tab;
    constexpr auto ns_esc_line_feed = 'n'_l;
    constexpr auto ns_esc_vertical_tab = 'v'_l;
    constexpr auto ns_esc_form_feed = 'f'_l;
    constexpr auto ns_esc_carriage_return = 'r'_l;
    constexpr auto ns_esc_escape = 'e'_l;
    constexpr auto ns_esc_space = ' '_l;
    constexpr auto ns_esc_double_quote = '"'_l;
    constexpr auto ns_esc_slash = '/'_l;
    constexpr auto ns_esc_backslash = '\\'_l;
    constexpr auto ns_esc_next_line = 'N'_l;
    constexpr auto ns_esc_non_breaking_space = '_'_l;
    constexpr auto ns_esc_line_separator = 'L'_l;
    constexpr auto ns_esc_paragraph_separator = 'P'_l;
    constexpr auto ns_esc_8bit = cps::hex_str;
    constexpr auto ns_esc_16bit = cps::utf16_str;
    constexpr auto ns_esc_32bit = cps::utf32_str;

    constexpr auto c_ns_esc_char =
        c_escape &
        (ns_esc_null |
         ns_esc_bell |
         ns_esc_backspace |
         ns_esc_horizontal_tab |
         ns_esc_line_feed |
         ns_esc_vertical_tab |
         ns_esc_form_feed |
         ns_esc_carriage_return |
         ns_esc_escape |
         ns_esc_space |
         ns_esc_double_quote |
         ns_esc_slash |
         ns_esc_backslash |
         ns_esc_next_line |
         ns_esc_non_breaking_space |
         ns_esc_line_separator |
         ns_esc_paragraph_separator |
         ns_esc_8bit |
         ns_esc_16bit |
         ns_esc_32bit);

    constexpr auto s_indent = cps::indent;
    constexpr auto s_indent_less_than = cps::less_indent;
    constexpr auto s_indent_less_or_equal = cps::less_eq_indent;

    constexpr auto s_separate_in_line = str("s-separate-in-line", ~s_white | bol);

    constexpr auto s_block_line_prefix = str("s-block-line-prefix", s_indent);
    constexpr auto s_flow_line_prefix = group("s-flow-line-prefix", str("s_indent", s_indent) & s_separate_in_line);

    constexpr auto s_line_prefix = group("s-line-prefix", proxy([](auto&& seq, auto&& ctx, auto&& rec) {
                                             switch (ctx.get_context()) {
                                                 case Context::BLOCK_IN:
                                                 case Context::BLOCK_OUT:
                                                     return s_block_line_prefix(seq, ctx, rec);
                                                 case Context::FLOW_IN:
                                                 case Context::FLOW_OUT:
                                                     return s_flow_line_prefix(seq, ctx, rec);
                                                 default:
                                                     break;
                                             }
                                             ctxs::context_error(seq,ctx, "unexpected context");
                                             return Status::fatal;
                                         }));

    constexpr auto l_empty = group("l-empty", (s_line_prefix | str("s-indent-less-than", s_indent_less_than)) & b_as_line_feed);

    constexpr auto b_l_trimmed = group("b-l-trimmed", b_non_content & ~l_empty);

    constexpr auto b_as_space = str("b-as-space", b_break);

    constexpr auto b_l_folded = group("b-l-folded", b_l_trimmed | b_as_space);

    enum class Context {
        BLOCK_IN,
        BLOCK_OUT,
        BLOCK_KEY,
        FLOW_IN,
        FLOW_OUT,
        FLOW_KEY,
    };

    constexpr auto ctx_scope(Context ctx, auto&& f) {
        return proxy([=](auto&& seq, auto&& ctx, auto&& rec) {
            ctx.push_context(ctx);
            constexpr auto d = helper::defer([&] {
                ctx.pop_context();
            });
            return f(seq, ctx, rec);
        });
    };

    constexpr auto s_flow_folded = group(
        "s-flow-folded",
        -s_separate_in_line& ctx_scope(Context::FLOW_IN, b_l_folded) & s_flow_line_prefix);

    constexpr auto c_nb_comment_text = str("c-nb-comment-text", c_comment& nb_char);

    constexpr auto b_comment = str("b-comment", b_non_content | eos);

    constexpr auto s_b_comment = group("s-b-comment", -(s_separate_in_line & -c_nb_comment_text) & b_comment);

    constexpr auto l_comment = group("l-comment", s_separate_in_line & -c_nb_comment_text & b_comment);

    constexpr auto s_l_comments = group("s-l-comments", (s_b_comment | bol) & -~l_comment);

    constexpr auto s_separate_lines = group("s-separate-line", (s_l_comments & s_flow_line_prefix | s_separate_in_line));

    constexpr auto s_separate = group("s-separate", proxy([](auto&& seq, auto&& ctx, auto&& rec) {
                                          switch (ctx.get_context()) {
                                              case Context::BLOCK_IN:
                                              case Context::BLOCK_OUT:
                                              case Context::FLOW_IN:
                                              case Context::FLOW_OUT:
                                                  return s_separate_lines(seq, ctx, rec);
                                              case Context::FLOW_KEY:
                                              case Context::BLOCK_KEY:
                                                  return s_separate_in_line(seq, ctx, rec);
                                              default:
                                                  break;
                                          }
                                          ctxs::context_error(seq,ctx, "unexpected context");
                                          return Status::fatal;
                                      }));

    constexpr auto ns_directive_name = str("ns-directive-name", ~ns_char);
    constexpr auto ns_directive_parameter = str("ns-directive-parameter", ~ns_char);

    constexpr auto ns_reserved_directive = group(
        "ns-reserved-directive",
        ns_directive_name & -~(s_separate_in_line & ns_directive_parameter));

    constexpr auto ns_yaml_version = str("ns-yaml-version", ~ns_dec_digit & '.'_l & ~ns_dec_digit);
    constexpr auto ns_yaml_directive = group("ns-yaml-directive", str("ns-directive-name", "YAML"_l) & s_separate_in_line & ns_yaml_version);

    constexpr auto c_named_tag_handle = str("c-named-tag-handle", c_tag & ~ns_word_char & c_tag);
    constexpr auto c_secondary_tag_handle = str("c-scondary-tag-handle", "!!"_l);
    constexpr auto c_primary_tag_handle = str("c-primary-tag-handle", c_tag);

    constexpr auto c_tag_handle = group("c_tag_handle", c_named_tag_handle | c_secondary_tag_handle | c_primary_tag_handle);

    constexpr auto c_ns_local_tag_prefix = str("c-ns-local-tag-prefix", c_tag & ~ns_uri_char);
    constexpr auto ns_global_tag_prefix = str("c-ns-global-tag-prefix", ns_tag_char & ~ns_uri_char);
    constexpr auto ns_tag_prefix = group("ns-tag-prefix", c_ns_local_tag_prefix | ns_global_tag_prefix);

    constexpr auto ns_tag_directive = group(
        "ns-tag-directive",
        str("ns-directive-name", "TAG"_l) & s_separate_in_line & c_tag_handle & s_separate_in_line & ns_tag_prefix);

    constexpr auto l_directive = group(
        "l-directive",
        c_directive&(ns_yaml_directive | ns_tag_directive | ns_reserved_directive) & s_l_comments);

    constexpr auto c_verbaim_tag = str("c-verbaim-tag", "!<"_l & +ns_uri_char & '>'_l);
    constexpr auto c_ns_shorthand_tag = group("c-ns-shorthand-tag", c_tag_handle& str("ns-tag-char", ~ns_tag_char));
    constexpr auto c_non_specific_tag = str("c-non-specific-tag", '!'_l);

    constexpr auto c_tag_property = group("c-ns-tag-property", c_verbaim_tag | c_ns_shorthand_tag | c_non_specific_tag);

    constexpr auto ns_anchor_char = not_(c_flow_indicator) & ns_char;
    constexpr auto ns_anchor_name = str("ns-anchor-name", ~ns_anchor_char);

    constexpr auto c_ns_anchor_property = group("c-ns-anchor-property", str("c-anchor", c_anchor) & ns_anchor_name);

    constexpr auto c_ns_alias_node = group("c-ns-alias-node", str("c-alias", c_alias) & ns_anchor_name);

    constexpr auto e_scalar = str("e-scalar", null);
    constexpr auto e_node = group("e-node", e_scalar);

    constexpr auto nb_double_char = c_ns_esc_char |
                                    (not_('\\'_l | '"'_l) & nb_json);
    constexpr auto ns_double_char = not_(s_white) & nb_double_char;

    constexpr auto nb_double_one_line = str("nb-double-one-line", -~nb_double_char);

    constexpr auto s_double_escaped = str("s-double-escaped",
                                          -~s_white& c_escape& b_non_content &
                                              -~ctx_scope(Context::FLOW_IN, l_empty) &
                                              s_flow_line_prefix);

    constexpr auto s_double_break = group("s-double-break",
                                          s_double_escaped | s_flow_folded);

    constexpr auto nb_ns_double_in_line = str("nb-ns-double-in-line", -~(-~s_white& ns_double_char));

    constexpr auto s_double_next_line = group(
        "s-double-next-line",
        s_double_break & -(ns_double_char & nb_ns_double_in_line &
                           (method_proxy(s_double_next_line) | str("s-white", -~s_white))));

    constexpr auto nb_double_multi_line = group(
        "nb-double-multi-line",
        nb_ns_double_in_line&(s_double_next_line | str("s-white", -~s_white)));

    constexpr auto ns_double_text = group("nb-double-text", proxy([](auto&& seq, auto&& ctx, auto&& rec) {
                                              switch (ctx.get_context()) {
                                                  case Context::FLOW_IN:
                                                  case Context::FLOW_OUT:
                                                      return nb_double_multi_line(seq, ctx, rec);
                                                  case Context::FLOW_KEY:
                                                  case Context::BLOCK_KEY:
                                                      return nb_double_one_line(seq, ctx, rec);
                                                  default:
                                                      break;
                                              }
                                              ctxs::context_error(seq,ctx, "unexpected context");
                                              return Status::fatal;
                                          }));

    constexpr auto c_double_quoted = group(
        "c-double-quoted",
        str("c-double-quote", c_double_quote) &
            ns_double_text &
            str("c-double-quote", c_double_quote));

    constexpr auto c_quoted_quote = "''"_l;

    constexpr auto nb_single_char = c_quoted_quote |
                                    (not_('\''_l) & nb_char);
    constexpr auto ns_single_char = not_(s_white) & nb_double_char;

    constexpr auto nb_single_one_line = str("nb-single-one-line", -~nb_single_char);

    constexpr auto s_double_escaped = str("s-double-escaped",
                                          -~s_white& c_escape& b_non_content &
                                              -~ctx_scope(Context::FLOW_IN, l_empty) &
                                              s_flow_line_prefix);

    constexpr auto s_double_break = group("s-double-break",
                                          s_double_escaped | s_flow_folded);

    constexpr auto nb_ns_single_in_line = str("nb-ns-single-in-line", -~(-~s_white& ns_single_char));

    constexpr auto s_single_next_line = group(
        "s-single-next-line",
        s_flow_folded & -(ns_single_char & nb_ns_single_in_line &
                          (method_proxy(s_single_next_line) | str("s-white", -~s_white))));

    constexpr auto nb_single_multi_line = group(
        "nb-double-multi-line",
        nb_ns_double_in_line&(s_single_next_line | str("s-white", -~s_white)));

    constexpr auto ns_single_text = group("nb-single-text", proxy([](auto&& seq, auto&& ctx, auto&& rec) {
                                              switch (ctx.get_context()) {
                                                  case Context::FLOW_IN:
                                                  case Context::FLOW_OUT:
                                                      return nb_single_multi_line(seq, ctx, rec);
                                                  case Context::FLOW_KEY:
                                                  case Context::BLOCK_KEY:
                                                      return nb_single_one_line(seq, ctx, rec);
                                                  default:
                                                      break;
                                              }
                                              ctxs::context_error(seq,ctx, "unexpected context");
                                              return Status::fatal;
                                          }));

    constexpr auto c_single_quoted = group(
        "c-single-quoted",
        str("c-single-quote", c_double_quote) &
            ns_single_text &
            str("c-single-quote", c_double_quote));

}  // namespace futils::yaml::parser
