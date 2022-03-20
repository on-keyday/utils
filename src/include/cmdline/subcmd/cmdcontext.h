/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// cmdcontext - subcommand context
#pragma once
#include "parse.h"
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
                Derived* final_reached = nullptr;
                option::Context ctx;
                bool need_subcommand = false;
                wrap::map<wrap::string, wrap::shared_ptr<Derived>> subcommand;
                wrap::vector<wrap::shared_ptr<Derived>> list;
                wrap::vector<wrap::string> alias;

                void update_reached(Derived* p = nullptr) {
                    if (p) {
                        final_reached = p;
                        if (parent_) {
                            parent_->update_reached(p);
                        }
                    }
                    else {
                        if (parent_) {
                            parent_->reached_child = static_cast<Derived*>(this);
                            parent_->update_reached(static_cast<Derived*>(this));
                        }
                    }
                }

                option::Context& context() {
                    update_reached();
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

                template <class Usage = const char*>
                wrap::shared_ptr<Derived> make_subcommand(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    wrap::string mainname;
                    wrap::vector<wrap::string> alias;
                    if (!option::make_cvtvec(name, mainname, subcommand, alias)) {
                        return nullptr;
                    }
                    auto sub = wrap::make_shared<Derived>();
                    sub->mainname = std::move(mainname);
                    sub->alias = std::move(alias);
                    sub->desc = utf::convert<wrap::string>(desc);
                    sub->usage = utf::convert<wrap::string>(usage);
                    sub->need_subcommand = need_subcommand;
                    sub->parent_ = static_cast<Derived*>(this);
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
                template <class Str>
                void Usage(Str& str, option::ParseFlag flag) {
                    helper::appends(str, mainname, " - ", desc, "\n");
                    ctx.Usage(str, flag, mainname.c_str(), usage.c_str(), "    ");
                    if (subcommand.size()) {
                        helper::append(str, "Command:\n");
                    }
                    for (auto& cmd : subcommand) {
                        helper::appends(str, "    ", cmd.first, " - ", cmd.second->desc, "\n");
                    }
                }

                template <class Str = wrap::string>
                Str Usage(option::ParseFlag flag) {
                    Str v;
                    Usage(v, flag);
                    return v;
                }

                const wrap::string& name() {
                    return mainname;
                }

                wrap::string cuc() const {
                    return parent_ ? parent_->cuc() + ": " + mainname : mainname;
                }

                virtual wrap::vector<wrap::string>& arg() {
                    assert(parent_);
                    return parent_->arg();
                }

                option::Context& option() {
                    return ctx;
                }

                wrap::string erropt() {
                    wrap::string ret;
                    if (ctx.erropt().size()) {
                        ret = ctx.erropt();
                    }
                    if (reached_child) {
                        ret += ": ";
                        ret += reached_child->erropt();
                    }
                    return ret;
                }

                Derived* parent() {
                    return parent_;
                }

                wrap::shared_ptr<Derived> find_cmd(auto&& name) {
                    auto found = subcommand.find(name);
                    if (found == subcommand.end()) {
                        return nullptr;
                    }
                    return get<1>(*found);
                }
            };

            struct Command : public CommandBase<Command> {
                template <class Usage = const char*>
                wrap::shared_ptr<Command> SubCommand(auto&& name, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    return this->make_subcommand(name, desc, usage, need_subcommand);
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
                bool* help_flag_ptr;

                virtual int help_run(RunCommand& ctx) {
                    assert(parent_);
                    return parent_->help_run(ctx);
                }

                static int run_interal(RunCommand& ctx) {
                    if (ctx.help_flag_ptr && *ctx.help_flag_ptr) {
                        return ctx.help_run(ctx);
                    }
                    if (!ctx.Run) {
                        return -1;
                    }
                    return ctx.Run(ctx);
                }

               public:
                void set_help_ptr(bool* ptr) {
                    help_flag_ptr = ptr;
                }

                template <class Usage = const char*>
                wrap::shared_ptr<RunCommand> SubCommand(auto&& name, runner_t runner, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    auto ptr = this->make_subcommand(name, desc, usage, need_subcommand);
                    if (!ptr) {
                        return nullptr;
                    }
                    ptr->Run = std::move(runner);
                    return ptr;
                }

                int run() {
                    return run_interal(this->final_reached ? *this->final_reached : *this);
                }
            };

            struct RunContext : public RunCommand {
               private:
                using runner_t = CommandRunner<RunCommand>;
                wrap::vector<wrap::string> arg_;
                runner_t help_runner;

                int help_run(RunCommand& ctx) override {
                    if (!help_runner) {
                        return -1;
                    }
                    return help_runner(ctx);
                }

               public:
                wrap::vector<wrap::string>& arg() override {
                    return arg_;
                }

                void SetHelpRun(runner_t runner) {
                    help_runner = std::move(runner);
                }

                template <class Usage = const char*>
                void Set(auto&& name, runner_t runner, auto&& desc, Usage&& usage = "[option]", bool need_subcommand = false) {
                    set_self(name, desc, usage, need_subcommand);
                    this->Run = std::move(runner);
                }
            };
        }  // namespace subcmd
    }      // namespace cmdline
}  // namespace utils
