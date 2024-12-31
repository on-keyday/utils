/*
    futils - utility library
    Copyright (c) 2021-2025 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <fnet/util/hpack/hpack_encode.h>
#include <fnet/util/hpack/hpack.h>
#include <string>
#include <fnet/quic/crypto/keys.h>
#include <number/array.h>
#include <view/expand_iovec.h>
#include <memory>
#include <map>
#include <deque>
#include <wrap/cout.h>
using string = futils::number::Array<char, 100>;
using rvec = futils::view::basic_rvec<char>;

constexpr bool encode_decode(const char* base) {
    auto in = rvec(base);
    string buf;
    buf.resize(in.size() * 2);
    futils::binary::basic_bit_writer<char> w{buf};
    futils::hpack::encode_huffman(w, in);
    auto encoded = w.get_base().written();
    futils::binary::basic_bit_reader<char> d{encoded};
    string val;
    return futils::hpack::decode_huffman(val, d) && val == in;
}

template <size_t s>
constexpr bool decode(futils::fnet::quic::crypto::KeyMaterial<s> data, const char* expect) {
    auto v = futils::view::wvec(data.material);
    futils::binary::bit_reader d{v};
    string val;
    return futils::hpack::decode_huffman(val, d) && val == rvec(expect);
}

void check_string_encoding() {
    static_assert(encode_decode("object identifier z"));
    static_assert(encode_decode("\x32\x44\xff"));
    static_assert(encode_decode("custom-key"));
    static_assert(encode_decode("custom-value"));
    bool d = encode_decode("object identifierz");
    bool d2 = encode_decode("custom-key");
    static_assert(decode(futils::fnet::quic::crypto::make_material_from_bintext("d07abe941054d444a8200595040b8166e082a62d1bff"), "Mon, 21 Oct 2013 20:13:21 GMT"));
}

using Header = std::map<std::string, std::string>;
using Table = std::deque<std::pair<std::string, std::string>>;

struct Tables {
    Table encode_table;
    Table decode_table;
    std::uint32_t max_size_limit = 0;
    std::uint32_t encode_size_limit = 0;
    std::uint32_t decode_size_limit = 0;

    void set_limit(std::uint32_t lim) {
        max_size_limit = lim;
        encode_size_limit = lim;
        decode_size_limit = lim;
    }
};

void check_header_compression_part(Header& encode_header, Tables& table) {
    Header decode_header;
    std::string payload;
    auto on_modify = [](auto&& key, auto&& value, bool insert) {
        ;
    };
    auto err = futils::hpack::encode(payload, table.encode_table, encode_header, table.encode_size_limit, true, on_modify);
    assert(err == futils::hpack::HpackError::none);
    err = futils::hpack::decode<std::string>(payload, table.decode_table, decode_header, table.decode_size_limit, table.max_size_limit, on_modify);
    assert(err == futils::hpack::HpackError::none);
    assert(encode_header == decode_header);
    assert(table.encode_table == table.decode_table);
}

void check_header_compression_resize(std::uint32_t new_size, Tables& table) {
    Header decode_header;
    std::string payload;
    auto on_modify = [](auto&& key, auto&& value, bool insert) {
        ;
    };
    table.encode_size_limit = new_size;
    futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &payload};
    auto err = futils::hpack::encode_table_size_update(w, new_size, table.encode_table, on_modify);
    assert(err == futils::hpack::HpackError::none);
    err = futils::hpack::decode<std::string>(
        payload, table.decode_table, [](auto&&...) {}, table.decode_size_limit, table.max_size_limit, on_modify);
    assert(err == futils::hpack::HpackError::none);
    assert(table.encode_table == table.decode_table);
}

void check_header_compression() {
    Header encode_header;
    Tables tables;
    tables.set_limit(300);
    encode_header[":method"] = "GET";
    encode_header[":path"] = "/";
    encode_header[":authority"] = "www.google.com";
    encode_header[":scheme"] = "https";
    encode_header["accept"] = "*/*";
    encode_header["upgrade"] = "websocket";
    encode_header["unexpected"] = "value";
    check_header_compression_part(encode_header, tables);
    encode_header[":path"] = "/teapot";
    encode_header["upgrade"] = "h3";
    check_header_compression_part(encode_header, tables);
    encode_header.clear();
    encode_header[":method"] = "GET";
    encode_header[":path"] = "/";
    encode_header[":authority"] = "www.google.com";
    encode_header[":scheme"] = "https";
    check_header_compression_part(encode_header, tables);
    check_header_compression_resize(0, tables);
    check_header_compression_resize(200, tables);
    encode_header[":method"] = "GET";
    encode_header[":path"] = "/";
    encode_header[":authority"] = "www.google.com";
    encode_header[":scheme"] = "https";
    for (auto i = 0; i < 20; i++) {
        encode_header["abstract" + futils::number::to_string<std::string>(i)] = "object";
    }
    check_header_compression_part(encode_header, tables);
    auto raw_table = futils::hpack::huffman::decode_tree.place();

    for (size_t i = 0; i < futils::hpack::huffman::decode_tree.place_size(); i++) {
        futils::wrap::cout_wrap() << raw_table[i].raw() << ",\n";
    }

    for (auto& codes : futils::hpack::huffman::codes.codes) {
        futils::wrap::cout_wrap() << "Code{.code = " << codes.code << ", .bits = " << codes.bits << ", .literal =" << codes.literal << "},"
                                  << "\n";
    }

    for (auto& hdr : futils::hpack::predefined_header) {
        futils::wrap::cout_wrap() << "KeyValue{.key = \"" << hdr.first << "\", .value = \"" << hdr.second << "\"},\n";
    }
}

int main() {
    check_string_encoding();
    check_header_compression();
}
