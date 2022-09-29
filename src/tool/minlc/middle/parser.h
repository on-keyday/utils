/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <minilang/minlstat.h>

namespace minlc {
    namespace mi = utils::minilang;
    namespace parsers {
        constexpr auto comment = mi::comment(mi::ComBr{"/*", "*/"}, mi::ComBr{"//"});
        constexpr auto head = mi::until_eof_or_not_matched(mi::statment(comment, mi::stat_head_default));
        constexpr auto body_impl = mi::statment(comment, mi::stat_default);
        constexpr auto body = mi::until_eof_or_not_matched(body_impl);
        constexpr auto head_parser = mi::make_stats(mi::null_userdefined, head);
        constexpr auto primitives = mi::primitives(mi::make_funcexpr_primitive(body_impl, mi::typesig_default), mi::type_primitive(body_impl, mi::typesig_default));
        constexpr auto body_parser = mi::make_stats(primitives, body);
    }  // namespace parsers
    using pNode = std::shared_ptr<mi::MinNode>;
    std::pair<pNode, pNode> parse(auto&& seq, auto&& errc) {
        auto h = parsers::head_parser(seq, errc);
        if (!h) {
            return {};
        }
        auto b = parsers::body_parser(seq, errc);
        if (!b) {
            return {};
        }
        return {h, b};
    }
}  // namespace minlc
