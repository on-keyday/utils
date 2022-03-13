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

extern utils::wrap::UtfOut& cout;
constexpr auto mode = utils::cmdline::option::ParseFlag::assignable_mode;

namespace netutil {
    using namespace utils::cmdline;

    extern bool* help;
    extern bool* verbose;
    void common_option(subcmd::RunCommand& ctx);

    // set option
    void httpreq_option(subcmd::RunContext& ctx);

    // command
    int httpreq(subcmd::RunCommand& cmd);
}  // namespace netutil
