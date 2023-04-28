/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net_util/qpack/qpack_header.h>
#include <wrap/cout.h>

int main() {
    auto copy = utils::qpack::header::predefined_headers<>;
    for (auto i = 0; i < 99; i++) {
        utils::wrap::cout_wrap() << i << " " << copy.table[i].first << ": "
                                 << copy.table[i].second << "\n";
    }
    utils::wrap::cout_wrap() << "\n";

    auto print = [](auto& cp) {
        for (auto i = 0; i < cp.row_; i++) {
            for (auto j = 0; j < cp.col_; j++) {
                if (cp.table[i][j]) {
                    if (cp.table[i][j] - 1 < 10) {
                        utils::wrap::cout_wrap() << "0";
                    }
                    utils::wrap::cout_wrap() << cp.table[i][j] - 1;
                }
                else {
                    utils::wrap::cout_wrap() << "--";
                }
                utils::wrap::cout_wrap() << " ";
            }
            utils::wrap::cout_wrap() << "\n";
        }
        utils::wrap::cout_wrap() << "\n";
    };
    print(utils::qpack::header::internal::field_hash_table);
    print(utils::qpack::header::internal::header_hash_table);
}