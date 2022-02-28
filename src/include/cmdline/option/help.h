/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// help - show option help
#pragma once
#include "option_set.h"

namespace utils {
    namespace cmdline {
        namespace option {
            template <class Result, class OptVec>
            bool desc(Result& result, ParseFlag flag, wrap::shared_ptr<Option>& opt) {
                if (!opt) {
                    return false;
                }
                auto write = [&](auto&&... v) {
                    helper::appends(result, v...);
                };
                auto write_assign = [&](auto&& argdesc) {
                    if (any(flag & ParseFlag::sf_assign)) {
                        write(get_assignment(flag));
                    }
                    else {
                        write(" ");
                    }
                    write(argdesc);
                };
                auto write_opt = [&](auto&& opt, auto&& argdesc) {
                    bool sht = any(flag & ParseFlag::pf_short);
                    bool log = any(flag & ParseFlag::pf_long);
                    bool val = any(flag & ParseFlag::pf_value);
                    if (sht && log) {
                        if (bufsize(opt) == 1) {
                            write("-", opt);
                            if (val) {
                                write(argdesc);
                            }
                            else {
                                write_assign(argdesc);
                            }
                        }
                        else {
                            write("--", opt);
                            write_assign(argdesc);
                        }
                    }
                    else if (sht) {
                        write(get_prefix(flag), opt);
                        if (val) {
                            write(argdesc);
                        }
                        else {
                            write_assign(argdesc);
                        }
                    }
                    else if (log) {
                        write("--", opt);
                    }
                };
                write_opt(opt->mainname, opt->argdesc);
                for (auto& alias : opt->aliases) {
                    write(", ");
                    write_opt(alias, opt->argdesc);
                }
            }
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
