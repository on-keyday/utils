/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#pragma once
#include "encoder.h"
#include "decoder.h"
#include "field_line.h"

namespace utils {
    namespace qpack {
        template <class TypeConfig>
        struct Context {
            fields::FieldEncodeContext<TypeConfig> enc;
            fields::FieldDecodeContext<TypeConfig> dec;
            io::expand_writer<typename TypeConfig::string> enc_stream;

            template <class T, class H>
            QpackError write_header(StreamID id, io::expand_writer<T>& w, H&& h, fields::FieldMode mode) {
                typename TypeConfig::string tmp;
                fields::SectionPrefix prefix;
                prefix.base = enc.get_inserted();
                for (auto kv : h) {
                    auto&& [k, v] = kv;
                    auto err = fields::render_field_line(id, enc, prefix, w, Field{k, v}, mode);
                    if (err != QpackError::none) {
                        return err;
                    }
                }
            }
        };
    }  // namespace qpack
}  // namespace utils
