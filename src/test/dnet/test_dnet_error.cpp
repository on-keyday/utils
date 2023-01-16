/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/error.h>
using namespace utils;

int main() {
    dnet::error::Error err(errno);
    assert(err.category() == dnet::error::ErrorCategory::syserr);
    struct LocalError {
        int val = 0;
        void error(helper::IPushBacker pb) {
            helper::append(pb, "local_error");
        }

        bool operator==(const LocalError& eq) const {
            return true;
        }

        dnet::error::Error unwrap() {
            return dnet::error::Error(LocalError{});
        }
    };
    err = dnet::error::Error(LocalError{});
    assert(err.type() == dnet::error::ErrorType::wrap);
    err = dnet::error::Error("EOF");
    assert(err == dnet::error::Error("EOF"));
    err = dnet::error::Error(LocalError{.val = 1});
    assert(err == dnet::error::Error(LocalError{}));
    constexpr auto EEOF = dnet::error::Error("EOF", dnet::error::ErrorCategory::syserr);
    constexpr auto numerr = dnet::error::Error(2, dnet::error::ErrorCategory::liberr);
    auto ptr = err.as<LocalError>();
    assert(ptr->val == 1);
    err = err.unwrap();
    ptr = err.as<LocalError>();
    assert(ptr->val == 0);
}
