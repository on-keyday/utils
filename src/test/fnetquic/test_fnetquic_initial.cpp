/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

/*
#include <fnet/tls/tls.h>
#include <cassert>
#include <fnet/quic/transport_parameter/transport_param.h>
#include <string>
#include <random>
#include <fnet/quic/old/frame_old/frame.h>
#include <fnet/quic/old/frame_old/frame_util.h>
#include <fnet/quic/old/packet_old/packet.h>
#include <fnet/socket.h>
#include <fnet/plthead.h>
#include <fnet/addrinfo.h>
#include <fnet/quic/old/frame_old/vframe.h>
#include <fnet/quic/old/packet_old/packet_util.h>
#include <fnet/quic/old/frame_old/frame_make.h>
#include <fnet/quic/crypto/crypto.h>

using namespace utils;
using ubytes = std::basic_string<byte>;

struct Test {
   ubytes handshake_data;
   bool flush;
   byte alert;
   fnet::quic::crypto::EncryptionLevel wlevel = {};
   fnet::quic::crypto::EncryptionLevel rlevel = {};
   ubytes wsecret;
   fnet::TLSCipher wcipher;
   ubytes rsecret;
   fnet::TLSCipher rcipher;
};

int quic_method(Test* t, fnet::quic::MethodArgs args) {
   if (args.type == fnet::quic::crypto::ArgType::handshake_data) {
       t->handshake_data.append(args.data, args.len);
   }
   if (args.type == fnet::quic::crypto::ArgType::wsecret) {
       t->wsecret.append(args.write_secret, args.secret_len);
       t->wlevel = args.level;
       t->wcipher = args.cipher;
   }
   if (args.type == fnet::quic::crypto::ArgType::rsecret) {
       t->rsecret.append(args.read_secret, args.secret_len);
       t->rlevel = args.level;
       t->rcipher = args.cipher;
   }
   if (args.type == fnet::quic::crypto::ArgType::flush) {
       t->flush = true;
   }
   if (args.type == fnet::quic::crypto::ArgType::alert) {
       t->alert = args.alert;
   }
   return 1;
}

ubytes genrandom(size_t len) {
   ubytes val;
   auto dev = std::random_device();
   std::uniform_int_distribution<> uni(0, 0xff);
   for (size_t i = 0; i < len; i++) {
       val.push_back(byte(uni(dev)));
   }
   return val;
}

using ByteLen = fnet::ByteLen;

int errors(const char* msg, size_t len, void*) {
   return 1;
}

void test_fnetquic_initial() {

   fnet::TLS tls = fnet::create_tls();
   assert(tls);
   tls.set_cacert_file(nullptr, "D:/MiniTools/QUIC_mock/goserver/keys/");
   tls.set_verify(fnet::SSL_VERIFY_PEER_);
   Test test;
   auto res = tls.setup_quic(quic_method, &test).is_noerr();
   assert(res);
   tls.set_alpn("\x02h3", 3);
   fnet::quic::trsparam::DefinedTransportParams params{};
   fnet::WPacket srcw, dstw;
   byte src[3000]{}, data[5000]{};
   srcw.b.data = src;
   srcw.b.len = sizeof(src);
   auto srcID = genrandom(20);
   params.initial_src_connection_id = {srcID.data(), 20};
   auto list = params.to_transport_param(srcw);
   dstw.b.data = data;
   dstw.b.len = sizeof(data);
   for (auto& param : list) {
       auto pid = fnet::quic::trsparam::DefinedTransportParamID(param.first.id.value);
       if (!fnet::quic::trsparam::is_defined_both_set_allowed(int(pid)) || !param.second) {
           continue;
       }
       res = param.first.render(dstw);
       assert(res);
   }
   res = tls.set_quic_transport_params(dstw.b.data, dstw.offset).is_noerr();
   assert(res);
   auto block = tls.connect();
   assert(fnet::isTLSBlock(block));
   dstw.offset = 0;
   srcw.offset = 0;
   auto dstID = genrandom(20);
   auto crypto = fnet::quic::frame::make_crypto(nullptr, 0, {test.handshake_data.data(), test.handshake_data.size()});
   auto payload = fnet::WPacket{srcw.acquire(crypto.render_len())};
   crypto.render(payload);
   auto packet = fnet::quic::packet::make_initial_plain(
       {0, 1}, 1, {dstID.data(), 20}, {srcID.data(), 20},
       {}, payload.b, 16, 1200);
   auto info = fnet::quic::packet::make_cryptoinfo_from_plain_packet(dstw, packet.first, 16);
   auto secret = srcw.acquire(32);
   res = fnet::quic::crypto::make_initial_secret(view::wvec(secret.data, secret.len), 1, view::rvec(packet.first.dstID.data, packet.first.dstID.len), true);
   assert(res);
   fnet::quic::crypto::Keys keys;
   res = fnet::quic::crypto::make_keys_from_secret(keys, {}, 1, view::rvec(secret.data, secret.len)).is_noerr();
   assert(res);
   auto comp = info.composition();
   fnet::quic::packet::CryptoPacket parsed;
   parsed.src = view::wvec(comp.data, comp.len);
   parsed.head_len = info.head.len;
   res = fnet::quic::crypto::encrypt_packet(keys, {}, parsed).is_noerr();

   assert(res);
   auto packet_ = info.composition();

   auto wait = fnet::resolve_address("localhost", "8090", {.socket_type = SOCK_DGRAM});
   auto resolved = wait.wait();
   assert(!wait.failed());
   resolved.next();
   auto addr = resolved.sockaddr();

   auto sock = fnet::make_socket(addr.attr.address_family, addr.attr.socket_type, addr.attr.protocol);
   assert(sock);
   fnet::error::Error err;
   std::tie(std::ignore, err) = sock.writeto(addr.addr, view::rvec(packet_.data, packet_.len));
   assert(!err);
   ::sockaddr_storage storage;
   int addrlen = sizeof(storage);
   while (true) {
       if (auto [read, addr, err] = sock.readfrom(view::wvec(dstw.b.data, dstw.b.len)); !err) {
           dstw.offset = read.size();
           break;
       }
       else if (fnet::isSysBlock(err)) {
           assert(false);
       }
   }
   auto from_server = dstw.b.resized(dstw.offset);
   auto cur = tls.get_cipher();
   auto valid_v1 = [](std::uint32_t version, ByteLen, ByteLen) {
       return version == 1;
   };

   while (from_server.len) {
       auto copy = from_server;
       auto pack = fnet::quic::PacketFlags{from_server.data};
       if (pack.type() == fnet::quic::PacketType::Initial) {
           auto clientDstID = ByteLen{dstID.data(), dstID.size()};
           auto pinfo = fnet::quic::flow::parseInitialPartial(copy, clientDstID, valid_v1);
           assert(pinfo.payload.valid());
           secret = fnet::quic::crypto::make_initial_secret(srcw, secret_key, 1, clientDstID, false);
           res = fnet::quic::crypto::decrypt_packet(keys, 1, {}, info, secret);
           assert(res);
           auto payload = pinfo.processed_payload;
           res = fnet::quic::frame::parse_frames(payload, [&](fnet::quic::frame::Frame& frame, bool err) {
               assert(!err);
               if (frame.type.type() == fnet::quic::FrameType::ACK) {
                   ;
               }
               if (auto c = fnet::quic::frame::frame_cast<fnet::quic::frame::CryptoFrame>(&frame)) {
                   auto& crypto = *c;
                   res = tls.provide_quic_data(
                       fnet::quic::EncryptionLevel::initial,
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
       if (pack.type() == fnet::quic::PacketType::HandShake) {
           auto cipher = tls.get_cipher();
           auto info = fnet::quic::flow::parseHandshakePartial(copy, valid_v1);
           fnet::quic::Keys keys;
           res = fnet::quic::decrypt_packet(keys, 1, cipher, info, {test.rsecret.data(), test.rsecret.size()});
           assert(res);
           auto payload = info.processed_payload;
           std::vector<fnet::quic::frame::Frame*> vec;
           fnet::quic::FrameType fty{};
           srcw.offset = 0;
           res = fnet::quic::frame::parse_frames(payload, fnet::quic::frame::wpacket_frame_vec(srcw, vec, &fty));
           assert(res);
           for (auto& fr : vec) {
               if (auto c = fnet::quic::frame::frame_cast<fnet::quic::frame::CryptoFrame>(fr)) {
                   tls.provide_quic_data(fnet::quic::EncryptionLevel::handshake, c->crypto_data.data, c->crypto_data.len);
                   tls.clear_err();
                   res = tls.connect();
                   tls.get_errors(errors, nullptr);
                   assert(res || tls.block());
               }
           }
           from_server = from_server.forward(info.head.len + info.payload.len);
       }
       if (pack.type() == fnet::quic::PacketType::OneRTT) {
           break;
       }
   }
}*/

int main() {
    /*
    fnet::set_libcrypto("D:/quictls/boringssl/built/lib/crypto.dll");
    fnet::set_libssl("D:/quictls/boringssl/built/lib/ssl.dll");
    test_fnetquic_initial();*/
}
