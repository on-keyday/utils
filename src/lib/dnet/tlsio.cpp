/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <dnet/dll/dllcpp.h>
#include <dnet/httpstring.h>
#include <dnet/tls_head.h>
#include <dnet/tls.h>

namespace utils {
    namespace dnet {
        dnet_dll_internal(size_t) tls_input(TLS& tls, String& buf, size_t* next_expect) {
            tls.clear_err();
            size_t record_count = 0;
            while (true) {
                size_t record_len = 0;
                auto should_shift = tls::record::enough_to_parse_record(buf.text(), buf.size(), &record_len);
                if (!should_shift) {
                    if (next_expect) {
                        *next_expect = record_len;
                    }
                    return record_count;
                }
                if (!tls.provide_tls_data(buf.text(), record_len)) {
                    return record_count;
                }
                buf.shift(record_len);
                record_count++;
            }
        }

        dnet_dll_internal(size_t) tls_output(TLS& tls, String& buf, String& tmpbuf) {
            tls.clear_err();
            size_t record_count = 0;
            while (true) {
                char data[1024];
                if (!tls.receive_tls_data(data, 1024)) {
                    return record_count;
                }
                tmpbuf.append(data, tls.readsize());
                size_t expect_len = 0;
                if (tls::record::enough_to_parse_record(tmpbuf.text(), tmpbuf.size(), &expect_len)) {
                    buf.append((const char*)data, expect_len);
                    tmpbuf.shift(expect_len);
                    record_count++;
                }
            }
        }
    }  // namespace dnet
}  // namespace utils
