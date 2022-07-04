/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// quic_var_int - quic variable integer
#pragma once
#include "doc.h"

namespace utils {
    namespace quic {
        // 16. Variable - Length Integer Encoding
        // Table 4: Summary of Integer Encodings
        namespace varint {
            enum class Error {
                none,
                invalid_argument,
                unexpected_msb,
                need_more_length,
                too_large_number,
            };

            constexpr byte msb_mask = 0b11'00'00'00;
            constexpr byte msb_len1 = 0b00'00'00'00;
            constexpr byte msb_len2 = 0b01'00'00'00;
            constexpr byte msb_len4 = 0b10'00'00'00;
            constexpr byte msb_len8 = 0b11'00'00'00;

            constexpr byte get_msb(tsize enclen) {
                switch (enclen) {
                    case 1:
                        return msb_len1;
                    case 2:
                        return msb_len2;
                    case 4:
                        return msb_len4;
                    case 8:
                        return msb_len8;
                    default:
                        return 0;
                }
            }

            constexpr tsize least_1 = 63;
            constexpr tsize least_2 = 16383;
            constexpr tsize least_4 = 1073741823;
            constexpr tsize least_8 = 4611686018427387903;

            constexpr tsize least_enclen(tsize v) {
                if (v <= least_1) {
                    return 1;
                }
                if (v <= least_2) {
                    return 2;
                }
                if (v <= least_4) {
                    return 4;
                }
                if (v <= least_8) {
                    return 8;
                }
                return 0;
            }

            constexpr bool in_range(tsize v, tsize enclen) {
                switch (enclen) {
                    case 1:
                        return v <= least_1;
                    case 2:
                        return v <= least_2;
                    case 4:
                        return v <= least_4;
                    case 8:
                        return v <= least_8;
                    default:
                        return false;
                }
            }

            inline bool netorder() {
                union {
                    uint v;
                    byte s[sizeof(uint)];
                } v{uint(1)};
                return v.s[0] == 0;
            }

            template <class T>
            union Swap {
                T t;
                byte swp[sizeof(T)];
            };

            template <class T>
            void reverse(Swap<T>& v) {
                constexpr auto szT = sizeof(T);
                static_assert(szT == 1 || szT == 2 || szT == 4 || szT == 8, "unexpected size");
                Swap<T> result;
                result.swp[0] = v.swp[szT - 1];
                if constexpr (szT >= 2) {
                    result.swp[1] = v.swp[szT - 2];
                }
                if constexpr (szT >= 4) {
                    result.swp[2] = v.swp[szT - 3];
                    result.swp[3] = v.swp[szT - 4];
                }
                if constexpr (szT >= 8) {
                    result.swp[4] = v.swp[szT - 5];
                    result.swp[5] = v.swp[szT - 6];
                    result.swp[6] = v.swp[szT - 7];
                    result.swp[7] = v.swp[szT - 8];
                }
                v.t = result.t;
            }

            template <class T>
            T endian_swap(T t) {
                Swap<T> v{t};
                reverse(v);
                return result.t;
            }

            template <class T>
            T swap_if(T t) {
                return netorder() ? t : endian_swap(t);
            }

            template <class T, class Bytes>
            Error encode_bytes(Bytes&& bytes, tsize value, tsize size, tsize* offset) {
                constexpr auto enclen = sizeof(T);
                if (offset == nullptr || enclen >= size - *offset) {
                    return Error::invalid_argument;
                }
                if (!in_range(value, sizeof(T))) {
                    return Error::too_large_number;
                }
                if constexpr (enclen == 1) {
                    bytes[*offset] = byte(value);
                    ++*offset;
                    return Error::none;
                }
                else {
                    union {
                        T t;
                        byte s[enclen];
                    } t{swap_if(T(value))};
                    for (tsize i = 0; i < enclen; i++) {
                        if (i == 0) {
                            bytes[*offset] = t.s[i] | get_msb(enclen);
                        }
                        else {
                            bytes[*offset] = t.s[i];
                        }
                        ++*offset;
                    }
                    return Error::none;
                }
            }

            template <class Bytes>
            Error encode(Bytes&& bytes, tsize value, tsize enclen, tsize size, tsize* offset) {
                static_assert(sizeof(bytes[0]) == 1, "unexpected byte size. expect 1");
                if (enclen == 0) {
                    enclen = least_enclen(enclen);
                }
                switch (enclen) {
                    case 1:
                        return encode_bytes<byte>(bytes, value, size, offset);
                    case 2:
                        return encode_bytes<ushort>(bytes, value, size, offset);
                    case 4:
                        return encode_bytes<uint>(bytes, value, size, offset);
                    case 8:
                        return encode_bytes<tsize>(bytes, value, size, offset);
                    default:
                        return Error::invalid_argument;
                }
            }

            template <class T, class Bytes>
            Error decode_bytes(Bytes&& bytes, tsize size, tsize offset, size_t* red, size_t* redsize) {
                constexpr auto szT = sizeof(T);
                *redsize = szT;
                if (offset + szT - 1 >= size) {
                    return Error::need_more_length;
                }
                Swap<T> v;
                for (tsize i = 0; i < szT; i++) {
                    if (i == 0) {
                        v.swp[i] = byte(bytes[offset + 0] & ~msb_mask);
                    }
                    else {
                        v.swp[i] = bytes[offset + i];
                    }
                }
                *red = swap_if(v.t);
                return Error::none;
            }

            template <class Bytes>
            Error decode(Bytes&& bytes, tsize* red, tsize* redsize, tsize size, tsize offset = 0) {
                static_assert(sizeof(bytes[0]) == 1, "unexpected byte size. expect 1");
                if (red == nullptr || redsize == nullptr || offset >= size) {
                    return Error::invalid_argument;
                }
                auto msbcheck = byte(bytes[offset + 0]);
                byte mask = msb_mask & msbcheck;
                switch (mask) {
                    case msb_len1: {
                        *red = msbcheck;
                        *redsize = 1;
                        return Error::none;
                    }
                    case msb_len2: {
                        return decode_bytes<ushort>(bytes, size, offset, red, redsize);
                    }
                    case msb_len4: {
                        return decode_bytes<uint>(bytes, size, offset, red, redsize);
                    }
                    case msb_len8: {
                        return decode_bytes<tsize>(bytes, size, offset, red, redsize);
                    }
                    default:
                        return Error::unexpected_msb;
                }
            }
        }  // namespace varint
    }      // namespace quic
}  // namespace utils