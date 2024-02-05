/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/error.h>
using namespace futils;

int main() {
    fnet::error::Error err(errno, futils::fnet::error::Category::os);
    assert(err.category() == fnet::error::Category::os);
    struct LocalError {
        int val = 0;
        void error(helper::IPushBacker<> pb) {
            strutil::append(pb, "local_error");
        }

        bool operator==(const LocalError& eq) const {
            return true;
        }

        fnet::error::Error unwrap() {
            return fnet::error::Error(LocalError{});
        }
    };
    err = fnet::error::Error(LocalError{});
    assert(err.type() == fnet::error::ErrorType::ptr);
    err = fnet::error::Error("EOF", fnet::error::Category::os);
    assert(err == fnet::error::Error("EOF", fnet::error::Category::os));
    err = fnet::error::Error(LocalError{.val = 1});
    assert(err == fnet::error::Error(LocalError{}));
    constexpr auto EEOF = fnet::error::Error("EOF", fnet::error::Category::os);
    constexpr auto numerr = fnet::error::Error(2, fnet::error::Category::lib);
    auto ptr = err.as<LocalError>();
    assert(ptr->val == 1);
    err = err.unwrap();
    ptr = err.as<LocalError>();
    assert(ptr->val == 0);
}
