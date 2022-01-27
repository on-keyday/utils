/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// logical_parser - basic and logical parser
#pragma once

#include "parser_base.h"

namespace utils {
    namespace parser {

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct RepeatParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            wrap::shared_ptr<SubParser> subparser;
            String token;
            Kind kind;
            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                auto ret = make_token<String, Kind, Vec>(token, kind, posctx);
                auto tmp = subparser->parse(input, posctx);
                if (!tmp.tok) {
                    return {};
                }
                ret->child.push_back(tmp.tok);
                tmp.tok->parent = ret;
                while (true) {
                    tmp = subparser->parse(input, posctx);
                    if (!tmp.tok) {
                        break;
                    }
                    ret->child.push_back(tmp.tok);
                    tmp.tok->parent = ret;
                }
                return {ret};
            }

            PaserKind declkind() const override {
                return PaserKind::repeat;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct AllowNoneParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            wrap::shared_ptr<SubParser> subparser;

            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                return subparser->parse(input, posctx);
            }

            bool none_is_not_error() const override {
                return true;
            }
            PaserKind declkind() const override {
                return PaserKind::allow_none;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct OnlyOneParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            Vec<wrap::shared_ptr<SubParser>> subparser;

            void add_parser(wrap::shared_ptr<SubParser> sub) {
                subparser.push_back(sub);
            }

            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                size_t begin = input.rptr, sucpos = begin;
                Pos postmp = posctx, suc = posctx;
                wrap::shared_ptr<token_t> ret = nullptr;
                for (auto& p : subparser) {
                    input.rptr = begin;
                    posctx = postmp;
                    auto tmp = p->parse(input, posctx);
                    if (tmp.fatal) {
                        return tmp;
                    }
                    if (tmp.tok) {
                        if (ret) {
                            input.rptr = begin;
                            posctx = postmp;
                            return {};
                        }
                        sucpos = input.rptr;
                        suc = posctx;
                        ret = std::move(tmp.tok);
                    }
                }
                input.rptr = sucpos;
                posctx = suc;
                return ret;
            }

            PaserKind declkind() const override {
                return PaserKind::only_one;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct SomePatternParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            Vec<wrap::shared_ptr<SubParser>> subparser;
            String token;
            Kind kind;

            void add_parser(wrap::shared_ptr<SubParser> sub) {
                subparser.push_back(sub);
            }

            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                size_t begin = input.rptr;
                Pos postmp = posctx;
                auto ret = make_token<String, Kind, Vec>(token, kind, posctx);
                for (auto& p : subparser) {
                    input.rptr = begin;
                    posctx = postmp;
                    auto tmp = p->parse(input, posctx);
                    if (tmp.fatal) {
                        return tmp;
                    }
                    if (tmp.tok) {
                        ret->child.push_back(tmp.tok);
                        tmp.tok->parent = ret;
                    }
                }
                input.rptr = begin;
                posctx = postmp;
                return ret;
            }

            PaserKind declkind() const override {
                return PaserKind::some_pattern;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct OrParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            Vec<wrap::shared_ptr<SubParser>> subparser;

            void add_parser(wrap::shared_ptr<SubParser> sub) {
                subparser.push_back(sub);
            }

            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                for (auto& p : subparser) {
                    auto ret = p->parse(input, posctx);
                    if (ret.tok) {
                        return ret;
                    }
                    if (ret.fatal) {
                        return ret;
                    }
                }
                return {};
            }

            PaserKind declkind() const override {
                return PaserKind::or_;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec>
        struct AndParser : Parser<Input, String, Kind, Vec> {
            using token_t = Token<String, Kind, Vec>;
            using result_t = ParseResult<String, Kind, Vec>;
            using SubParser = Parser<Input, String, Kind, Vec>;
            Vec<wrap::shared_ptr<SubParser>> subparser;
            String token;
            Kind kind;

            void add_parser(wrap::shared_ptr<SubParser> sub) {
                subparser.push_back(sub);
            }

            result_t parse(Sequencer<Input>& input, Pos& posctx) override {
                size_t begin = input.rptr;
                Pos postmp = posctx;
                auto ret = make_token<String, Kind, Vec>(token, kind, posctx);
                for (auto& p : subparser) {
                    auto tmp = p->parse(input, posctx);
                    if (!tmp.tok) {
                        if (tmp.fatal) {
                            return tmp;
                        }
                        if (p->none_is_not_error()) {
                            continue;
                        }
                        input.rptr = begin;
                        posctx = std::move(postmp);
                        return {};
                    }
                    ret->child.push_back(tmp.tok);
                    tmp.tok->parent = ret;
                }
                return {ret};
            }

            PaserKind declkind() const override {
                return PaserKind::and_;
            }
        };

        template <class Input, class String, class Kind, template <class...> class Vec, class String2>
        wrap::shared_ptr<RepeatParser<Input, String, Kind, Vec>> make_repeat(wrap::shared_ptr<Parser<Input, String, Kind, Vec>> sub, String2 token, Kind kind) {
            auto ret = wrap::make_shared<RepeatParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            ret->token = std::move(token);
            ret->kind = kind;
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<AllowNoneParser<Input, String, Kind, Vec>> make_allownone(wrap::shared_ptr<Parser<Input, String, Kind, Vec>> sub) {
            auto ret = wrap::make_shared<AllowNoneParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<OnlyOneParser<Input, String, Kind, Vec>> make_onlyone(Vec<wrap::shared_ptr<Parser<Input, String, Kind, Vec>>> sub) {
            auto ret = wrap::make_shared<OnlyOneParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec, class String2>
        wrap::shared_ptr<SomePatternParser<Input, String, Kind, Vec>> make_somepattern(Vec<wrap::shared_ptr<Parser<Input, String, Kind, Vec>>> sub, String2 token, Kind kind) {
            auto ret = wrap::make_shared<OnlyOneParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            ret->token = std::move(token);
            ret->kind = kind;
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec>
        wrap::shared_ptr<OrParser<Input, String, Kind, Vec>> make_or(Vec<wrap::shared_ptr<Parser<Input, String, Kind, Vec>>> sub) {
            auto ret = wrap::make_shared<OrParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            return ret;
        }

        template <class Input, class String, class Kind, template <class...> class Vec, class String2>
        wrap::shared_ptr<AndParser<Input, String, Kind, Vec>> make_and(Vec<wrap::shared_ptr<Parser<Input, String, Kind, Vec>>> sub, String2 token, Kind kind) {
            auto ret = wrap::make_shared<AndParser<Input, String, Kind, Vec>>();
            ret->subparser = std::move(sub);
            ret->token = std::move(token);
            ret->kind = kind;
            return ret;
        }
    }  // namespace parser
}  // namespace utils
