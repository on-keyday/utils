/*
    utils - utility library
    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

#include "durl.h"
#include <dnet/conn.h>
#include <dnet/plthead.h>
#include <thread>

namespace durl {
    namespace dnet = utils::dnet;
    struct AddrWait {
        URIToJSON uri;
        dnet::WaitAddrInfo wait;
    };

    std::atomic_uint32_t reqcounter;

    struct Context {
        std::atomic_uint32_t reqcounter;
    };

    void request_message(URI& uri, auto&&... msg) {}

    struct Request {
        URI uri;
    };

    void wait_connect(Request req, dnet::AddrInfo& info, dnet::Socket& conn) {
        while (true) {
            if (auto err = conn.wait_writable(0, 1000)) {
                if (!dnet::isBlock(err)) {
                    request_message(req.uri, "failed to connect to ", info.string(), ". last error is ", err.error<std::string>());
                    return;
                }
                continue;
            }
            break;
        }
    }

    // on thread
    void resolved(Request& req, dnet::AddrInfo& info) {
        dnet::error::Error lasterr;
        while (info.next()) {
            dnet::SockAddr addr;
            info.sockaddr(addr);
            auto sock = dnet::make_socket(addr.af, addr.type, addr.proto);
            if (!sock) {
                lasterr = dnet::error::Error("make_socket failed");
                continue;
            }
            auto err = sock.connect(addr.addr, addr.addrlen);
            if (err && !dnet::isBlock(err)) {
                lasterr = std::move(err);
                continue;
            }
        }
        info.next();  // seek to top
        request_message(req.uri, "failed to connect to ", info.string(), ". last error is ", lasterr.error<std::string>());
        reqcounter--;
    }

    void request(Request req) {
        auto& uri = req.uri;
        uri::tidy(uri);
        if (uri.scheme == "") {
            uri.scheme = httpopt.uri.https_parse_prefix ? "https" : "http";
        }
        if (uri.scheme != "http" && uri.scheme != "https") {
            request_message(uri, "unexpected scheme ", uri.scheme, ". expect https or http");
            reqcounter--;
            return;
        }
        if (uri.port == "") {
            if (uri.scheme == "http") {
                uri.port = "80";
            }
            else if (uri.scheme == "https") {
                uri.port = "443";
            }
        }
        dnet::SockAddr addr{};
        addr.hostname = uri.hostname.c_str();
        addr.namelen = uri.hostname.size();
        addr.type = SOCK_STREAM;
        auto wait = dnet::resolve_address(addr, uri.port.c_str());
        int err = 0;
        if (wait.failed(&err)) {
            request_message(uri, "resolve address failed at code ", err);
            reqcounter--;
            return;
        }
        dnet::AddrInfo info;
        if (wait.wait(info, 1)) {
            std::thread(
                [](dnet::AddrInfo info, Request req) {
                    resolved(req, info);
                },
                std::move(info), std::move(req))
                .detach();
            return;
        }
        if (wait.failed(&err)) {
            request_message(uri, "resolve address failed at code ", err);
            reqcounter--;
            return;
        }
        std::thread(
            [=](dnet::WaitAddrInfo wait, Request req) {
                dnet::AddrInfo info;
                while (true) {
                    if (wait.wait(info, 1)) {
                        resolved(req, info);
                        return;
                    }
                    int err = 0;
                    if (wait.failed(&err)) {
                        request_message(req.uri, "resolve address failed at code ", err);
                        reqcounter--;
                        return;
                    }
                }
            },
            std::move(wait), std::move(req))
            .detach();
    }

    int do_request(URIVec& uris) {
        for (auto& uri : uris) {
            reqcounter++;
            request(Request{std::move(uri.uri)});
        }
        return 0;
    }
}  // namespace durl
