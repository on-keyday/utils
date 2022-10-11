/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "durl.h"
#include <wrap/argv.h>
#include <wrap/exepath.h>
#include <file/file_view.h>
#include <filesystem>

namespace durl {
    namespace fs = std::filesystem;
    GlobalOption global;

    bool loadjson(fs::path& path) {
        path.append("durl_config.json");
        utils::file::View view;
        if (view.open(path.c_str())) {
            auto js = utils::json::parse<utils::json::JSON>(view);
            if (!js) {
                return false;
            }
            auto glob = js.at("global");
            if (glob) {
                if (!utils::json::convert_from_json(*glob, global, utils::json::FromFlag::force_element)) {
                    global = {};
                    return false;
                }
            }
            auto uri = js.at("uri");
            if (uri) {
                if (!utils::json::convert_from_json(*uri, uriopt, utils::json::FromFlag::force_element)) {
                    global = {};
                    uriopt = {};
                    return false;
                }
            }
            cout << "config loaded from " << path.u8string();
            return true;
        }
        return false;
    }

    void loadConfig() {
        if (auto path = utils::wrap::get_exepath(); path != "") {
            auto dir = fs::path((const char8_t*)path.c_str()).parent_path();
            if (loadjson(dir)) {
                return;
            }
        }
        std::error_code ec;
        auto cd = fs::current_path(ec);
        if (ec) {
            return;
        }
        loadjson(cd);
    }

    auto& cout = utils::wrap::cout_wrap();
    auto& cerr = utils::wrap::cerr_wrap();

    int durl_main(int argc, char** argv) {
        subcmd::RunContext ctx;
        ctx.Set(
            argv[0], [](subcmd::RunCommand& c) {
                return global.show_usage(c);
            },
            "curl like command line network tool");
        global.bind(ctx);
        auto err = subcmd::parse(argc, argv, ctx, opt::ParseFlag::assignable_mode);
        if (opt::perfect_parsed(err) && global.help) {
            return ctx.run();
        }
        if (auto msg = opt::error_msg(err)) {
            cerr << "error :" << ctx.erropt() << ": " << msg << "\n";
        }
        return ctx.run();
    }
}  // namespace durl

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    return durl::durl_main(argc, argv);
}
