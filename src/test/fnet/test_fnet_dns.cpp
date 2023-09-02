/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <fnet/dns/dns.h>
#include <fnet/socket.h>
#include <fnet/addrinfo.h>
#include <fnet/dns/records.h>
#include <binary/expandable_writer.h>
#include <vector>
#include <string>
using namespace utils;

int main() {
    fnet::Socket sock = fnet::make_socket(fnet::sockattr_udp(fnet::ip::Version::ipv4)).value();
    assert(sock);
    auto addr = fnet::ipv4(1, 1, 1, 1, 53);
    namespace dns = fnet::dns;
    dns::Message<std::vector> msg;
    msg.header.set_op_code(dns::OpCode::query);
    msg.header.id = 0x3f93;
    msg.header.set_response(false);
    msg.header.set_recursion_desired(true);
    dns::Query query;
    query.name.name = "google.com";
    query.type = dns::RecordType::AAAA;
    query.dns_class = dns::DNSClass::IN;
    msg.query.push_back(std::move(query));
    query.name.name = "google.com";
    query.type = dns::RecordType::CNAME;
    query.dns_class = dns::DNSClass::IN;
    msg.query.push_back(std::move(query));
    dns::Record rec;
    dns::EDNSRecord edns;
    edns.mtu = 1200;
    edns.pack(rec);
    msg.additional.push_back(std::move(rec));
    byte buf[1200]{};
    binary::writer w{buf};
    auto res = msg.render(w);
    assert(res);
    binary::reader r{w.written()};
    res = msg.parse(r);
    assert(res);
    sock.writeto(addr, w.written()).value();
    fnet::BufferManager<view::wvec> d{
        view::wvec(buf),
    };
    bool end = false;
    auto result = sock.readfrom_async(d, [&](fnet::Socket&& s, fnet::BufferManager<view::wvec>& w, fnet::NetAddrPort&& addr, fnet::NotifyResult result) {
                          view::wvec data;
                          std::tie(data, addr) = result.readfrom_unwrap(w.buffer, addr, [&] { return s.readfrom(w.buffer); }).value();
                          utils::binary::reader r{data};
                          res = msg.parse(r);
                          assert(res);
                          assert(msg.answer.size());
                          std::string resolv;
                          res = dns::resolve_name(resolv, msg.answer[0].name, data, true);
                          assert(res);
                          end = true;
                      })
                      .value();
    if (result.state == fnet::NotifyState::done) {
        return 0;  // done
    }
    while (!end) {
        fnet::wait_io_event(10000);
        if (!end) {
            result.cancel.cancel();
        }
    }
}
