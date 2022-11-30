/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/tls.h>
#include <cassert>
#include <dnet/quic/transport_parameter/transport_param.h>
#include <string>
#include <random>
#include <dnet/quic/frame/frame.h>
#include <dnet/quic/frame/frame_util.h>
#include <dnet/quic/packet/packet.h>
#include <dnet/socket.h>
#include <dnet/plthead.h>
#include <dnet/addrinfo.h>
#include <dnet/quic/frame/vframe.h>
#include <dnet/quic/packet/packet_util.h>
#include <dnet/quic/frame/frame_make.h>
#include <dnet/quic/crypto/crypto.h>

using namespace utils;
using ubytes = std::basic_string<dnet::byte>;

struct Test {
    ubytes handshake_data;
    bool flush;
    dnet::byte alert;
    dnet::quic::crypto::EncryptionLevel wlevel = {};
    dnet::quic::crypto::EncryptionLevel rlevel = {};
    ubytes wsecret;
    dnet::TLSCipher wcipher;
    ubytes rsecret;
    dnet::TLSCipher rcipher;
};

int quic_method(Test* t, dnet::quic::MethodArgs args) {
    if (args.type == dnet::quic::crypto::ArgType::handshake_data) {
        t->handshake_data.append(args.data, args.len);
    }
    if (args.type == dnet::quic::crypto::ArgType::wsecret) {
        t->wsecret.append(args.write_secret, args.secret_len);
        t->wlevel = args.level;
        t->wcipher = args.cipher;
    }
    if (args.type == dnet::quic::crypto::ArgType::rsecret) {
        t->rsecret.append(args.read_secret, args.secret_len);
        t->rlevel = args.level;
        t->rcipher = args.cipher;
    }
    if (args.type == dnet::quic::crypto::ArgType::flush) {
        t->flush = true;
    }
    if (args.type == dnet::quic::crypto::ArgType::alert) {
        t->alert = args.alert;
    }
    return 1;
}

ubytes genrandom(size_t len) {
    ubytes val;
    auto dev = std::random_device();
    std::uniform_int_distribution<> uni(0, 0xff);
    for (size_t i = 0; i < len; i++) {
        val.push_back(dnet::byte(uni(dev)));
    }
    return val;
}

using ByteLen = dnet::ByteLen;

int errors(const char* msg, size_t len, void*) {
    return 1;
}

