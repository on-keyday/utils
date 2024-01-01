/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <env/env.h>
#include <env/env_sys.h>
#include <cassert>
#include <wrap/cout.h>

int main() {
    futils::env::test::check_expand();
    auto res = futils::env::sys::test::check_setter_getter();
    assert(res);
    using EnvT = futils::env::sys::env_value_t;
    auto env = futils::env::sys::env_getter();
    env.get({}, [](EnvT key, EnvT value) {
        if (key[0] == '=') {
            futils::wrap::cout_wrap() << "wow! " << key << "\n";
        }
        // this may be `key=value` form
        auto key_value = EnvT{key.begin(), value.end()};
        futils::wrap::cerr_wrap() << key_value << "\n";
        return true;
    });
    auto path = env.get<std::string>("PATH");
    auto gopath = env.get<std::string>("GOPATH");
    auto text = futils::env::expand<std::string>("PATH is $PATH and GOPATH is $GOPATH", futils::env::sys::expand_sys<std::string>());
    assert(text == "PATH is " + path + " and GOPATH is " + gopath);
    futils::wrap::cout_wrap() << text << "\n";
    auto s = futils::env::sys::env_setter().set("PATH", "unspecific");
    assert(s);
    text = futils::env::expand<std::string>("PATH is $PATH and GOPATH is $GOPATH", futils::env::sys::expand_sys<std::string>());
    assert(text == "PATH is unspecific and GOPATH is " + gopath);
    futils::wrap::cout_wrap() << text << "\n";
    path.clear();
    env.get(path, "PATH");
    futils::wrap::cout_wrap() << path << "\n";
}
