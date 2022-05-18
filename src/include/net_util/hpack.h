/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


// hpack - http2 hpack
// almost copied from socklib
#pragma once

#include <utility>
#include <array>
#include "../wrap/light/enum.h"
#include "../endian/reader.h"
#include "../endian/writer.h"
#include "../helper/equal.h"

namespace utils {
    namespace net {
        namespace hpack {
            enum class HpackError {
                none,
                too_large_number,
                too_short_number,
                internal,
                invalid_mask,
                invalid_value,
                not_exists,
            };

            using HpkErr = wrap::EnumWrap<HpackError, HpackError::none, HpackError::internal>;

            namespace internal {

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
                    bitvec_writer() {
                        buf.push_back(0);
                    }

                    void push_back(bool b) {
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
                    void append(const T& a) {
                        for (auto i = 0; i < a.size(); i++) {
                            push_back(a[i]);
                        }
                    }

                    size_t size() const {
                        return idx * 8 + pos;
                    }

                    string_t& data() {
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
                    bitvec_reader(String& in)
                        : str(in) {}

                    bool get() const {
                        if (idx == str.size()) return true;
                        return (bool)(((unsigned char)str[idx]) & (1 << (7 - pos)));
                    }

                    bool incremant() {
                        if (idx == str.size()) return false;
                        pos++;
                        if (pos == 8) {
                            idx++;
                            pos = 0;
                        }
                        return true;
                    }

                    bool decrement() {
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

                    bool eos() const {
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
                struct HuffmanCoder {
                    using string_t = String;
                    using writer_t = bitvec_writer<String>;
                    using reader_t = bitvec_reader<String>;

                    constexpr static size_t gethuffmanlen(const string_t& str) {
                        size_t ret = 0;
                        for (auto& c : str) {
                            ret += h2huffman[(unsigned char)c].size();
                        }
                        return (ret + 7) / 8;
                    }

                    static string_t encode(const string_t& in) {
                        writer_t vec;
                        for (auto c : in) {
                            vec.append(h2huffman[(unsigned char)c]);
                        }
                        while (vec.size() % 8) {
                            vec.push_back(true);
                        }
                        return vec.data();
                    }

                   private:
                    static HpkErr decode_achar(unsigned char& c, reader_t& r, const h2huffman_tree* t, const h2huffman_tree*& fin, std::uint32_t& allone) {
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

                   public:
                    static HpkErr decode(string_t& res, string_t& src) {
                        reader_t r(src);
                        auto tree = h2huffman_tree::tree();
                        while (!r.eos()) {
                            unsigned char c = 0;
                            const h2huffman_tree* fin = nullptr;
                            std::uint32_t allone = 1;
                            auto tmp = decode_achar(c, r, tree, fin, allone);
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
                };
#define TRY(...) \
    if (auto e = (__VA_ARGS__); !e) return e
                template <class String>
                struct IntegerCoder {
                    using string_t = String;

                    template <std::uint32_t n, class T>
                    static HpkErr decode(endian::Reader<T>& se, size_t& sz, std::uint8_t& firstmask) {
                        static_assert(n > 0 && n <= 8, "invalid range");
                        constexpr unsigned char msk = static_cast<std::uint8_t>(~0) >> (8 - n);
                        std::uint8_t tmp = 0;

                        TRY((bool)se.read(tmp));
                        firstmask = tmp & ~msk;
                        tmp &= msk;
                        sz = tmp;
                        if (tmp < msk) {
                            return true;
                        }
                        size_t m = 0;
                        constexpr auto pow = [](size_t x) -> size_t {
                            return (size_t(0x1) << 63) >> ((sizeof(size_t) * 8 - 1) - x);
                        };
                        do {
                            TRY((bool)se.read(tmp));
                            sz += (tmp & 0x7f) * pow(m);
                            m += 7;
                            if (m > (sizeof(size_t) * 8 - 1)) {
                                return HpackError::too_large_number;
                            }
                        } while (tmp & 0x80);
                        return true;
                    }

                    template <std::uint32_t n, class T>
                    static HpkErr encode(endian::Writer<T>& se, size_t sz, std::uint8_t firstmask) {
                        static_assert(n > 0 && n <= 8, "invalid range");
                        constexpr std::uint8_t msk = static_cast<std::uint8_t>(~0) >> (8 - n);
                        if (firstmask & msk) {
                            return HpackError::invalid_mask;
                        }
                        if (sz < static_cast<size_t>(msk)) {
                            se.write(std::uint8_t(firstmask | sz));
                        }
                        else {
                            se.write(std::uint8_t(firstmask | msk));
                            sz -= msk;
                            while (sz > 0x7f) {
                                se.write(std::uint8_t((sz % 0x80) | 0x80));
                                sz /= 0x80;
                            }
                            se.write(std::uint8_t(sz));
                        }
                        return true;
                    }
                };

                template <class String>
                struct HpackStringCoder {
                    using string_t = String;
                    template <class In>
                    static void encode(endian::Writer<In>& se, const string_t& value) {
                        if (value.size() > HuffmanCoder<String>::gethuffmanlen(value)) {
                            string_t enc = HuffmanCoder<String>::encode(value);
                            IntegerCoder<In>::template encode<7>(se, enc.size(), 0x80);
                            se.write_seq(enc);
                        }
                        else {
                            IntegerCoder<In>::template encode<7>(se, value.size(), 0);
                            se.write_seq(value);
                        }
                    }

                    template <class In>
                    static HpkErr decode(string_t& str, endian::Reader<In>& se) {
                        size_t sz = 0;
                        unsigned char mask = 0;
                        TRY(IntegerCoder<In>::template decode<7>(se, sz, mask));
                        TRY(se.template read_seq<std::uint8_t>(str, sz));
                        if (mask & 0x80) {
                            string_t decoded;
                            TRY(HuffmanCoder<String>::decode(decoded, str));
                            str = std::move(decoded);
                        }
                        return true;
                    }
                };

                template <class String, class Table, class Header>
                struct Hpack {
                    using string_coder = HpackStringCoder<String>;
                    using integer_coder = IntegerCoder<String>;
                    using string_t = String;
                    using table_t = Table;
                    using header_t = Header;

                   private:
                    template <class F>
                    static bool get_idx(F&& f, size_t& idx, table_t& dymap) {
                        if (auto found = std::find_if(predefined_header.begin() + 1, predefined_header.end(), [&](auto& c) {
                                return f(get<0>(c), get<1>(c));
                            });
                            found != predefined_header.end()) {
                            idx = std::distance(predefined_header.begin(), found);
                        }
                        else if (auto found = std::find_if(dymap.begin(), dymap.end(), [&](auto& c) {
                                     return f(get<0>(c), get<1>(c));
                                 });
                                 found != dymap.end()) {
                            idx = std::distance(dymap.begin(), found) + predefined_header_size;
                        }
                        else {
                            return false;
                        }
                        return true;
                    }

                    static size_t calc_table_size(table_t& table) {
                        size_t size = 32;
                        for (auto& e : table) {
                            size += e.first.size();
                            size += e.second.size();
                        }
                        return size;
                    }

                   public:
                    static HpkErr encode(bool adddy, header_t& src, string_t& dst,
                                         table_t& dymap, std::uint32_t maxtablesize) {
                        endian::Writer<string_t&> se(dst);
                        for (auto&& h : src) {
                            size_t idx = 0;
                            if (get_idx(
                                    [&](const auto& k, const auto& v) {
                                        return helper::default_equal(k, get<0>(h)) && helper::default_equal(v, get<1>(h));
                                    },
                                    idx, dymap)) {
                                TRY(integer_coder::template encode<7>(se, idx, 0x80));
                            }
                            else {
                                if (get_idx(
                                        [&](const auto& k, const auto&) {
                                            return helper::default_equal(k, get<0>(h));
                                        },
                                        idx, dymap)) {
                                    if (adddy) {
                                        TRY(integer_coder::template encode<6>(se, idx, 0x40));
                                    }
                                    else {
                                        TRY(integer_coder::template encode<4>(se, idx, 0));
                                    }
                                }
                                else {
                                    se.write(std::uint8_t(0));
                                    string_coder::encode(se, h.first);
                                }
                                string_coder::encode(se, h.second);
                                if (adddy) {
                                    dymap.push_front({h.first, h.second});
                                    size_t tablesize = calc_table_size(dymap);
                                    while (tablesize > maxtablesize) {
                                        if (!dymap.size()) return false;
                                        tablesize -= dymap.back().first.size();
                                        tablesize -= dymap.back().second.size();
                                        dymap.pop_back();
                                    }
                                }
                            }
                        }
                        return true;
                    }

                    static HpkErr decode(header_t& res, string_t& src,
                                         table_t& dymap, std::uint32_t& maxtablesize) {
                        auto update_dymap = [&] {
                            size_t tablesize = calc_table_size(dymap);
                            while (tablesize > maxtablesize) {
                                if (tablesize == 32) break;  // TODO: check what written in RFC means
                                if (!dymap.size()) {
                                    return false;
                                }
                                tablesize -= get<0>(dymap.back()).size();
                                tablesize -= get<1>(dymap.back()).size();
                                dymap.pop_back();
                            }
                            return true;
                        };
                        endian::Reader<string_t&> se(src);
                        while (!se.seq.eos()) {
                            unsigned char tmp = se.seq.current();
                            string_t key, value;
                            auto read_two_literal = [&]() -> HpkErr {
                                TRY(string_coder::decode(key, se));
                                TRY(string_coder::decode(value, se));
                                res.emplace(key, value);
                                return true;
                            };
                            auto read_idx_and_literal = [&](size_t idx) -> HpkErr {
                                if (idx < predefined_header_size) {
                                    key = predefined_header[idx].first;
                                }
                                else {
                                    if (dymap.size() <= idx - predefined_header_size) {
                                        return HpackError::not_exists;
                                    }
                                    key = dymap[idx - predefined_header_size].first;
                                }
                                TRY(string_coder::decode(value, se));
                                res.emplace(key, value);
                                return true;
                            };
                            if (tmp & 0x80) {
                                size_t idx = 0;
                                TRY(integer_coder::template decode<7>(se, idx, tmp));
                                if (idx == 0) {
                                    return HpackError::invalid_value;
                                }
                                if (idx < predefined_header_size) {
                                    if (!predefined_header[idx].second[0]) {
                                        return HpackError::not_exists;
                                    }
                                    res.emplace(predefined_header[idx].first, predefined_header[idx].second);
                                }
                                else {
                                    if (dymap.size() <= idx - predefined_header_size) {
                                        return HpackError::not_exists;
                                    }
                                    res.emplace(dymap[idx - predefined_header_size].first, dymap[idx - predefined_header_size].second);
                                }
                            }
                            else if (tmp & 0x40) {
                                size_t sz = 0;

                                TRY(integer_coder::template decode<6>(se, sz, tmp));

                                if (sz == 0) {
                                    TRY(read_two_literal());
                                }
                                else {
                                    TRY(read_idx_and_literal(sz));
                                }
                                dymap.push_front({key, value});
                                TRY(update_dymap());
                            }
                            else if (tmp & 0x20) {  // dynamic table size change
                                // unimplemented
                                size_t sz = 0;
                                TRY(integer_coder::template decode<5>(se, sz, tmp));
                                if (maxtablesize > 0x80000000) {
                                    return HpackError::too_large_number;
                                }
                                maxtablesize = (std::int32_t)sz;
                                TRY(update_dymap());
                            }
                            else {
                                size_t sz = 0;
                                TRY(integer_coder::template decode<4>(se, sz, tmp));
                                if (sz == 0) {
                                    TRY(read_two_literal());
                                }
                                else {
                                    TRY(read_idx_and_literal(sz));
                                }
                            }
                        }
                        return true;
                    }
                };
#undef TRY
            }  // namespace internal

            template <class Table, class Header, class String>
            HpkErr encode(String& str, Table& table, Header& header, std::uint32_t maxtablesize, bool adddymap = false) {
                return internal::Hpack<String, Table, Header>::encode(adddymap, header, str, table, maxtablesize);
            }

            template <class Table, class Header, class String>
            HpkErr decode(String& str, Table& table, Header& header, std::uint32_t& maxtablesize) {
                return internal::Hpack<String, Table, Header>::decode(header, str, table, maxtablesize);
            }
        }  // namespace hpack

    }  // namespace net
}  // namespace utils
