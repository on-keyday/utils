/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// hpack_huffman - hpack huffman codec
#pragma once
#include <utility>
#include <array>
#include "../wrap/light/enum.h"
#include "../helper/equal.h"
#include "../io/number.h"
#include "../io/expandable_writer.h"

namespace utils {
    namespace hpack {
        enum class HpackError {
            none,
            too_large_number,
            too_short_number,
            internal,
            invalid_mask,
            invalid_value,
            not_exists,
            table_update,
        };

        using HpkErr = wrap::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

        using sheader_t = std::pair<const char*, const char*>;

        constexpr size_t predefined_header_size = 62;

        constexpr std::array<sheader_t, predefined_header_size> predefined_header = {
            sheader_t("INVALIDINDEX", "INVALIDINDEX"),
            sheader_t(":authority", ""),
            sheader_t(":method", "GET"),
            sheader_t(":method", "POST"),
            sheader_t(":path", "/"),
            sheader_t(":path", "/index.html"),
            sheader_t(":scheme", "http"),
            sheader_t(":scheme", "https"),
            sheader_t(":status", "200"),
            sheader_t(":status", "204"),
            sheader_t(":status", "206"),
            sheader_t(":status", "304"),
            sheader_t(":status", "400"),
            sheader_t(":status", "404"),
            sheader_t(":status", "500"),
            sheader_t("accept-charset", ""),
            sheader_t("accept-encoding", "gzip, deflate"),
            sheader_t("accept-language", ""),
            sheader_t("accept-ranges", ""),
            sheader_t("accept", ""),
            sheader_t("access-control-allow-origin", ""),
            sheader_t("age", ""),
            sheader_t("allow", ""),
            sheader_t("authorization", ""),
            sheader_t("cache-control", ""),
            sheader_t("content-disposition", ""),
            sheader_t("content-encoding", ""),
            sheader_t("content-language", ""),
            sheader_t("content-length", ""),
            sheader_t("content-location", ""),
            sheader_t("content-range", ""),
            sheader_t("content-type", ""),
            sheader_t("cookie", ""),
            sheader_t("date", ""),
            sheader_t("etag", ""),
            sheader_t("expect", ""),
            sheader_t("expires", ""),
            sheader_t("from", ""),
            sheader_t("host", ""),
            sheader_t("if-match", ""),
            sheader_t("if-modified-since", ""),
            sheader_t("if-none-match", ""),
            sheader_t("if-range", ""),
            sheader_t("if-unmodified-since", ""),
            sheader_t("last-modified", ""),
            sheader_t("link", ""),
            sheader_t("location", ""),
            sheader_t("max-forwards", ""),
            sheader_t("proxy-authenticate", ""),
            sheader_t("proxy-authorization", ""),
            sheader_t("range", ""),
            sheader_t("referer", ""),
            sheader_t("refresh", ""),
            sheader_t("retry-after", ""),
            sheader_t("server", ""),
            sheader_t("set-cookie", ""),
            sheader_t("strict-transport-security", ""),
            sheader_t("transfer-encoding", ""),
            sheader_t("user-agent", ""),
            sheader_t("vary", ""),
            sheader_t("via", ""),
            sheader_t("www-authenticate", "")};

        struct bitvec_t {
           private:
            std::uint32_t bits = 0;
            std::uint32_t size_ = 0;

            constexpr int shift(int i) const {
                return (sizeof(bits) * 8 - 1 - i);
            }

           public:
            constexpr bitvec_t() {}
            constexpr bitvec_t(std::initializer_list<int> init) noexcept {
                size_ = init.size();
                for (auto i = 0; i < size_; i++) {
                    std::uint32_t bit = init.begin()[i] ? 1 : 0;
                    bits |= bit << shift(i);
                }
            }

            constexpr bool operator[](size_t idx) const {
                if (idx >= size_) {
                    throw "out of range";
                }
                return (bool)(bits & (1 << shift(idx)));
            }

            constexpr size_t size() const {
                return size_;
            }

            template <class T>
            constexpr bool operator==(T& in) const {
                if (size() != in.size()) return false;
                for (auto i = 0; i < in.size(); i++) {
                    if ((*this)[i] != in[i]) {
                        return false;
                    }
                }
                return true;
            }

            constexpr bool push_back(bool in) {
                if (size_ >= sizeof(bits) * 8) {
                    throw "too large vector";
                }
                std::uint32_t bit = in ? 1 : 0;
                bits |= (bit << shift(size_));
                size_++;
                return true;
            }
        };

