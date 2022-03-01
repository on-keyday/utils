/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <cmdline/option/parse.h>
#include <cmdline/option/optcontext.h>
#include <wrap/cout.h>
#include <wrap/argv.h>
using namespace utils::cmdline;
void test_optctx(int argc, char** argv, option::ParseFlag flag) {
    option::Context ctx;

    auto flagset = ctx.FlagSet("long,l", option::ParseFlag::pf_long, "long flag", "");
    ctx.VarFlagSet(flagset, "short,S", option::ParseFlag::pf_short, "short flag", "");
    auto test = ctx.Bool("test,t", false, "test flag", "");
    auto str = ctx.String<utils::wrap::string>("str,s", "default", "help", "VALUE");
    bool test2 = false;
    ctx.VarBool(&test2, "test2,2", "test flag 2", "");
    auto vec = ctx.VecString("vector,v", 2, "vector", "STR1 STR2");
    auto veci = ctx.VecInt("int,i", 2, "int vector", "INT1 INT2");
    ctx.UnboundBool("unbound,u", "unbound option", "");
    ctx.UnboundInt("int-unbound,B", "unbound int", "INT");
    ctx.UnboundString<utils::wrap::string>("str-unbound,b", "unbound string", "STRING");
    auto& cout = utils::wrap::cout_wrap();
    cout << ctx.help<utils::wrap::string>(flag);
    auto err = option::parse(argc, argv, ctx, utils::helper::nop, flag);
    if (auto val = error_msg(err)) {
        cout << "error: " << ctx.erropt() << ": " << val << "\n";
    }
    else {
        cout << "parse succeeded:\n";
        cout << "test: " << std::boolalpha << *test << "\n";
        cout << "str: " << *str << "\n";
        cout << "test2: " << std::boolalpha << test2 << "\n";
        cout << "vector:\n";
        for (auto& v : *vec) {
            cout << v << "\n";
        }
        cout << "int:\n";
        for (auto& i : *veci) {
            cout << i << "\n";
        }
        cout << "long and short: " << option::get_flag_state<utils::wrap::string>(*flagset)
             << "\n";
        auto unbound = ctx.find("unbound");
        cout << "unbound:\n";
        for (auto& it : unbound) {
            auto v = it.value.get_ptr<bool>();
            if (!v) {
                cout << "(not bool value)\n";
            }
            else {
                cout << std::boolalpha << *v << "\n";
            }
        }
        auto int_unbound = ctx.find("int-unbound");
        cout << "int-unbound:\n";
        for (auto& it : int_unbound) {
            auto v = it.value.get_ptr<std::int64_t>();
            if (!v) {
                cout << "(not int value)\n";
            }
            else {
                cout << *v << "\n";
            }
        }
        auto str_unbound = ctx.find("str-unbound");
        cout << "str-unbound:\n";
        for (auto& it : str_unbound) {
            auto v = it.value.get_ptr<utils::wrap::string>();
            if (!v) {
                cout << "(not string value)\n";
            }
            else {
                cout << *v << "\n";
            }
        }
    }
}

int main(int argc, char** argv) {
    utils::wrap::U8Arg _(argc, argv);
    test_optctx(argc, argv, option::ParseFlag::golang_mode);
    test_optctx(argc, argv, option::ParseFlag::optget_ext_mode);
    test_optctx(argc, argv, option::ParseFlag::assignable_mode);
}
