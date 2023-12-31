/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include <string>
#include <memory>
#include "simple_node.h"
#include "error.h"

namespace utils::comb2::tree::node::traverse {

    using NodePtr = std::shared_ptr<utils::comb2::tree::node::Node>;
#define SWITCH(tag)                   \
    if (auto tag___ = (tag); false) { \
    }
#define CASE(tag) \
    else if (tag___ == tag)
#define EXPECT(to, meth, n, expect_tag)                                                                                                                                                                                                \
    auto to = meth(n);                                                                                                                                                                                                                 \
    if (!to || to->tag != expect_tag) {                                                                                                                                                                                                \
        return report_error(out, n, std::string("expect tag: ") + ((const char*)expect_tag ? expect_tag : "(no_tag)") + "(" #meth "," __FILE__ ":" + utils::number::to_string<std::string>(__LINE__) + ":1) but not . library bug!!"); \
    }

    // push error then return false
    bool report_error(auto& err, const NodePtr& n, const std::string& msg = "invalid structure. library bug!!") {
        err.err.push(Error{msg, n->pos});
        return false;
    }

    bool not_implemented(auto& err, const NodePtr& n) {
        return report_error(err, n, std::string("unexpected tag: ") + n->tag + ": not implemented");
    }

    bool unexpected_token(auto& err, const NodePtr& n, const std::string& tok) {
        return report_error(err, n, std::string("unexpected token: ") + tok + ": not implemented");
    }

}  // namespace utils::comb2::tree::node::traverse