        constexpr std::array<const bitvec_t, 257> h2huffman{
            {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
             {0, 1, 0, 1, 0, 0},  // 0x32
             {1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1},
             {0, 1, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 0, 1, 0},
             {0, 1, 0, 1, 1, 0},
             {0, 1, 0, 1, 1, 1},
             {0, 1, 1, 0, 0, 0},
             {0, 0, 0, 0, 0},
             {0, 0, 0, 0, 1},
             {0, 0, 0, 1, 0},
             {0, 1, 1, 0, 0, 1},
             {0, 1, 1, 0, 1, 0},
             {0, 1, 1, 0, 1, 1},
             {0, 1, 1, 1, 0, 0},
             {0, 1, 1, 1, 0, 1},
             {0, 1, 1, 1, 1, 0},
             {0, 1, 1, 1, 1, 1},
             {1, 0, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0},
             {1, 0, 0, 0, 0, 1},
             {1, 0, 1, 1, 1, 0, 1},
             {1, 0, 1, 1, 1, 1, 0},
             {1, 0, 1, 1, 1, 1, 1},
             {1, 1, 0, 0, 0, 0, 0},
             {1, 1, 0, 0, 0, 0, 1},
             {1, 1, 0, 0, 0, 1, 0},
             {1, 1, 0, 0, 0, 1, 1},
             {1, 1, 0, 0, 1, 0, 0},
             {1, 1, 0, 0, 1, 0, 1},
             {1, 1, 0, 0, 1, 1, 0},
             {1, 1, 0, 0, 1, 1, 1},
             {1, 1, 0, 1, 0, 0, 0},
             {1, 1, 0, 1, 0, 0, 1},
             {1, 1, 0, 1, 0, 1, 0},
             {1, 1, 0, 1, 0, 1, 1},
             {1, 1, 0, 1, 1, 0, 0},
             {1, 1, 0, 1, 1, 0, 1},
             {1, 1, 0, 1, 1, 1, 0},
             {1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 0, 0, 0, 1},
             {1, 1, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
             {0, 0, 0, 1, 1},
             {1, 0, 0, 0, 1, 1},
             {0, 0, 1, 0, 0},
             {1, 0, 0, 1, 0, 0},
             {0, 0, 1, 0, 1},
             {1, 0, 0, 1, 0, 1},
             {1, 0, 0, 1, 1, 0},
             {1, 0, 0, 1, 1, 1},
             {0, 0, 1, 1, 0},
             {1, 1, 1, 0, 1, 0, 0},
             {1, 1, 1, 0, 1, 0, 1},
             {1, 0, 1, 0, 0, 0},
             {1, 0, 1, 0, 0, 1},
             {1, 0, 1, 0, 1, 0},
             {0, 0, 1, 1, 1},
             {1, 0, 1, 0, 1, 1},
             {1, 1, 1, 0, 1, 1, 0},
             {1, 0, 1, 1, 0, 0},
             {0, 1, 0, 0, 0},
             {0, 1, 0, 0, 1},
             {1, 0, 1, 1, 0, 1},
             {1, 1, 1, 0, 1, 1, 1},
             {1, 1, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},                                               // 0xfe = 126
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},  // 127
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},                          // 128
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0},                    // 129
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},                          // 130
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},                          // 131
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1},                    // 132
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0},                    // 133
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1},                    // 134
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},                 // 135
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0},                    // 136
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},                 // 137
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},                 // 138
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},                 // 139
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},                 // 140
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},                 // 141
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},              // 142
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},                 // 143
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},              // 144
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},              // 145
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1},                    // 146
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},                 // 147
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},              // 148
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},                 // 149
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},                 // 150
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},                 // 151
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},                 // 152
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},                       // 153
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0},                    // 154
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},                 // 155
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},                    // 156
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},                 // 157
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},                 // 158
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},              // 159
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},                    // 160
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},                       // 161
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},                          // 162
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},                    // 163
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},                    // 164
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},                 // 165
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},                 // 166
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},                       // 167
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},                 // 168
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},                    // 169
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},                    // 170
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},              // 171
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},                       // 172
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},                    // 173
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},                 // 174
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},                 // 175
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},                       // 176
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},                       // 177
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}};

        template <class String>
        struct bitvec_writer {
            using string_t = String;

           private:
            string_t buf{};
            size_t idx = 0;
            size_t pos = 0;

           public:
            constexpr bitvec_writer() {
                buf.push_back(0);
            }

            constexpr void push_back(bool b) {
                if (pos == 8) {
                    buf.push_back(0);
                    idx++;
                    pos = 0;
                }
                unsigned char v = b ? 1 : 0;
                buf[idx] |= (v << (7 - pos));
                pos++;
            }

            template <class T>
            constexpr void append(const T& a) {
                for (auto i = 0; i < a.size(); i++) {
                    push_back(a[i]);
                }
            }

            constexpr size_t size() const {
                return idx * 8 + pos;
            }

            constexpr string_t& data() {
                return buf;
            }
        };

        template <class String>
        struct bitvec_reader {
           private:
            size_t pos = 0;
            size_t idx = 0;
            String& str;

           public:
            constexpr bitvec_reader(String& in)
                : str(in) {}

            constexpr bool get() const {
                if (idx == str.size()) return true;
                return (bool)(((unsigned char)str[idx]) & (1 << (7 - pos)));
            }

            constexpr bool incremant() {
                if (idx == str.size()) return false;
                pos++;
                if (pos == 8) {
                    idx++;
                    pos = 0;
                }
                return true;
            }

            constexpr bool decrement() {
                if (pos == 0) {
                    if (idx == 0) {
                        return false;
                    }
                    idx--;
                    pos = 8;
                }
                pos--;
                return true;
            }

            constexpr bool eos() const {
                return idx == str.size();
            }
        };

        struct huffman_tree {
           private:
            std::uint16_t zero = 0;
            std::uint16_t one = 0;
            friend struct huffman_tree_table_t;

            // hpack huffman_tree_table_t index is smaller than 0x1ff = 511
            static constexpr std::uint16_t flag_eos = 0x0400;
            static constexpr std::uint16_t flag_char = 0x0800;

            constexpr void set_char(byte c) {
                zero |= flag_char;
                one = c;
            }

            constexpr void set_eos() {
                zero |= flag_eos;
            }

           public:
            constexpr bool has_char() const {
                return zero & flag_char;
            }

            constexpr bool is_eos() const {
                return zero & flag_eos;
            }

            constexpr byte get_char() const {
                return one;
            }
        };

        struct huffman_tree_table_t {
           private:
            constexpr static size_t table_size = 513;
            huffman_tree table[table_size]{};

            constexpr void check_throw(std::uint16_t one, std::uint16_t zero) {
                if (one || zero) {
                    throw "invalid";
                }
            }

            constexpr void append(std::uint16_t& table_index, huffman_tree* tree, bool eos, byte c, const bitvec_t& v, bitvec_t& res, size_t idx = 0) {
                if (!tree) {
                    throw "unexpected";
                }
                if (v == res) {
                    check_throw(tree->one, tree->zero);
                    tree->set_char(c);
                    if (eos) {
                        tree->set_eos();
                    }
                    return;
                }
                res.push_back(v[idx]);
                if (v[idx]) {
                    if (tree->one == 0) {
                        tree->one = table_index;
                        table_index++;
                    }
                    return append(table_index, &table[tree->one], eos, c, v, res, idx + 1);
                }
                else {
                    if (tree->zero == 0) {
                        tree->zero = table_index;
                        table_index++;
                    }
                    return append(table_index, &table[tree->zero], eos, c, v, res, idx + 1);
                }
            }

            constexpr void init() {
                huffman_tree& root = table[0];
                std::uint16_t table_index = 1;
                for (auto i = 32; i < 127; i++) {
                    bitvec_t res;
                    append(table_index, &root, false, i, h2huffman[i], res);
                }
                for (auto i = 0; i < 32; i++) {
                    bitvec_t res;
                    append(table_index, &root, false, i, h2huffman[i], res);
                }
                for (auto i = 127; i < 256; i++) {
                    bitvec_t res;
                    append(table_index, &root, false, i, h2huffman[i], res);
                }
                bitvec_t res;
                append(table_index, &root, true, 0, h2huffman[256], res);
            }

           public:
            constexpr huffman_tree_table_t() {
                init();
            }

            constexpr const huffman_tree* root() const {
                return &table[0];
            }

            constexpr const huffman_tree* next(const huffman_tree* current, bool bit) const {
                if (!current) {
                    return nullptr;
                }
                if (current->has_char() || current->is_eos()) {
                    return nullptr;
                }
                std::uint16_t next_index = bit ? current->one : current->zero;
                if (next_index == 0) {
                    return nullptr;
                }
                return &table[next_index];
            }
        };

        constexpr auto huffman_tree_table = huffman_tree_table_t{};

        template <class String>
        constexpr size_t gethuffmanlen(const String& str) {
            size_t ret = 0;
            for (auto& c : str) {
                ret += h2huffman[(unsigned char)c].size();
            }
            return (ret + 7) / 8;
        }

        template <class T, class String>
        constexpr void encode_huffman(bitvec_writer<T>& vec, const String& in) {
            for (auto c : in) {
                vec.append(h2huffman[(unsigned char)c]);
            }
            while (vec.size() % 8) {
                vec.push_back(true);
            }
        }

        template <class T>
        constexpr HpkErr decode_huffman_achar(bitvec_reader<T>& r,
                                              const huffman_tree*& fin,
                                              std::uint32_t& allone) {
            const huffman_tree* t = huffman_tree_table.root();
            for (;;) {
                if (!t) {
                    return HpackError::invalid_value;
                }
                if (t->has_char()) {
                    fin = t;
                    return HpackError::none;
                }
                bool f = r.get();
                if (!r.incremant()) {
                    return HpackError::too_short_number;
                }
                allone = (allone && f) ? allone + 1 : 0;
                t = huffman_tree_table.next(t, f);
            }
        }

        template <class T, class Out>
        constexpr HpkErr decode_huffman(Out& res, bitvec_reader<T>& r) {
            while (!r.eos()) {
                const huffman_tree* fin = nullptr;
                std::uint32_t allone = 1;
                auto tmp = decode_huffman_achar(r, fin, allone);
                if (tmp != HpackError::none) {
                    if (tmp == HpackError::too_short_number) {
                        if (!r.eos() && allone - 1 > 7) {
                            return HpackError::too_large_number;
                        }
                        break;
                    }
                    return tmp;
                }
                if (fin->is_eos()) {
                    return HpackError::invalid_value;
                }
                res.push_back(fin->get_char());
            }
            return HpackError::none;
        }

        template <std::uint32_t n>
        constexpr HpkErr decode_integer(io::reader& r, size_t& size, std::uint8_t& firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr unsigned char msk = 0xff >> (8 - n);
            std::uint8_t tmp = 0;
            if (!r.read(view::wvec(&tmp, 1))) {
                return HpackError::internal;
            }
            firstmask = tmp & ~msk;
            tmp &= msk;
            size = tmp;
            if (tmp < msk) {
                return true;
            }
            size_t m = 0;
            constexpr auto pow = [](size_t x) -> size_t {
                return (size_t(0x1) << 63) >> ((sizeof(size_t) * 8 - 1) - x);
            };
            do {
                if (!r.read(view::wvec(&tmp, 1))) {
                    return HpackError::internal;
                }
                size += (tmp & 0x7f) * pow(m);
                m += 7;
                if (m > (sizeof(size_t) * 8 - 1)) {
                    return HpackError::too_large_number;
                }
            } while (tmp & 0x80);
            return true;
        }

        template <size_t n, class T>
        constexpr HpkErr encode_integer(io::expand_writer<T>& w, size_t size, std::uint8_t firstmask) {
            static_assert(n > 0 && n <= 8, "invalid range");
            constexpr std::uint8_t msk = static_cast<std::uint8_t>(~0) >> (8 - n);
            if (firstmask & msk) {
                return HpackError::invalid_mask;
            }
            if (size < static_cast<size_t>(msk)) {
                w.write(byte(firstmask | size), 1);
            }
            else {
                w.write(byte(firstmask | msk), 1);
                size -= msk;
                while (size > 0x7f) {
                    w.write(byte((size % 0x80) | 0x80), 1);
                    size /= 0x80;
                }
                w.write(byte(size), 1);
            }
            return true;
        }

        template <class T, class String>
        constexpr void encode_string(io::expand_writer<T>& w, const String& value) {
            if (value.size() > gethuffmanlen(value)) {
                bitvec_writer<String> vec;
                encode_huffman(vec, value);
                encode_integer<7>(w, vec.data().size(), 0x80);
                w.write(vec.data());
            }
            else {
                encode_integer<7>(w, value.size(), 0);
                w.write(value);
            }
        }

        template <class String>
        constexpr HpkErr decode_string(String& str, io::reader& r) {
            size_t size = 0;
            byte mask = 0;
            auto err = decode_integer<7>(r, size, mask);
            if (err != HpackError::none) {
                return err;
            }
            auto [d, ok] = r.read(size);
            if (!ok) {
                return HpackError::internal;
            }
            if (mask & 0x80) {
                bitvec_reader r(d);
                return decode_huffman(str, r);
            }
            else {
                for (auto c : d) {
                    str.push_back(c);
                }
            }
            return true;
        }

    }  // namespace hpack
}  // namespace utils
