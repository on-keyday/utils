/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/dll/dllcpp.h>
#include <fnet/dll/lazy/load_error.h>
#include <fnet/storage.h>

namespace futils::fnet::lazy {

    struct PltSpecific {
        flex_storage storage;

        void error(auto&& pb) {
            strutil::append(pb, storage);
        }
    };

    error::Error load_error(const char* msg) {
        auto err = error::Error(msg, error::Category::lib, error::fnet_lib_load_error);
        auto sysErr = error::Errno();
        auto err_str = platform_specific_error();
        auto sysErr2 = error::Errno();
        if (err_str) {
            PltSpecific ps;
            ps.storage = view::rvec(err_str);
            err = error::ErrList{ps, err};
        }
        if (err.code() != 0) {
            err = error::ErrList{sysErr, err};
        }
        if (sysErr2.code() != 0 && sysErr.code() != sysErr2.code()) {
            err = error::ErrList{sysErr2, err};
        }
        return err;
    }
}  // namespace futils::fnet::lazy
