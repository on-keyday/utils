/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <quic/crypto/crypto.h>
#include <helper/readutil.h>
#include <helper/pushbacker.h>
#include <number/parse.h>
#include <number/array.h>
#include <string>
#include <quic/packet/packet_context.h>
#include <quic/packet/read_packet.h>
#include <quic/internal/context_internal.h>
#include <quic/core/core.h>
#include <quic/frame/decode.h>
#include <quic/frame/frame_callback.h>
#include <quic/frame/cast.h>
#include <quic/frame/length.h>
#include <quic/packet/length.h>
#include <quic/packet/write_packet.h>
#include <quic/frame/encode.h>

using namespace utils::quic;
constexpr auto packet_by_client = R"(c000000001088394c8f03e5157080000 449e7b9aec34d1b1c98dd7689fb8ec11
d242b123dc9bd8bab936b47d92ec356c 0bab7df5976d27cd449f63300099f399
1c260ec4c60d17b31f8429157bb35a12 82a643a8d2262cad67500cadb8e7378c
8eb7539ec4d4905fed1bee1fc8aafba1 7c750e2c7ace01e6005f80fcb7df6212
30c83711b39343fa028cea7f7fb5ff89 eac2308249a02252155e2347b63d58c5
457afd84d05dfffdb20392844ae81215 4682e9cf012f9021a6f0be17ddd0c208
4dce25ff9b06cde535d0f920a2db1bf3 62c23e596d11a4f5a6cf3948838a3aec
4e15daf8500a6ef69ec4e3feb6b1d98e 610ac8b7ec3faf6ad760b7bad1db4ba3
485e8a94dc250ae3fdb41ed15fb6a8e5 eba0fc3dd60bc8e30c5c4287e53805db
059ae0648db2f64264ed5e39be2e20d8 2df566da8dd5998ccabdae053060ae6c
7b4378e846d29f37ed7b4ea9ec5d82e7 961b7f25a9323851f681d582363aa5f8
9937f5a67258bf63ad6f1a0b1d96dbd4 faddfcefc5266ba6611722395c906556
be52afe3f565636ad1b17d508b73d874 3eeb524be22b3dcbc2c7468d54119c74
68449a13d8e3b95811a198f3491de3e7 fe942b330407abf82a4ed7c1b311663a
c69890f4157015853d91e923037c227a 33cdd5ec281ca3f79c44546b9d90ca00
f064c99e3dd97911d39fe9c5d0b23a22 9a234cb36186c4819e8b9c5927726632
291d6a418211cc2962e20fe47feb3edf 330f2c603a9d48c0fcb5699dbfe58964
25c5bac4aee82e57a85aaf4e2513e4f0 5796b07ba2ee47d80506f8d2c25e50fd
14de71e6c418559302f939b0e1abd576 f279c4b2e0feb85c1f28ff18f58891ff
ef132eef2fa09346aee33c28eb130ff2 8f5b766953334113211996d20011a198
e3fc433f9f2541010ae17c1bf202580f 6047472fb36857fe843b19f5984009dd
c324044e847a4f4a0ab34f719595de37 252d6235365e9b84392b061085349d73
203a4a13e96f5432ec0fd4a1ee65accd d5e3904df54c1da510b0ff20dcc0c77f
cb2c0e0eb605cb0504db87632cf3d8b4 dae6e705769d1de354270123cb11450e
fc60ac47683d7b8d0f811365565fd98c 4c8eb936bcab8d069fc33bd801b03ade
a2e1fbc5aa463d08ca19896d2bf59a07 1b851e6c239052172f296bfb5e724047
90a2181014f3b94a4e97d117b4381303 68cc39dbb2d198065ae3986547926cd2
162f40a29f0c3c8745c0f50fba3852e5 66d44575c29d39a03f0cda721984b6f4
40591f355e12d439ff150aab7613499d bd49adabc8676eef023b15b65bfc5ca0
6948109f23f350db82123535eb8a7433bdabcb909271a6ecbcb58b936a88cd4e
8f2e6ff5800175f113253d8fa9ca8885 c2f552e657dc603f252e1a8e308f76f0
be79e2fb8f5d5fbbe2e30ecadd220723 c8c0aea8078cdfcb3868263ff8f09400
54da48781893a7e49ad5aff4af300cd8 04a6b6279ab3ff3afb64491c85194aab
760d58a606654f9f4400e8b38591356f bf6425aca26dc85244259ff2b19c41b9
f96f3ca9ec1dde434da7d2d392b905dd f3d1f9af93d1af5950bd493f5aa731b4
056df31bd267b6b90a079831aaf579be 0a39013137aac6d404f518cfd4684064
7e78bfe706ca4cf5e9c5453e9f7cfd2b 8b4c8d169a44e55c88d4a9a7f9474241
e221af44860018ab0856972e194cd934)";
std::string make_packet_raw(const char* packet) {
    using namespace utils;
    std::string str, data;
    auto seq = make_ref_seq(packet);
    helper::append_if(str, helper::nop, seq, [](auto c) {
        return c != ' ' && c != '\n';
    });
    auto rseq = make_ref_seq(str);
    assert(rseq.size() % 2 == 0);
    auto b = rseq.size() / 2;
    for (size_t i = 0; i < b; i++) {
        number::Array<2, char> a{rseq.current(), rseq.current(1)};
        a.i = 2;
        byte b;
        auto ok = number::parse_integer(a, b, 16);
        assert(ok);
        rseq.consume(2);
        data.push_back(b);
    }
    return data;
}

