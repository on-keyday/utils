/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/util/qpack/qpack_header.h>
#include <fnet/util/qpack/qpack.h>
#include <fnet/util/qpack/typeconfig.h>
#include <wrap/cout.h>

int main() {
    auto copy = futils::qpack::header::predefined_headers<>;
    for (auto i = 0; i < 99; i++) {
        futils::wrap::cout_wrap() << i << " " << copy.table[i].first << ": "
                                  << copy.table[i].second << "\n";
    }
    futils::wrap::cout_wrap() << "\n";

    auto print = [](auto& cp) {
        for (auto i = 0; i < cp.row_; i++) {
            for (auto j = 0; j < cp.col_; j++) {
                if (cp.table[i][j]) {
                    if (cp.table[i][j] - 1 < 10) {
                        futils::wrap::cout_wrap() << "0";
                    }
                    futils::wrap::cout_wrap() << cp.table[i][j] - 1;
                }
                else {
                    futils::wrap::cout_wrap() << "--";
                }
                futils::wrap::cout_wrap() << " ";
            }
            futils::wrap::cout_wrap() << "\n";
        }
        futils::wrap::cout_wrap() << "\n";
    };
    print(futils::qpack::header::internal::field_hash_table);
    print(futils::qpack::header::internal::header_hash_table);

    futils::qpack::Context<futils::qpack::TypeConfig<>> ctx;
    std::string buf;
    futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buf};
    ctx.enc.set_max_capacity(2000);
    ctx.update_capacity(2000);
    ctx.write_header(0, w, [&](auto&& add_entry, auto&& add_field) {
        add_field(":status", "200");
        add_field("x-object", "undefined", futils::qpack::FieldPolicy::force_literal);
        add_entry("cookie", "I want to eat cookie");
        add_field("cookie", "I want to eat cookie", futils::qpack::FieldPolicy::prior_dynamic);
        add_field("purpose", "sightseeing");
        add_field("set-cookie", "cookie is very very fun", futils::qpack::FieldPolicy::no_dynamic);
        add_field("set-cookie", "symbols: !@#$%^&*()_+{}|:\"<>?`-=[]\\;',./", futils::qpack::FieldPolicy::no_dynamic);
    });

    futils::binary::reader r{ctx.enc_send_stream.stream([&](auto& s) { return s.written(); })};
    ctx.dec.set_max_capacity(2000);
    ctx.read_encoder_stream(r);
    r.reset_buffer(w.written());
    ctx.read_header(0, r, [](auto&& field) {
        futils::wrap::cout_wrap() << field.key << ": " << field.value << "\n";
    });
}
