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
#include "../../include/thread/channel.h"
#include "../../include/async/worker.h"

extern utils::wrap::UtfOut& cout;
constexpr auto mode = utils::cmdline::option::ParseFlag::assignable_mode;

namespace netutil {
    using namespace utils::cmdline;

    extern bool* help;
    extern bool* verbose;
    extern bool* quiet;
    enum class TagFlag {
        none,
        redirect = 0x1,
        save_if_succeed = 0x2,
        show_request = 0x4,
        show_response = 0x5,
        show_body = 0x6,
    };

    DEFINE_ENUM_FLAGOP(TagFlag)

    struct TagCommand {
        utils::wrap::string file;
        TagFlag flag = TagFlag::none;
        utils::wrap::string method;
        utils::wrap::string data_loc;
        int ipver = 0;
    };

    using msg_chan = utils::thread::SendChan<utils::async::Any>;

    struct UriWithTag {
        utils::net::URI uri;
        TagCommand tagcmd;
    };

    void common_option(subcmd::RunCommand& ctx);

    // set option
    extern utils::wrap::string* cacert;
    extern bool* h2proto;
    extern bool* show_timer;
    void httpreq_option(subcmd::RunContext& ctx);
    void debug_log_option(subcmd::RunContext& ctx);

    // command
    int httpreq(subcmd::RunCommand& cmd);

    bool preprocese_a_uri(utils::wrap::internal::Pack&& cout, utils::wrap::string cuc, utils::wrap::string& raw, UriWithTag& uri, UriWithTag& prev);

    int http_do(subcmd::RunCommand& ctx, utils::wrap::vector<UriWithTag>& uris);

    int debug_log_parse(subcmd::RunCommand& ctx);

    bool parse_tagcommand(const utils::wrap::string& str, TagCommand& cmd, utils::wrap::string& err);
}  // namespace netutil
