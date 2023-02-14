/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include <net_util/hpack_huffman.h>
#include <string>
#include <dnet/quic/crypto/keys.h>

constexpr bool encode_decode(const char* base) {
    std::string str = base;
    utils::hpack::bitvec_writer<std::string> w;
    utils::hpack::encode_huffman(w, str);
    auto encoded = w.data();
    std::string val;
    utils::hpack::bitvec_reader<std::string&> d{encoded};
    return utils::hpack::decode_huffman(val, d) && val == str;
}

template <size_t s>
constexpr bool decode(utils::dnet::quic::crypto::Key<s> data, const char* expect) {
    auto v = utils::view::wvec(data.key);
    utils::hpack::bitvec_reader<utils::view::wvec> d{v};
    std::string val;
    return utils::hpack::decode_huffman(val, d) && val == expect;
}

int main() {
    static_assert(encode_decode("object identifier z"));
    static_assert(encode_decode("\x32\x44\xff"));
    encode_decode("object identifierz");
    static_assert(decode(utils::dnet::quic::crypto::make_key_from_bintext("d07abe941054d444a8200595040b8166e082a62d1bff"),
                         "Mon, 21 Oct 2013 20:13:21 GMT"));
}
