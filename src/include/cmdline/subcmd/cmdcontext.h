/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cmdcontext - subcommand context
#pragma once
#include "../option/optcontext.h"
#include "cmdrunner.h"

namespace utils {
    namespace cmdline {
        namespace subcmd {

            enum class CmdType {
                command,
                context,
                runcommand,
                runcontext,
            };
            template <class Derived>
            struct CommandBase {
               protected:
                wrap::string mainname;
                wrap::string desc;
                wrap::string usage;
                Derived* parent_ = nullptr;
                Derived* reached_child = nullptr;
                option::Context ctx;
                bool need_subcommand = false;
                wrap::map<wrap::string, wrap::shared_ptr<Derived>> subcommand;
                wrap::vector<wrap::shared_ptr<Derived>> list;
                wrap::vector<wrap::string> alias;

                void update_reached(Derived* p) {
                    reached_child = p;
                    if (parent_) {
                        parent_->update_reached(p);
                    }
                }

                option::Context& context() {
                    if (parent_) {
                        parent_->update_reached(this);
                    }
                    return ctx;
                }

                template <class Ctx>
                friend option::FlagType parse(int argc, char** argv, Ctx& ctx,
                                              option::ParseFlag flag,
                                              int start_index);
                auto cmd_end() {
                    return nullptr;
                }

                bool need_sub() const {
                    return need_subcommand;
                }

                template <class CmdType = Derived, class Usage = const char*>
                wrap::shared_ptr<CmdType> make_subcommand(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    wrap::string mainname;
                    wrap::vector<wrap::string> alias;
                    if (!option::make_cvtvec(name, mainname, subcommand, alias)) {
                        return nullptr;
                    }
                    auto sub = wrap::make_shared<CmdType>();
                    sub->mainname = std::move(mainname);
                    sub->alias = std::move(alias);
                    sub->desc = utf::convert<wrap::string>(desc);
                    sub->usage = utf::convert<wrap::string>(usage);
                    sub->need_subcommand = need_subcommand;
                    sub->parent = this;
                    subcommand.emplace(sub->mainname, sub);
                    for (auto& s : sub->alias) {
                        subcommand.emplace(s, sub);
                    }
                    return sub;
                }

                template <class Usage = const char*>
                void set_self(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    this->mainname = utf::convert<wrap::string>(name);
                    this->desc = utf::convert<wrap::string>(desc);
                    this->usage = utf::convert<wrap::string>(usage);
                    this->need_subcommand = need_subcommand;
                }

               public:
                const wrap::string& name() {
                    return mainname;
                }

                virtual wrap::vector<wrap::string>& arg() {
                    assert(parent_);
                    return parent_->arg();
                }

                option::Context& get_option() {
                    return ctx;
                }

                Derived* parent() {
                    return parent_;
                }

                wrap::shared_ptr<Derived> find_cmd(auto&& name) {
                    auto found = subcommand.find(name);
                    if (found == subcommand.end()) {
                        return nullptr;
                    }
                    return *found;
                }
            };

            struct Command : public CommandBase<Command> {
                template <class Usage = const char*>
                wrap::shared_ptr<Command> SubCommand(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    return this->make_subcommand<Command>(name, desc, usage, need_subcommand);
                }
            };

            struct Context : public Command {
               private:
                wrap::vector<wrap::string> arg_;

               public:
                template <class Usage = const char*>
                void Set(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    set_self(name, desc, usage, need_subcommand);
                }

                wrap::vector<wrap::string>& arg() override {
                    return arg_;
                }
            };

            struct RunCommand : public CommandBase<RunCommand> {
                using runner_t = CommandRunner<RunCommand>;

               protected:
                runner_t Run;

               public:
                template <class Usage = const char*>
                wrap::shared_ptr<RunCommand> SubCommand(auto&& name, runner_t runner, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    auto ptr = this->make_subcommand<RunCommand>(name, desc, usage, need_subcommand);
                    if (!ptr) {
                        return nullptr;
                    }
                    ptr->Run = std::move(runner);
                    return ptr;
                }

                int run() {
                    if (!this->reached_child) {
                        if (!this->Run) {
                            return -1;
                        }
                        return this->Run(*this);
                    }
                    if (!this->reached_child->Run) {
                        return -1;
                    }
                    return this->reached_child->Run(*this->reached_child);
                }
            };

            struct RunContext : public RunCommand {
               private:
                wrap::vector<wrap::string> arg_;

               public:
                wrap::vector<wrap::string>& arg() override {
                    return arg_;
                }

                template <class Usage = const char*>
                void Set(auto&& name, CommandRunner<RunCommand> runner, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    set_self(name, desc, usage, need_subcommand);
                    this->Run = std::move(runner);
                }
            };
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
