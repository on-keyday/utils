/*
    futils - utility library
    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once

namespace futils::strutil {
    template <class View>
    constexpr int count_indent(View view) {
        int count = 0;
        for (auto c : view) {
            if (c != ' ') {
                return count;
            }
            count++;
        }
        return -1;
    }

    template <class View>
    constexpr auto per_line(View cur, auto&& apply, auto&& add_line) {
        while (cur.size()) {
            auto target = cur.find("\n");
            if (auto t = cur.find("\r"); t < target) {
                target = t;
            }
            if (target == cur.npos) {
                target = cur.size();
            }
            auto sub = cur.substr(0, target);
            apply(sub);
            auto next = cur.substr(target);
            if (next.starts_with("\r\n")) {
                cur = next.substr(2);
                add_line();
            }
            else if (next.starts_with("\r") || next.starts_with("\n")) {
                cur = next.substr(1);
                add_line();
            }
            else {
                cur = next;
            }
        }
    }
}  // namespace futils::strutil