void test_dnetquic_initial() {
    dnet::TLS tls = dnet::create_tls();
    assert(tls);
    tls.set_cacert_file(nullptr, "D:/MiniTools/QUIC_mock/goserver/keys/");
    tls.set_verify(dnet::SSL_VERIFY_PEER_);
    Test test;
    auto res = tls.make_quic(quic_method, &test).is_noerr();
    assert(res);
    tls.set_alpn("\x02h3", 3);
    dnet::quic::trsparam::DefinedTransportParams params{};
    dnet::WPacket srcw, dstw;
    dnet::byte src[3000]{}, data[5000]{};
    srcw.b.data = src;
    srcw.b.len = sizeof(src);
    auto srcID = genrandom(20);
    params.initial_src_connection_id = {srcID.data(), 20};
    auto list = params.to_transport_param(srcw);
    dstw.b.data = data;
    dstw.b.len = sizeof(data);
    for (auto& param : list) {
        auto pid = dnet::quic::trsparam::DefinedTransportParamID(param.first.id.value);
        if (!dnet::quic::trsparam::is_defined_both_set_allowed(int(pid)) || !param.second) {
            continue;
        }
        res = param.first.render(dstw);
        assert(res);
    }
    res = tls.set_quic_transport_params(dstw.b.data, dstw.offset).is_noerr();
    assert(res);
    auto block = tls.connect();
    assert(dnet::isTLSBlock(block));
    dstw.offset = 0;
    srcw.offset = 0;
    auto dstID = genrandom(20);
    auto crypto = dnet::quic::frame::make_crypto(nullptr, 0, {test.handshake_data.data(), test.handshake_data.size()});
    auto payload = dnet::WPacket{srcw.acquire(crypto.render_len())};
    crypto.render(payload);
    auto packet = dnet::quic::packet::make_initial_plain(
        {0, 1}, 1, {dstID.data(), 20}, {srcID.data(), 20},
        {}, payload.b, 16, 1200);
    auto info = dnet::quic::packet::make_cryptoinfo_from_plain_packet(dstw, packet.first, 16);
    auto secret = dnet::quic::crypto::make_initial_secret(srcw, 1, packet.first.dstID, true);
    dnet::quic::crypto::Keys keys;
    res = dnet::quic::crypto::encrypt_packet(keys, 1, {}, info, secret).is_noerr();

    assert(res);
    auto packet_ = info.composition();

    dnet::SockAddr addr{};
    addr.hostname = "localhost";
    addr.namelen = 9;
    addr.type = SOCK_DGRAM;
    auto wait = dnet::resolve_address(addr, "8090");
    auto resolved = wait.wait();
    assert(!wait.failed());
    resolved.next();
    resolved.sockaddr(addr);
    auto sock = dnet::make_socket(addr.af, addr.type, addr.proto);
    assert(sock);
    dnet::error::Error err;
    std::tie(std::ignore, err) = sock.writeto(addr.addr, addr.addrlen, packet_.data, packet_.len);
    assert(!err);
    ::sockaddr_storage storage;
    int addrlen = sizeof(storage);
    while (true) {
        if (auto [readsize, err] = sock.readfrom(reinterpret_cast<dnet::raw_address*>(&storage), &addrlen, dstw.b.data, dstw.b.len); !err) {
            dstw.offset = readsize;
            break;
        }
        else if (dnet::isSysBlock(err)) {
            assert(false);
        }
    }
    auto from_server = dstw.b.resized(dstw.offset);
    auto cur = tls.get_cipher();
    auto valid_v1 = [](std::uint32_t version, ByteLen, ByteLen) {
        return version == 1;
    };
    /*
    while (from_server.len) {
        auto copy = from_server;
        auto pack = dnet::quic::PacketFlags{from_server.data};
        if (pack.type() == dnet::quic::PacketType::Initial) {
            auto clientDstID = ByteLen{dstID.data(), dstID.size()};
            auto pinfo = dnet::quic::flow::parseInitialPartial(copy, clientDstID, valid_v1);
            assert(pinfo.payload.valid());
            secret = dnet::quic::crypto::make_initial_secret(srcw, secret_key, 1, clientDstID, false);
            res = dnet::quic::crypto::decrypt_packet(keys, 1, {}, info, secret);
            assert(res);
            auto payload = pinfo.processed_payload;
            res = dnet::quic::frame::parse_frames(payload, [&](dnet::quic::frame::Frame& frame, bool err) {
                assert(!err);
                if (frame.type.type() == dnet::quic::FrameType::ACK) {
                    ;
                }
                if (auto c = dnet::quic::frame::frame_cast<dnet::quic::frame::CryptoFrame>(&frame)) {
                    auto& crypto = *c;
                    res = tls.provide_quic_data(
                        dnet::quic::EncryptionLevel::initial,
                        crypto.crypto_data.data, crypto.crypto_data.len);
                    assert(res);
                    test.handshake_data = {};
                    res = tls.connect();
                    assert(tls.block());
                }
            });
            assert(res);
            from_server = from_server.forward(pinfo.head.len + pinfo.payload.len);
        }
        if (pack.type() == dnet::quic::PacketType::HandShake) {
            auto cipher = tls.get_cipher();
            auto info = dnet::quic::flow::parseHandshakePartial(copy, valid_v1);
            dnet::quic::Keys keys;
            res = dnet::quic::decrypt_packet(keys, 1, cipher, info, {test.rsecret.data(), test.rsecret.size()});
            assert(res);
            auto payload = info.processed_payload;
            std::vector<dnet::quic::frame::Frame*> vec;
            dnet::quic::FrameType fty{};
            srcw.offset = 0;
            res = dnet::quic::frame::parse_frames(payload, dnet::quic::frame::wpacket_frame_vec(srcw, vec, &fty));
            assert(res);
            for (auto& fr : vec) {
                if (auto c = dnet::quic::frame::frame_cast<dnet::quic::frame::CryptoFrame>(fr)) {
                    tls.provide_quic_data(dnet::quic::EncryptionLevel::handshake, c->crypto_data.data, c->crypto_data.len);
                    tls.clear_err();
                    res = tls.connect();
                    tls.get_errors(errors, nullptr);
                    assert(res || tls.block());
                }
            }
            from_server = from_server.forward(info.head.len + info.payload.len);
        }
        if (pack.type() == dnet::quic::PacketType::OneRTT) {
            break;
        }
    }*/
}

int main() {
    dnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    dnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    test_dnetquic_initial();
}
