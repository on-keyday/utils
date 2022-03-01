/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cmdcontext - subcommand context
#pragma once
#include "../option/optcontext.h"

namespace utils {
    namespace cmdline {
        namespace subcmd {
            struct Context {
               private:
                Context* parent = nullptr;
                Context* reached_child = nullptr;
                option::Context ctx;
                bool need_subcommand;
                wrap::map<wrap::string, Context> subcommand;

                void update_reached(Context* p) {
                    reached_child = p;
                    if (parent) {
                        parent->update_reached(p);
                    }
                }

                option::Context& context() {
                    if (parent) {
                        parent->update_reached(this);
                    }
                    return ctx;
                }

                template <class Ctx>
                friend option::FlagType parse(int argc, char** argv, Ctx& ctx,
                                              option::ParseFlag flag,
                                              int start_index);

               public:
                option::Context& get_option() {
                    return ctx;
                }

                auto find_cmd(auto&& name) {
                    return subcommand.find(name);
                }

                auto cmd_end() {
                    return subcommand.end();
                }

                bool need_sub() const {
                    return need_subcommand;
                }
            };
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
