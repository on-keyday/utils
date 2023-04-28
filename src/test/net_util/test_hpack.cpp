/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net_util/hpack/hpack_encode.h>
#include <string>
#include <fnet/quic/crypto/keys.h>
#include <number/array.h>
#include <view/expand_iovec.h>
#include <memory>
using string = utils::number::Array<char, 100>;
using rvec = utils::view::basic_rvec<char, utils::byte>;

constexpr bool encode_decode(const char* base) {
    auto in = rvec(base);
    utils::io::basic_bit_writer<string, char, utils::byte> w{};
    utils::hpack::encode_huffman(w, in);
    auto encoded = w.get_base().written();
    utils::io::basic_bit_reader<rvec, char, utils::byte> d{encoded};
    string val;
    return utils::hpack::decode_huffman(val, d) && val == in;
}

template <size_t s>
constexpr bool decode(utils::fnet::quic::crypto::KeyMaterial<s> data, const char* expect) {
    auto v = utils::view::wvec(data.material);
    utils::io::bit_reader<utils::view::wvec> d{v};
    string val;
    return utils::hpack::decode_huffman(val, d) && val == rvec(expect);
}

int main() {
    static_assert(encode_decode("object identifier z"));
    static_assert(encode_decode("\x32\x44\xff"));
    bool d = encode_decode("object identifierz");
    static_assert(decode(utils::fnet::quic::crypto::make_material_from_bintext("d07abe941054d444a8200595040b8166e082a62d1bff"), "Mon, 21 Oct 2013 20:13:21 GMT"));
}
