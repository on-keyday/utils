/*
    utils - utility library
    Copyright (c) 2021-2023 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/


#include <dnet/dns/dns.h>
#include <dnet/socket.h>
#include <dnet/addrinfo.h>
#include <dnet/dns/records.h>
#include <io/expandable_writer.h>
#include <vector>
#include <string>
using namespace utils;

int main() {
    dnet::Socket sock = dnet::make_socket(dnet::sockattr_udp(4));
    assert(sock);
    auto addr = dnet::ipv4(1, 1, 1, 1, 53);
    namespace dns = dnet::dns;
    dns::Message<std::vector> msg;
    msg.header.flag.set_op_code(dns::OpCode::query);
    msg.header.id = 0x3f93;
    msg.header.flag.set_response(false);
    msg.header.flag.set_recursion_desired(true);
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
    io::writer w{buf};
    auto res = msg.render(w);
    assert(res);
    io::reader r{w.written()};
    res = msg.parse(r);
    assert(res);
    auto [_, e] = sock.writeto(addr, w.written());
    assert(!e);
    bool end = false;
    auto [cancel, err] = sock.readfrom_async(view::wvec(buf), [&](dnet::Socket&& s, view::wvec data, view::wvec buf, dnet::NetAddrPort addr, dnet::error::Error err, void*) {
        r.reset(data);
        res = msg.parse(r);
        assert(res);
        assert(msg.answer.size());
        std::string resolv;
        res = dns::resolve_name(resolv, msg.answer[0].name, data, true);
        assert(res);
        end = true;
    });
    assert(!err);
    while (!end) {
        dnet::wait_io_event(10000);
        cancel.cancel();
    }
}
