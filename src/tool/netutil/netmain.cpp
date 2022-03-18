/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "crtdbg.h"
#include "subcommand.h"
#include "../../include/wrap/argv.h"
#include <windows.h>
#include <number/to_string.h>
#include <number/insert_space.h>
using namespace utils;
using namespace cmdline;

wrap::UtfOut& cout = wrap::cout_wrap();

namespace netutil {
    bool* help;
    bool* verbose;
    bool* quiet;

    void common_option(subcmd::RunCommand& ctx) {
        if (help) {
            ctx.option().VarBool(help, "h,help", "show help");
            ctx.option().VarBool(verbose, "v,verbose", "verbose log");
            ctx.option().VarBool(quiet, "quiet", "quiet log");
        }
        else {
            help = ctx.option().Bool("h,help", false, "show help");
            verbose = ctx.option().Bool("v,verbose", false, "verbose log");
            quiet = ctx.option().Bool("quiet", false, "quiet log");
        }
    }
}  // namespace netutil

int main_help(subcmd::RunCommand& cmd) {
    cout << cmd.Usage(mode);
    return 0;
}

int main_proc(int argc, char** argv) {
    wrap::U8Arg _(argc, argv);
    subcmd::RunContext ctx;
    ctx.Set(argv[0], main_help, "cli network utility", "[command]");
    netutil::common_option(ctx);
    netutil::httpreq_option(ctx);
    auto err = subcmd::parse(argc, argv, ctx, mode);
    if (auto msg = error_msg(err)) {
        cout << argv[0] << ": error: " << ctx.erropt() << ": " << msg << "\n";
        return -1;
    }
    return ctx.run();
}

_CRT_ALLOC_HOOK base_alloc_hook;

int alloc_hook(int nAllocType, void* pvData,
               size_t nSize, int nBlockUse, long lRequest,
               const unsigned char* szFileName, int nLine) {
    auto res = base_alloc_hook(nAllocType, pvData, nSize, nBlockUse, lRequest, szFileName, nLine);
    auto save_log = [&](auto name) {
        number::Array<64, char> arr{0};
        helper::appends(arr, name, ": ");
        if (nSize < 10) {
            OutputDebugStringA("check!\n");
        }
        number::insert_space(arr, 8, nSize);
        number::to_string(arr, nSize);
        helper::appends(arr, " req: ");
        number::to_string(arr, lRequest);
        helper::append(arr, "\n");
        OutputDebugStringA(arr.buf);
    };
    if (nAllocType == _HOOK_ALLOC) {
        save_log("alloc");
    }
    else if (nAllocType == _HOOK_FREE) {
        if (nSize != 0) {
            save_log("free");
        }
    }
    return res;
}

int main(int argc, char** argv) {
    _CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    base_alloc_hook = _CrtGetAllocHook();
    _CrtSetAllocHook(alloc_hook);
    return main_proc(argc, argv);
}
