/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/tls.h>
#include <cassert>
#include <dnet/quic/transport_param.h>
#include <string>
#include <random>
#include <dnet/quic/frame.h>
#include <dnet/quic/frame_util.h>
#include <dnet/quic/packet.h>
#include <dnet/socket.h>
#include <dnet/plthead.h>
#include <dnet/addrinfo.h>
#include <dnet/quic/flow/initial.h>

using namespace utils;
struct Test {
    dnet::String data;
    bool flush;
    dnet::quic::byte alert;
};

int quic_method(Test* t, dnet::quic::MethodArgs args) {
    if (args.type == dnet::quic::ArgType::handshake_data) {
        t->data.append((const char*)args.data, args.len);
    }
    if (args.type == dnet::quic::ArgType::flush) {
        t->flush = true;
    }
    if (args.type == dnet::quic::ArgType::alert) {
        t->alert = args.alert;
    }
    return 1;
}

using ubytes = std::basic_string<dnet::quic::byte>;

ubytes genrandom(size_t len) {
    ubytes val;
    auto dev = std::random_device();
    std::uniform_int_distribution<> uni(0, 0xff);
    for (size_t i = 0; i < len; i++) {
        val.push_back(dnet::quic::byte(uni(dev)));
    }
    return val;
}

using ByteLen = dnet::quic::ByteLen;

void test_dnetquic_initial() {
    dnet::TLS tls = dnet::create_tls();
    assert(tls);
    Test test;
    auto res = tls.make_quic(quic_method, &test);
    assert(res);
    tls.set_alpn("\x02h3", 3);
    dnet::quic::DefinedTransportParams params{};
    dnet::quic::WPacket srcw, dstw;
    dnet::quic::byte src[3000]{}, data[3000]{};
    srcw.b.data = src;
    srcw.b.len = sizeof(src);
    auto srcID = genrandom(20);
    params.initial_src_connection_id = {srcID.data(), 20};
    auto list = params.to_transport_param(srcw);
    dstw.b.data = data;
    dstw.b.len = sizeof(data);
    for (auto& param : list) {
        auto pid = dnet::quic::DefinedTransportParamID(param.id.varint());
        if (!dnet::quic::is_defined_both_set_allowed(int(pid)) ||
            !param.data.valid()) {
            continue;
        }
        res = param.render(dstw);
        assert(res);
    }
    res = tls.set_quic_transport_params(dstw.b.data, dstw.offset);
    assert(res);
    tls.connect();
    assert(tls.block());
    dstw.offset = 0;
    srcw.offset = 0;

    auto dstID = genrandom(20);

    auto packet = dnet::quic::flow::make_first_flight(
        srcw,
        0, 1,
        {(dnet::quic::byte*)test.data.text(), test.data.size()},
        {dstID.data(), 20},
        {srcID.data(), 20},
        srcw.acquire(0));

    auto info = dnet::quic::flow::makePacketInfo(dstw, packet, true);

    res = dnet::quic::encrypt_initial_packet(info, true);

    assert(res);

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
    res = sock.writeto(addr.addr, addr.addrlen, dstw.b.data, dstw.offset);
    assert(res);
    ::sockaddr_storage storage;
    int addrlen = sizeof(storage);
    while (true) {
        if (sock.readfrom(&storage, &addrlen, dstw.b.data, dstw.b.len)) {
            dstw.offset = sock.readsize();
            break;
        }
        if (!sock.block()) {
            assert(false);
        }
    }
    auto from_server = dstw.b.resized(dstw.offset);
    auto copy = from_server;
    auto pack = dnet::quic::PacketFlags{from_server.data};
    if (pack.type() == dnet::quic::PacketType::Initial) {
        auto pinfo = dnet::quic::flow::parseInitialPartial(copy, {dstID.data(), dstID.size()},
                                                           [](std::uint32_t version) {
                                                               return version == 1;
                                                           });
        assert(pinfo.payload.valid());
        dnet::String str;
        str.resize(pinfo.payload.len - 16);
        pinfo.processed_payload = {(unsigned char*)str.text(), str.size()};
        res = dnet::quic::decrypt_initial_packet(pinfo, false);
        assert(res);
        auto payload = pinfo.processed_payload;
        dnet::quic::parse_frames(payload, [&](dnet::quic::Frame& frame) {
            if (frame.type.type() == dnet::quic::FrameType::ACK) {
            }
            if (frame.type.type() == dnet::quic::FrameType::CRYPTO) {
            }
        });
    }
}

int main() {
    dnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    dnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    test_dnetquic_initial();
}
