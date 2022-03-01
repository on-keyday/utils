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

            struct Command {
               protected:
                wrap::string mainname;
                wrap::string desc;
                wrap::string usage;
                Command* parent = nullptr;
                Command* reached_child = nullptr;
                option::Context ctx;
                bool need_subcommand = false;
                wrap::map<wrap::string, wrap::shared_ptr<Command>> subcommand;
                wrap::vector<wrap::shared_ptr<Command>> list;
                wrap::vector<wrap::string> alias;

                void update_reached(Command* p) {
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
                const wrap::string& name() {
                    return mainname;
                }

                virtual wrap::vector<wrap::string>& arg() {
                    assert(parent);
                    return parent->arg();
                }

                option::Context& get_option() {
                    return ctx;
                }

                wrap::shared_ptr<Command> find_cmd(auto&& name) {
                    auto found = subcommand.find(name);
                    if (found == subcommand.end()) {
                        return nullptr;
                    }
                    return *found;
                }

                auto cmd_end() {
                    return nullptr;
                }

                bool need_sub() const {
                    return need_subcommand;
                }

                template <class Usage = const char*>
                wrap::shared_ptr<Command> SubCommand(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    wrap::string mainname;
                    wrap::vector<wrap::string> alias;
                    if (!option::make_cvtvec(name, mainname, subcommand, alias)) {
                        return nullptr;
                    }
                    auto sub = wrap::make_shared<Command>();
                    sub->mainname = std::move(mainname);
                    sub->alias = std::move(alias);
                    sub->desc = utf::convert<wrap::string>(desc);
                    sub->usage = utf::convert<wrap::string>(usage);
                    sub->need_subcommand = need_subcommand;
                    subcommand.emplace(sub->mainname, sub);
                    for (auto& s : sub->alias) {
                        subcommand.emplace(s, sub);
                    }
                    return sub;
                }
            };

            struct Context : public Command {
               private:
                wrap::vector<wrap::string> arg_;

               public:
                wrap::vector<wrap::string>& arg() override {
                    return arg_;
                }
            };
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
