/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// io - io definition
#pragma once
#include "../doc.h"

namespace utils {
    namespace quic {
        namespace io {

            using TargetKey = uint;
            using TargetID = tsize;
            using IPAddress = byte[16];
            using Port = ushort;
            using OptionKey = tsize;

            constexpr TargetID global = 0;
            constexpr TargetID invalid = ~0;

            struct Option {
                OptionKey key;
                TargetID target;
                union {
                    void* ptr;
                    const byte* b;
                };
                union {
                    void* rp;
                    tsize rn;
                };
            };

            struct Target {
                TargetKey key;
                TargetID target;
                union {
                    struct {
                        IPAddress address;
                        Port port;
                    } ip;
                    struct {
                        const byte* address;
                        tsize len;
                    } local;
                };
            };

            enum class Status {
                ok,
                incomplete,
                failed,
                callback,
                canceled,
                timeout,
                unsupported,
                invalid_arg,
            };

            using ErrorCode = tsize;

            struct Result {
                Status status;
                ErrorCode code;
                bool nsys;
            };

            bool ok(Result s) {
                return s.status == Status::ok || s.status == Status::callback;
            }

            bool incomplete(Result s) {
                return s.status == Status::incomplete;
            }

            bool fail(Result s) {
                return s.status == Status::failed || s.status == Status::canceled ||
                       s.status == Status::timeout;
            }

            bool timeout(Result s) {
                return s.status == Status::timeout;
            }

            bool unsupported(Result s) {
                return s.status == Status::unsupported;
            }

            tsize code(Result s) {
                return s.code;
            }

            bool non_sys(Result s) {
                return s.nsys;
            }

            using Poller = void (*)(void* user, Target* from, const byte* data, tsize len);

            struct IO {
                void* C;
                Result (*option)(void* C, io::Option* opt);
                Result (*write_to)(void* C, const Target* target, const byte* data, tsize len, tsize* written);
                Result (*read_from)(void* C, const Target* target, byte* data, tsize len, tsize* read);
                TargetID (*new_target)(void* C, const Target* hint);
                Result (*select)(void* C, io::Poller callback);
                void (*free)(void* C);
            };

        }  // namespace io
    }      // namespace quic
}  // namespace utils
