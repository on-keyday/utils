/*
    utils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/util/qpack/qpack_header.h>
#include <fnet/util/qpack/qpack.h>
#include <fnet/util/qpack/typeconfig.h>
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

    utils::qpack::Context<utils::qpack::TypeConfig<>> ctx;
    utils::binary::expand_writer<std::string> w;
    ctx.enc.set_max_capacity(2000);
    ctx.update_capacity(2000);
    ctx.write_header(0, w, [&](auto&& add_entry, auto&& add_field) {
        add_field(":status", "200");
        add_field("x-object", "undefined", utils::qpack::FieldPolicy::force_literal);
        add_entry("cookie", "I want to eat cookie");
        add_field("cookie", "I want to eat cookie", utils::qpack::FieldPolicy::prior_dynamic);
        add_field("purpose", "sightseeing");
        add_field("set-cookie", "cookie is very very fun", utils::qpack::FieldPolicy::no_dynamic);
    });

    utils::binary::reader r{ctx.enc_stream.written()};
    ctx.dec.set_max_capacity(2000);
    ctx.read_encoder_stream(r);
    r.reset(w.written());
    ctx.read_header(0, r, [](auto&& field) {
        utils::wrap::cout_wrap() << field.key << ": " << field.value << "\n";
    });
}
