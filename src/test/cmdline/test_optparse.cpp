/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <cmdline/option/parser_loop.h>

#include <wrap/cout.h>

auto& cout = utils::wrap::cout_wrap();

int test_optparse(int argc, char** argv) {
    using namespace utils::cmdline;
    option::parse(argc, argv, [](option::CmdParseState& state) {
        if (state.state == option::FlagType::arg) {
            assert(state.arg);
            cout << "arg: "
                 << state.arg << "\n";
        }
        else if (state.state == option::FlagType::ignore) {
            cout << "remain args:\n";
            for (auto i = state.arg_track_index; i < state.argc; i++) {
                cout << state.argv[i] << "\n";
            }
        }
        else {
            if (state.arg) {
                cout << "flag: " << state.arg << "\n";
            }
            if (state.val) {
                cout << "value: " << state.val << "\n";
            }
        }
        return true;
    });
}

int main(int argc, char** argv) {
}