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
// #include "../endian/reader.h"
// #include "../endian/writer.h"
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
             {0, 1, 0, 1, 0, 0},
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
             {1, 1, 1, 1, 1, 1, 0},
             {1, 1, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 0},
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
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
             {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1},
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
            string_t buf;
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

        struct h2huffman_tree {
           private:
            h2huffman_tree* zero = nullptr;
            h2huffman_tree* one = nullptr;
            unsigned char c = 0;
            bool has_c = false;
            bool eos = false;
            constexpr h2huffman_tree() {}
            ~h2huffman_tree() noexcept {
                delete zero;
                delete one;
            }

            static bool append(h2huffman_tree* tree, bool eos, unsigned char c, const bitvec_t& v, bitvec_t& res, size_t idx = 0) {
                if (!tree) {
                    return false;
                }
                if (v == res) {
                    tree->c = c;
                    tree->has_c = true;
                    tree->eos = eos;
                    if (tree->zero || tree->one) {
                        throw "invalid hpack huffman";
                    }
                    return true;
                }
                res.push_back(v[idx]);
                if (v[idx]) {
                    if (!tree->one) {
                        tree->one = new h2huffman_tree();
                    }
                    return append(tree->one, eos, c, v, res, idx + 1);
                }
                else {
                    if (!tree->zero) {
                        tree->zero = new h2huffman_tree();
                    }
                    return append(tree->zero, eos, c, v, res, idx + 1);
                }
            }

            static h2huffman_tree* init_tree() {
                static h2huffman_tree _tree = {};
                for (auto i = 0; i < 256; i++) {
                    bitvec_t res;
                    append(&_tree, false, i, h2huffman[i], res);
                }
                bitvec_t res;
                append(&_tree, true, 0, h2huffman[256], res);
                return &_tree;
            }

           public:
            static const h2huffman_tree* tree() {
                static h2huffman_tree* ret = init_tree();
                return ret;
            }

            const h2huffman_tree* get(bool flag) const {
                return flag ? one : zero;
            }

            bool has_char() const {
                return has_c;
            }

            unsigned char get_char() const {
                return c;
            }

            bool is_eos() const {
                return eos;
            }
        };

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
        HpkErr decode_huffman_achar(unsigned char& c, bitvec_reader<T>& r,
                                    const h2huffman_tree* t, const h2huffman_tree*& fin,
                                    std::uint32_t& allone) {
            for (;;) {
                if (!t) {
                    return HpackError::invalid_value;
                }
                if (t->has_char()) {
                    c = t->get_char();
                    fin = t;
                    return true;
                }
                bool f = r.get();
                if (!r.incremant()) {
                    return HpackError::too_short_number;
                }
                const h2huffman_tree* next = t->get(f);
                allone = (allone && f) ? allone + 1 : 0;
                t = next;
            }
        }

        template <class T, class Out>
        HpkErr decode_huffman(Out& res, bitvec_reader<T>& r) {
            auto tree = h2huffman_tree::tree();
            while (!r.eos()) {
                unsigned char c = 0;
                const h2huffman_tree* fin = nullptr;
                std::uint32_t allone = 1;
                auto tmp = decode_huffman_achar(c, r, tree, fin, allone);
                if (!tmp) {
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
                res.push_back(c);
            }
            return true;
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
        HpkErr decode_string(String& str, io::reader& r) {
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