void test_read_packet() {
    auto d = make_packet_raw(packet_by_client);
    tsize offset = 0;
    packet::RWContext c;
    c.q = core::default_QUIC();
    auto nc = conn::new_connection(c.q, server);
    c.cb.initial = [&](packet::RWContext* c, packet::Packet* p) {
        crypto::decrypt_packet_protection(client, c->q->g->bpool, p);
        tsize offset = 0;
        frame::RWContext ctx;
        ctx.b = &c->q->g->bpool;
        frame::Crypto data;
        ctx.frame_cb = [&](frame::Frame* f) {
            frame::if_(f, [&](frame::Crypto* c) {
                data = std::move(*c);
            });
        };
        ctx.frame_error_cb = [] {
            assert(false);
        };
        while (offset < p->decrypted_length) {
            auto e = frame::read_frame(p->decrypted_payload.c_str(), p->decrypted_length, &offset, ctx);
            if (e != frame::Error::none) {
            }
            if (ctx.prev_type == frame::types::PADDING) {
                frame::read_paddings(p->decrypted_payload.c_str(), p->decrypted_length, &offset);
            }
        }

        crypto::advance_handshake(data, nc->self, crypto::EncryptionLevel::initial);
        ctx.b->put(std::move(data.crypto_data));
    };

    byte* data = (byte*)d.data();
    packet::read_packet(data, d.size(), &offset, c);
    core::del_QUIC(c.q);
}

void test_write_packet() {
    // initial packet
    packet::InitialPacket ini{};
    constexpr auto initial = packet::make_fb(packet::types::Initial, 1, 0);
    byte nul = 0, src[] = "client hel", tok[] = "variable_token";
    //                                   need to encode
    ini.flags = initial;              // 1
    ini.version = 1;                  // 4
    ini.dstID_len = 0;                // 1
    ini.dstID = &nul;                 // 0
    ini.srcID_len = 10;               // 1
    ini.srcID = src;                  // 10
    ini.token_len = sizeof(tok) - 1;  // 1
    ini.token = tok;                  // 13
    ini.payload_length = 1200 - 16;   // 2 (temporary)
    ini.packet_number = 0;            // 1
    //                                   total: 35

    auto q = core::default_QUIC();
    auto c = conn::new_connection(q, client);
    tpparam::DefinedParams params = tpparam::defaults;
    params.max_udp_payload_size = 1500;
    crypto::set_alpn(c, "\2h3", 3);
    crypto::set_hostname(c, "www.google.com");
    crypto::set_transport_parameter(c, &params);
    auto cframe = frame::frame<frame::types::CRYPTO>();
    crypto::start_handshake(c, [&](const byte* data, tsize size, bool err) {
        if (!err) {
            bytes::append(cframe.crypto_data, &cframe.length, &q->g->alloc, data, size);
        }
    });
    tsize header_len = packet::length(&ini);
    tsize payload_len = frame::length(cframe, true);
    ini.payload_length = payload_len + ini.flags.packet_number_length();
    tsize padding_len = 1200 - 16 - header_len - payload_len;
    bytes::Buffer b{};
    b.a = &q->g->alloc;
    packet::RWContext rp;
    frame::RWContext rf;
    packet::write_packet(b, &ini, rp);
    frame::write_frame(b, &cframe, rf);
    frame::write_padding(b, padding_len, rf);
}

int main() {
    constexpr auto ossl_libssl = "D:/quictls/openssl+quic/debug/openssl/bin/libssl-81_3-x64.dll";
    constexpr auto ossl_libcrypto = "D:/quictls/openssl+quic/debug/openssl/bin/libcrypto-81_3-x64.dll";
    constexpr auto bssl_libssl = "D:/quictls/boringssl/built/lib/ssl.dll";
    constexpr auto bssl_libcrypto = "D:/quictls/boringssl/built/lib/crypto.dll";
    external::set_libcrypto_location(bssl_libcrypto);
    external::set_libssl_location(bssl_libssl);
    external::load_LibCrypto();
    external::load_LibSSL();

    test_read_packet();
    test_write_packet();
}
