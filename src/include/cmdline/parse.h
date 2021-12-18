/*
    utils - utility library
    Copyright (c) 2021 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// parse - parse option
#pragma once

#include "parse_impl.h"

namespace utils {
    namespace cmdline {

        template <class String, class Char, template <class...> class Map, template <class...> class Vec>
        ParseError parse_one(int& index, int argc, Char** argv, wrap::shared_ptr<Option<String>>& opt,
                             OptionSet<String, Vec, Map>& result,
                             ParseFlag flag, String* assign) {
            Option<String>& option = *opt;
            OptValue<>& def = option.defvalue;
            OptValue<>* target = nullptr;
            if (result.find(option.mainname, target)) {
                if (any(option.flag & OptFlag::once_in_cmd)) {
                    return ParseError::not_one_opt;
                }
                if (target->type() != type<Vec<OptValue<>>>()) {
                    auto tmp = std::move(*target);
                    *target = Vec<OptValue<>>{std::move(tmp)};
                }
            }
            else {
                result.emplace(option.mainname, target);
            }
            assert(target);
            if (auto b = def.template value<bool>()) {
                return internal::parse_bool<Vec>(index, argc, argv, opt, flag, assign, b, target);
            }
            else if (auto i = def.template value<std::int64_t>()) {
                return internal::parse_int<Vec>(index, argc, argv, opt, flag, assign, i, target);
            }
            else if (auto s = def.template value<String>()) {
                return internal::parse_string<Vec>(index, argc, argv, opt, flag, assign, s, target);
            }
            else if (auto bv = def.template value<VecOption<Vec, std::uint8_t>>()) {
                return internal::parse_vec_bool(index, argc, argv, opt, flag, assign, bv, target);
            }
            else if (auto iv = def.template value<VecOption<Vec, std::int64_t>>()) {
                return internal::parse_vec_int(index, argc, argv, opt, flag, assign, iv, target);
            }
            else if (auto sv = def.template value<VecOption<Vec, String>>()) {
                return internal::parse_vec_string(index, argc, argv, opt, flag, assign, sv, target);
            }
            return ParseError::unexpected_type;
        }

        template <class String, class Char, template <class...> class Map, template <class...> class Vec>
        ParseError parse(int& index, int argc, Char** argv,
                         OptionDesc<String, Vec, Map>& desc,
                         OptionSet<String, Vec, Map>& result,
                         ParseFlag flag, Vec<String>* arg = nullptr) {
            using option_t = wrap::shared_ptr<Option<String>>;
            bool nooption = false;
            ParseError ret = ParseError::none;
            auto parse_all = [&](bool failed) {
                if (failed) {
                    if (any(flag & ParseFlag::ignore_not_found)) {
                        return true;
                    }
                    if (any(flag & ParseFlag::failure_opt_as_arg)) {
                        arg->push_back(utf::convert<String>(argv[index]));
                        return true;
                    }
                }
                else {
                    if (any(flag & ParseFlag::parse_all) && arg) {
                        arg->push_back(utf::convert<String>(argv[index]));
                        return true;
                    }
                }
                return false;
            };
            bool has_assign = any(flag & ParseFlag::allow_assign);
            auto found_option = [&](int offset) {
                auto optname = argv[index] + offset;
                option_t opt;
                String name, value, *ptr = nullptr;
                if (has_assign) {
                    auto seq = utils::make_ref_seq(optname);
                    if (helper::read_until(name, seq, "=")) {
                        if (seq.eos()) {
                            return ParseError::wrong_assign;
                        }
                        value = utf::convert<String>(argv[index] + seq.rptr + offset);
                        ptr = &value;
                    }
                    desc.find(name, opt);
                }
                else {
                    desc.find(optname, opt);
                }
                if (opt) {
                    return parse_one(index, argc, argv, opt, result, flag, ptr);
                }
                return ParseError::not_found;
            };
            for (; index < argc; index++) {
                if (nooption || argv[index][0] != '-') {
                    if (parse_all(false)) {
                        continue;
                    }
                    return ParseError::suspend_parse;
                }
                if (helper::equal(argv[index], "--")) {
                    if (any(flag & ParseFlag::ignore_after_two_prefix)) {
                        nooption = true;
                        continue;
                    }
                }
                else if (helper::starts_with(argv[index], "--")) {
                    if (any(flag & ParseFlag::two_prefix_longname)) {
                        if (auto e = found_option(2); e == ParseError::none) {
                            continue;
                        }
                        else if (e != ParseError::not_found) {
                            return e;
                        }
                    }
                    if (parse_all(true)) {
                        continue;
                    }
                    return ParseError::not_found;
                }
                else {
                    if (any(flag & ParseFlag::one_prefix_longname)) {
                        if (auto e = found_option(1); e == ParseError::none) {
                            continue;
                        }
                        else if (e != ParseError::not_found) {
                            return e;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                    if (any(flag & ParseFlag::adjacent_value)) {
                        option_t opt;
                        auto view = helper::CharView<Char>(argv[index][1]);
                        desc.find(view.c_str(), opt);
                        if (opt) {
                            String* ptr = nullptr;
                            String value;
                            if (argv[index][2]) {
                                int offset = 2;
                                if (has_assign && argv[index][2] == '=') {
                                    offset = 3;
                                    if (!argv[index][3]) {
                                        return ParseError::wrong_assign;
                                    }
                                }
                                value = utf::convert<String>(argv[index] + offset);
                                ptr = &value;
                            }
                            if (auto e = parse_one(index, argc, argv, opt, result, flag, ptr); e != ParseError::none) {
                                return e;
                            }
                            continue;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                    auto current = index;
                    for (int i = 1; argv[current][i]; i++) {
                        option_t opt;
                        desc.find(helper::CharView<Char>(argv[current][1]).c_str(), opt);
                        if (opt) {
                            if (auto e = parse_one(index, argc, argv, opt, result, flag, static_cast<String*>(nullptr)); e != ParseError::none) {
                                return e;
                            }
                            continue;
                        }
                        if (parse_all(true)) {
                            continue;
                        }
                        return ParseError::not_found;
                    }
                }
            }
            return ParseError::none;
        }

    }  // namespace cmdline
}  // namespace utils
