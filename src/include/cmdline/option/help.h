/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// help - show option help
#pragma once
#include "option_set.h"
#include "../../helper/pushbacker.h"

namespace utils {
    namespace cmdline {
        namespace option {
            auto opts_write(auto& result) {
                return [&](ParseFlag flag, auto& opt) {
                    auto write = [&](auto&&... v) {
                        helper::appends(result, v...);
                    };
                    auto write_assign = [&](auto&& argdesc) {
                        if (bufsize(argdesc) == 0) {
                            return;
                        }
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
                                if (val && !any(flag & ParseFlag::assign_anyway_val)) {
                                    if (bufsize(argdesc)) {
                                        write(argdesc);
                                    }
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
                                if (bufsize(argdesc)) {
                                    write(argdesc);
                                }
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
                };
            }

            template <class Result, class OptVec>
            void desc(Result& result, ParseFlag flag, OptVec& vec, const char* indent = "    ") {
                helper::CountPushBacker push;
                size_t maxlen = 0;
                auto tmp = opts_write(push);
                for (wrap::shared_ptr<Option>& opt : vec) {
                    helper::append(push, indent);
                    tmp(flag, opt);
                    if (maxlen < push.count) {
                        maxlen = push.count;
                    }
                    push.count = 0;
                }
                helper::CountPushBacker<Result&> cb{result};
                auto actual = opts_write(cb);
                for (wrap::shared_ptr<Option>& opt : vec) {
                    helper::append(cb, indent);
                    actual(flag, opt);
                    while (maxlen > cb.count) {
                        cb.push_back(' ');
                    }
                    helper::append(result, " : ");
                    helper::append(result, opt->help);
                    helper::append(result, "\n");
                    cb.count = 0;
                }
            }
            template <class Result, class OptVec>
            Result desc(ParseFlag flag, OptVec& vec, const char* indent = "    ") {
                Result result;
                desc(result, flag, vec, indent);
                return result;
            }
        }  // namespace option
    }      // namespace cmdline
}  // namespace utils
