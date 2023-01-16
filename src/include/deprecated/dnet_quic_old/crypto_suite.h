/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// crypto_suite - crypto suite
#pragma once
#include "crypto_keys.h"
#include "crypto.h"
#include "../../tls.h"
#include "../../mem.h"
#include <deque>
#include "../../httpstring.h"
#include "../../dll/allocator.h"

namespace utils {
    namespace dnet {
        namespace quic {
            namespace crypto {
                struct HandshakeData {
                    dnet::String data;
                    size_t offset = 0;

                    bool add(const byte* p, size_t len) {
                        return data.append((const char*)p, len);
                    }

                    size_t size() {
                        return data.size();
                    }

                    size_t remain() {
                        return data.size() - offset;
                    }

                    ByteLen get(size_t l = ~0, size_t off = 0) {
                        if (data.size() - offset < off) {
                            return {};
                        }
                        if (data.size() - off < l) {
                            return {(byte*)data.text() + offset + off, data.size() - offset - off};
                        }
                        return {(byte*)data.text() + offset + off, l};
                    }

                    void consume(size_t l = ~0) {
                        if (l > data.size() - offset) {
                            l = data.size() - offset;
                        }
                        offset += l;
                    }
                };

                struct CryptoData {
                    size_t offset;
                    BoxByteLen b;
                };

                struct CryptoDataSet {
                    size_t read_offset = 0;
                    std::deque<CryptoData, glheap_allocator<CryptoData>> data;
                };

                struct Secret {
                    TLSCipher cipher;
                    BoxByteLen secret;
                    bool dropped = false;

                    bool is_set() const {
                        return dropped || secret.valid();
                    }

                    error::Error put_secret(ByteLen b) {
                        if (dropped) {
                            return error::Error("secret already dropped");
                        }
                        if (secret.valid()) {
                            return error::Error("secret alerady set");
                        }
                        secret = b;
                        if (!secret.valid()) {
                            return error::memory_exhausted;
                        }
                        return error::none;
                    }

                    void drop_secret() {
                        if (!dropped && secret.valid()) {
                            clear_memory(secret.data(), secret.len());
                            secret = {};
                            dropped = true;
                        }
                    }
                };

                struct CryptoSuite {
                    TLS tls;
                    CryptoDataSet crypto_data[4];
                    // write secret
                    Secret wsecrets[4];
                    // read secret
                    Secret rsecrets[4];
                    HandshakeData hsdata[4];
                    byte tls_alert = 0;
                    bool flush_called = false;
                    // tls.connect or tls.accept is completed
                    bool established = false;
                    bool got_peer_transport_param = false;

                    Secret& get_rsecret(EncryptionLevel level) {
                        return rsecrets[int(level)];
                    }

                    Secret& get_wsecret(EncryptionLevel level) {
                        return wsecrets[int(level)];
                    }

                    bool has_read_secret(EncryptionLevel level) const {
                        return rsecrets[int(level)].secret.valid();
                    }

                    bool has_write_secret(EncryptionLevel level) const {
                        return wsecrets[int(level)].secret.valid();
                    }
                };
            }  // namespace crypto
        }      // namespace quic
    }          // namespace dnet
}  // namespace utils
