/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <env/env.h>
#include <env/env_sys.h>
#include <cassert>
#include <wrap/cout.h>

int main() {
    utils::env::test::check_expand();
    auto res = utils::env::sys::test::check_setter_getter();
    assert(res);
    using EnvT = utils::env::sys::env_value_t;
    auto env = utils::env::sys::env_getter();
    env.get({}, [](EnvT key, EnvT value) {
        if (key[0] == '=') {
            utils::wrap::cout_wrap() << "wow! " << key << "\n";
        }
        // this may be `key=value` form
        auto key_value = EnvT{key.begin(), value.end()};
        utils::wrap::cerr_wrap() << key_value << "\n";
        return true;
    });
    auto path = env.get<std::string>("PATH");
    auto gopath = env.get<std::string>("GOPATH");
    auto text = utils::env::expand<std::string>("PATH is $PATH and GOPATH is $GOPATH", utils::env::sys::expand_sys<std::string>());
    assert(text == "PATH is " + path + " and GOPATH is " + gopath);
    utils::wrap::cout_wrap() << text << "\n";
    auto s = utils::env::sys::env_setter().set("PATH", "unspecific");
    assert(s);
    text = utils::env::expand<std::string>("PATH is $PATH and GOPATH is $GOPATH", utils::env::sys::expand_sys<std::string>());
    assert(text == "PATH is unspecific and GOPATH is " + gopath);
    utils::wrap::cout_wrap() << text << "\n";
    path.clear();
    env.get(path, "PATH");
    utils::wrap::cout_wrap() << path << "\n";
}
