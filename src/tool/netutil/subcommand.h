/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#pragma once
#include "../../include/cmdline/subcmd/cmdcontext.h"
#include "../../include/wrap/cout.h"
#include "../../include/json/json_export.h"
#include "../../include/json/convert_json.h"
#include "../../include/json/iterator.h"
#include "../../include/net_util/uri.h"

extern utils::wrap::UtfOut& cout;
constexpr auto mode = utils::cmdline::option::ParseFlag::assignable_mode;

namespace netutil {
    using namespace utils::cmdline;

    extern bool* help;
    extern bool* verbose;
    extern bool* quiet;
    void common_option(subcmd::RunCommand& ctx);

    // set option
    extern utils::wrap::string* cacert;
    extern bool* h2proto;
    void httpreq_option(subcmd::RunContext& ctx);

    // command
    int httpreq(subcmd::RunCommand& cmd);

    bool preprocese_a_uri(utils::wrap::internal::Pack&& cout, utils::wrap::string cuc, utils::wrap::string& raw, utils::net::URI& uri, utils::net::URI& prev);

    int http_do(subcmd::RunCommand& ctx, utils::wrap::vector<utils::net::URI>& uris);
}  // namespace netutil
