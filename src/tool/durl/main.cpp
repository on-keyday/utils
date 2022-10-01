/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "durl.h"
#include <wrap/argv.h>

namespace durl {

    void loadConfig() {
    }

    auto& cout = utils::wrap::cout_wrap();
    GlobalOption global;
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
            cout << "error :" << ctx.erropt() << ": " << msg << "\n";
        }
        return ctx.run();
    }
}  // namespace durl

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    return durl::durl_main(argc, argv);
}
