/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// address - address representation
#pragma once
#include "byte.h"
#include "boxbytelen.h"

namespace utils {
    namespace dnet {

        enum class NetAddrType : byte {
            null,
            ipv4,
            ipv6,
            unix_path,
            opaque,
        };

        namespace internal {
            constexpr bool NetAddronHeap(NetAddrType type) {
                return type != NetAddrType::ipv4 &&
                       type != NetAddrType::ipv6 &&
                       type != NetAddrType::null;
            }
        }  // namespace internal

        struct NetAddr {
           private:
            friend NetAddr make_netaddr(NetAddrType, ByteLen);
            union {
                byte bdata[16];
                BoxByteLen fdata;
            };
            NetAddrType type_ = NetAddrType::null;

            constexpr void data_copy(const byte* from, size_t len) {
                for (auto i = 0; i < len; i++) {
                    bdata[i] = from[i];
                }
            }

            void copy(const NetAddr& from) {
                if (internal::NetAddronHeap(from.type_)) {
                    fdata = from.fdata.unbox();
                    if (!fdata.valid()) {
                        type_ = NetAddrType::null;
                        return;
                    }
                }
                else {
                    data_copy(from.bdata, sizeof(bdata));
                }
                type_ = from.type_;
            }

            constexpr void move(NetAddr&& from) {
                if (internal::NetAddronHeap(from.type_)) {
                    fdata = std::move(from.fdata);
                }
                else {
                    data_copy(from.bdata, sizeof(bdata));
                }
                type_ = from.type_;
                from.type_ = NetAddrType::null;
            }

           public:
            constexpr NetAddr()
                : fdata() {}

            ~NetAddr() {
                if (internal::NetAddronHeap(type_)) {
                    fdata.~BoxByteLenBase();
                }
            }
            NetAddr(const NetAddr& from) {
                copy(from);
            }

            constexpr NetAddr(NetAddr&& from) {
                move(std::move(from));
            }

            NetAddr& operator=(const NetAddr& from) {
                if (this == &from) {
                    return *this;
                }
                this->~NetAddr();
                copy(from);
                return *this;
            }

            constexpr NetAddr& operator=(NetAddr&& from) {
                if (this == &from) {
                    return *this;
                }
                this->~NetAddr();
                move(std::move(from));
                return *this;
            }

            constexpr const byte* data() const {
                if (internal::NetAddronHeap(type_)) {
                    return fdata.data();
                }
                return const_cast<byte*>(bdata);
            }

            constexpr size_t size() const {
                if (type_ == NetAddrType::null) {
                    return 0;
                }
                if (type_ == NetAddrType::ipv4) {
                    return 4;
                }
                if (type_ == NetAddrType::ipv6) {
                    return 16;
                }
                return fdata.len();
            }

            constexpr NetAddrType type() const {
                return type_;
            }
        };

        struct NetPort {
           private:
            byte port[2]{};

           public:
            constexpr operator std::uint16_t() const {
                return ConstByteLen{port, 2}.as<std::uint16_t>();
            }
            constexpr NetPort(std::uint16_t v) {
                WPacket w{{port, 2}};
                w.as(v);
            }

            constexpr explicit NetPort(std::uint16_t v, bool big_endian) {
                WPacket w{{port, 2}};
                w.as(v, big_endian);
            }

            constexpr NetPort() = default;

            constexpr const byte* data() const {
                return port;
            }
        };

        struct NetAddrPort {
            NetAddr addr;
            NetPort port;
        };

    }  // namespace dnet
}  // namespace utils
