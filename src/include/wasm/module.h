/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "section.h"

namespace futils::wasm {
    constexpr byte magic[] = {0, 'a', 's', 'm'};
    struct Module {
        std::uint32_t version = 0;
        std::vector<section::Section> sections;

        result<void> parse(binary::reader& r) {
            auto cmp = r.read_best(4);
            if (cmp != magic) {
                return unexpect(Error::magic_mismatch);
            }
            if (!binary::read_num(r, version, false)) {
                return unexpect(Error::short_input);
            }
            while (!r.empty()) {
                section::Section sec;
                auto res = sec.parse(r).transform([&] { return sec; }).transform(push_back_to(sections));
                if (!res) {
                    return res;
                }
            }
            return {};
        }

        result<void> render(binary::writer& w) const {
            if (!w.write(magic)) {
                return unexpect(Error::short_buffer);
            }
            if (!binary::write_num(w, std::uint32_t(1), false)) {
                return unexpect(Error::short_buffer);
            }
            for (auto& sec : sections) {
                auto res = sec.render(w);
                if (!res) {
                    return res;
                }
            }
            return {};
        }
    };
}  // namespace futils::wasm
